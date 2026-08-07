#include "win_compat.h"

#include "Game.h"
#include "config.h"
#include "LoadingScreen.h"
#include "SteamManager.h"
#include "ModelLoader.h"
#include "TextRenderer.h"
#include "Skybox.h"
#include "CharacterAnimationController.h"
#include "constants/file.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <vector>


Game::Game() : m_argc(0), m_argv(nullptr) {
    if (!glfwInit()) {
        throw std::runtime_error("Impossible d'initialiser GLFW");
    }
    initialize();
}

Game::Game(int argc, char* argv[]) : m_argc(argc), m_argv(argv) {
    if (!glfwInit()) {
        throw std::runtime_error("Impossible d'initialiser GLFW");
    }
    initialize();
}

Game::~Game() {
    if (m_loadingThread.joinable()) m_loadingThread.join();

    // Détruire explicitement le loader (entités + ressources GPU des meshes)
    // TANT QUE le contexte GL principal est encore vivant : sinon elles
    // seraient libérées lors de la destruction des membres, APRÈS glfwTerminate().
    m_modelLoader.reset();

	SharedQuad::destroy();
    if (m_socket) m_socket->stop();
    if (m_steamManager) {
        m_steamManager->shutdown();
    }
    if (m_loaderWindow && m_window) m_window->destroySharedContext(m_loaderWindow);

    // Détruire le SoundManager AVANT glfwTerminate() : ce dernier peut
    // invalider le contexte OpenAL, ce qui ferait échouer alSourceStop
    // (AL_INVALID_OPERATION 0xa004) dans ~Sound().
    m_menuManager.reset();   // détient un pointeur vers m_soundManager
    m_soundManager.reset();
    m_loadingScreen.reset();

    glfwTerminate();
}

// ---------------------------------------------------------------------------
// Initialisation : Steam d'abord (bloquant) puis Window + Renderer.
// L'overlay Steam a besoin d'être initialisé avant la création du contexte
// OpenGL (hook wglCreateContext). On utilise init() qui lance Steam si absent
// et retente jusqu'à 10 secondes.
// ---------------------------------------------------------------------------

void Game::initialize() {
    // Steam d'abord (bloquant) : lance Steam si absent, retente 10× 1s.
    // La fenêtre n'est créée qu'après → l'overlay pourra hooker le contexte.
    m_steamManager = std::make_unique<SteamManager>();
    bool steamOk = m_steamManager->init();

    if (steamOk) {
        printf("[Game] Steam initialise, creation de la fenetre...\n");
    } else {
        printf("[Game] Steam non disponible, fonctionnement hors-ligne.\n");
    }

    // Création de la fenêtre et du contexte OpenGL APRÈS SteamAPI_InitEx
    m_window   = std::make_unique<Window>();
    m_renderer = std::make_unique<Renderer>();
    m_loadingScreen = std::make_unique<LoadingScreen>();

    loadResources();  // passe en LOADING ou READY en interne
}

// ---------------------------------------------------------------------------
// Chargement de TOUTES les ressources lourdes
// Appelée seulement quand Steam est prêt (ou timeout).
// ---------------------------------------------------------------------------

void Game::loadResources() {
    auto loadingTick = [this](float dt = 0.016f) -> bool {
        beginTextFrame();
        m_loadingScreen->draw(dt);
        m_loadingScreen->drawLabel();
        flushTextFrame();
        m_window->update();
        return !m_window->getShouldClose();
    };

    constexpr int TOTAL_STEPS = 7;
    int step = 0;
    m_loadingScreen->setStep(0, 0);  // pas d'indicateur au début du chargement

    // ── Partie synchrone : tout sauf les modèles 3D ──

    m_collisionManager  = std::make_unique<CollisionManager>();
    m_camera            = std::make_unique<Camera>();
    m_socket            = std::make_unique<Socket>();
    m_textureManager    = std::make_unique<TextureManager>();
    m_soundManager      = std::make_unique<SoundManager>(m_window.get());
    m_cursorManager     = std::make_unique<CursorManager>(m_window.get());
    m_shaderManager     = std::make_unique<ShaderManager>(m_camera.get());
    m_player            = std::make_unique<Player>(m_collisionManager.get(), m_renderer.get());
    m_player->setCamera(m_camera.get());
    m_camera->setCollisionManager(m_collisionManager.get());
    m_camera->setRenderer(m_renderer.get());
    m_lightManager      = std::make_unique<LightManager>(m_renderer.get(), m_player.get());
    m_textRenderers     = std::make_unique<std::vector<std::unique_ptr<TextRenderer>>>();
    m_menuManager       = std::make_unique<MenuManager>(this, m_soundManager.get(), m_renderer.get(), m_textRenderers.get(), m_textureManager.get(), m_shaderManager.get(), m_cursorManager.get());
    m_inputManager      = std::make_unique<InputManager>(this, m_menuManager.get(), m_window.get(), m_player.get());

    m_menuManager->setInputManager(m_inputManager.get());

    m_textRenderers->emplace_back(std::make_unique<TextRenderer>(m_shaderManager.get()));
    m_textRenderers->emplace_back(std::make_unique<TextRenderer>(m_shaderManager.get()));

    // Injecter le TextRenderer dans le LoadingScreen pour les labels d'étape
    m_loadingScreen->setTextRenderer((*m_textRenderers)[0].get());

    m_loadingScreen->setStep(++step, TOTAL_STEPS);

    // Charger les polices avant le tick pour que le label s'affiche
	m_textRenderers->at(0)->loadFont("res/fonts/armana/Amarna-Bold.ttf", 96.0f);
    m_textRenderers->at(1)->loadFont("res/fonts/Gnocchi.ttf", 282.0f);

    if (!loadingTick()) return;

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    m_socket->connectToServerAsync(ServerInfo());

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    Texture* containerTexture = m_textureManager->getTexture("container");
    Shader* cubeShader = m_shaderManager->getShader("cube/severallights");
    Shader* lightShader = m_shaderManager->getShader("cube/lightsource");
    std::vector<Texture*> crateTextures = { containerTexture };

    m_lightManager->addPointLight(new LightSource(
        glm::vec3(1, 0.5, 2), lightShader, m_player.get(),
        glm::vec3(0.2f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(5.0f, 0.0f, 0.0f)));

    m_lightManager->addPointLight(new LightSource(
        glm::vec3(3, 0.5, -2), lightShader, m_player.get(),
        glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(0.0f, 5.0f, 0.0f)));

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0, 0),  1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, 0, -2), 1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0.5, 2), 1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes[2]->setSpin(10.0f, glm::vec3(1.0f, 0.0f, 0.0f));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, -12, 0), 24.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));

    m_collisionManager->addStaticMesh(m_cubes[0]->getMesh(), m_cubes[0]->getTransformation()->getMatrix(), "cube1");
    m_collisionManager->addStaticMesh(m_cubes[1]->getMesh(), m_cubes[1]->getTransformation()->getMatrix(), "cube2");
    m_collisionManager->addStaticMesh(m_cubes[3]->getMesh(), m_cubes[3]->getTransformation()->getMatrix(), "cube4");
    m_collisionManager->buildBVH();

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    if (m_steamManager && m_steamManager->isInitialized()) {
        m_steamManager->setOnInviteReceived([this](CSteamID lobbyID) {
            printf("[Game] Invitation recue, tentative de rejoindre le lobby %llu...\n",
                   lobbyID.ConvertToUint64());
            m_steamManager->joinLobby(lobbyID);
        });

        m_steamManager->setOnLobbyCreated([this](CSteamID lobbyID) {
            printf("[Game] Lobby cree avec succes, ouverture de l'invitation...\n");
            m_steamManager->openInviteDialog();
        });

        m_steamManager->setOnLobbyEntered([this](CSteamID lobbyID) {
            printf("[Game] Connecte au lobby %llu !\n", lobbyID.ConvertToUint64());
            if (m_socket) {
                // TODO: utiliser le lobby pour établir la connexion réseau
            }
        });

        m_steamManager->setOnLobbyLeft([this]() {
            printf("[Game] Quitte le lobby.\n");
        });

        if (m_argc > 0 && m_argv != nullptr) {
            m_steamManager->parseCommandLine(m_argc, m_argv);
        }
    } else {
        printf("[Game] Steam non disponible, fonctionnement hors-ligne.\n");
    }

    glGetString(GL_VERSION) ? std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl
        : throw std::runtime_error("Impossible de recuperer la version OpenGL");

    m_loadingScreen->setStep(++step, TOTAL_STEPS);

    // ── Skybox nocturne : cubemap chargé dans le contexte principal ──
    m_skybox = std::make_unique<Skybox>(Constants::File::SKYBOX_NIGHT_PATH);

    // ── Partie asynchrone : modèles 3D sur un thread separe ──
    m_modelLoader = std::make_unique<ModelLoader>(m_camera.get(), m_lightManager.get(), m_renderer.get(),
                                                  m_textureManager.get(), m_inputManager.get());
    m_loaderWindow = m_window->createSharedContext();
    if (m_loaderWindow) {
        m_loadingDone = false;
        m_loadingThread = std::thread([this]() {
            m_modelLoader->load(m_loaderWindow);
            m_loadingDone = true;
        });
        m_initPhase = InitPhase::LOADING;
        printf("[Game] Modeles en cours de chargement (thread separe)...\n");
    } else {
        // Fallback : chargement synchrone si le contexte partage échoue
        printf("[Game] Contexte partage indisponible, chargement synchrone.\n");
        m_modelLoader->load(nullptr);
        adoptLoadedEntities();
        m_loadingScreen.reset();
        m_initPhase = InitPhase::READY;
    }
}

void Game::run() {
    while (!m_window->getShouldClose()) {
        m_renderer->handleFrameTiming();

        // ---- Phase d'init : chargement asynchrone des modèles ----
        if (m_initPhase == InitPhase::LOADING) {
            float dt = m_renderer->getDeltaTime();

            if (m_loadingDone) {
                // Rejoindre le thread une seule fois
                if (m_loadingThread.joinable()) {
                    m_loadingThread.join();
                    adoptLoadedEntities();

                    // Recréer les VAO dans le contexte principal (les VAO ne
                    // sont pas partages entre contextes OpenGL).
                    auto reloadMeshes = [](const std::vector<Mesh*>& meshes) {
                        for (Mesh* m : meshes) m->reloadGPUResources();
                    };
                    if (m_modelEntity)   reloadMeshes(m_modelEntity->getModel()->getMeshes());
                    if (m_fropyEntity)   reloadMeshes(m_fropyEntity->getModel()->getMeshes());
                    if (m_humanEntity)   reloadMeshes(m_humanEntity->getModel()->getMeshes());
                    if (m_firstPersonArms) reloadMeshes(m_firstPersonArms->getMeshes());

                    if (m_loaderWindow) {
                        m_window->destroySharedContext(m_loaderWindow);
                        m_loaderWindow = nullptr;
                    }
                }

                m_loadingScreen->setStep(7, 7);  // toutes les etapes completees

                m_loadingFadeTimer += dt;

                if (m_loadingFadeTimer < LOADING_EXTRA_DELAY) {
                    // Délai post-chargement : loading screen à 100%
                    beginTextFrame();
                    m_loadingScreen->draw(dt);
                    m_loadingScreen->drawLabel();
                    flushTextFrame();
                } else {
                    // Fondu : alpha décroît de 1.0 à 0.0
                    float elapsed = m_loadingFadeTimer - LOADING_EXTRA_DELAY;
                    float alpha = 1.0f - (elapsed / LOADING_FADE_DURATION);

                    if (alpha > 0.0f) {
                        m_loadingScreen->draw(dt, alpha);
                        // Pas de label pendant le fondu : le texte ne peut pas
                        // s'estomper proprement via renderText() (pas de paramètre alpha).
                    } else {
                        m_loadingScreen.reset();
                        m_initPhase = InitPhase::READY;
                        printf("[Game] Chargement termine !\n");
                    }
                }
            } else {
                beginTextFrame();
                m_loadingScreen->draw(dt);
                m_loadingScreen->drawLabel();
                flushTextFrame();
            }
            m_window->update();
            continue;
        }

        // Traiter les callbacks Steam à chaque frame
        if (m_steamManager && m_steamManager->isInitialized()) {
            m_steamManager->runCallbacks();
        }

        switch (m_menuManager->getCurrentState()) {
        case STATE_MENU:
        case STATE_OPTIONS:
        case STATE_PAUSED:
            // UPDATE
            m_inputManager->update();
            m_menuManager->update();
            
            m_renderer->clear();
            // DRAW
            beginTextFrame();
            m_menuManager->draw();
            flushTextFrame();

            m_window->update();
            break;
        case STATE_PLAYING:
            update();
            m_renderer->clear();
            draw();
            m_window->update();
            break;
        default:
            throw std::runtime_error("Etat du jeu inconnu");
            break;
        }
    }
}

void Game::update() {
	ClientEvent socketEvent;
    if (m_socket->pollEvent(socketEvent)) {
        printf("%s\n", socketEvent.data.c_str());
    }

    m_inputManager->update();
    m_camera->update(m_player.get());



    for (auto& cube : m_cubes) {
        cube->update();
    }
    for (auto& alphacube : m_alphacubes) {
        alphacube->update();
    }

    m_collisionManager->updateDynamic("cube3", { m_cubes[2]->getMesh() }, m_cubes[2]->getTransformation()->getMatrix());

    m_player->update();

    m_lightManager->update();

    m_soundManager->setListenerTransform(m_camera->getPosition(), m_camera->getFront(), m_camera->getUp());
    m_soundManager->update();

    m_collisionManager->updateDynamic("backpack", m_modelEntity->getModel()->getMeshes(), m_modelEntity->getModelMatrix());
    m_collisionManager->updateDynamic("fropy_low_poly", m_fropyEntity->getModel()->getMeshes(), m_fropyEntity->getModelMatrix());
    m_fropyEntity->update();

    // Humain 3eme personne : suit la position et la direction du joueur.
    if (m_humanEntity) {
        m_humanEntity->setPosition(m_player->getPosition());
        m_humanEntity->setDirection(*m_player->getDirection());

        // Animation du personnage 3P : deleguee au CharacterAnimationController
        // Sauf en no-clip (gravite desactivee) → on force l'idle.
        if (m_characterAnim) {
            if (m_player->isGravityEnabled()) {
                m_characterAnim->update(m_player->getPosition(),
                                        m_renderer->getDeltaTime(),
                                        m_player->getIsSprinting(),
                                        m_collisionManager->getIsPlayerGrounded());
            } else {
                // No-clip : rester en idle
                int idle = m_humanEntity->getIdleAnimIndex();
                if (idle >= 0) m_humanEntity->playAnimation(idle, true);
                m_humanEntity->updateAnimation(m_renderer->getDeltaTime());
            }
        }
    }

    // Mise a jour du bobbing des bras en premiere personne
    if (m_firstPersonArms) {
        m_firstPersonArms->update(m_renderer->getDeltaTime(), m_player->getPosition(), m_player->getIsSprinting());
    }
}

void Game::draw() {
    beginTextFrame();  // vide le batch de glyphes de la frame

    // 0. Ciel : rendu en premier (depth LEQUAL + mask off), la géométrie de
    // la scène le recouvre ensuite naturellement grâce au depth test.
    if (m_skybox) {
        m_skybox->draw(m_shaderManager->getShader("skybox"), m_camera.get());
    }

    // 1. Opaques
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    for (auto& cube : m_cubes) {
        cube->draw();
    }
    m_lightManager->draw();

    m_modelEntity->draw(m_shaderManager->getShader("model"));
    m_fropyEntity->draw(m_shaderManager->getShader("model"));

    // Humain 3ème personne (visible uniquement en vue 3P)
    if (m_humanEntity && m_player->isThirdPerson()) {
        m_humanEntity->draw(m_shaderManager->getShader("skinned"));
    }

    // 2. Transparences
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Tri des cubes transparents du plus loin au plus proche
    std::sort(m_alphacubes.begin(), m_alphacubes.end(),
        [this](const std::unique_ptr<Cube>& a, const std::unique_ptr<Cube>& b) {
            float da = glm::length(m_camera->getPosition() - a->getCenter());
            float db = glm::length(m_camera->getPosition() - b->getCenter());
            return da > db; // plus loin d'abord
        }
    );

    for (auto& alphacube : m_alphacubes) {
        alphacube->draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Bras : uniquement en 1ère personne (overlay viewmodel).
    if (m_firstPersonArms && !m_player->isThirdPerson()) {
        m_firstPersonArms->draw(m_shaderManager->getShader("skinned"));
    }

    // ── HUD debug (personnage 3P) : liste des animations du modele ────────
    // Desactive par defaut (couteux en FPS). Decommenter pour debugger.
    // if (m_humanEntity && m_player->isThirdPerson() && m_textRenderers) {
    //     CharacterAnimationController::drawDebugHUD(m_humanEntity, *m_textRenderers);
    // }

    // Tout le texte de la frame (HUD, menus...) est dessiné en UN SEUL draw
    // call batche, au-dessus du reste de la scène.
    flushTextFrame();
}

void Game::beginTextFrame() {
    if (!m_textRenderers) return;
    for (auto& tr : *m_textRenderers) tr->beginFrame();
}

void Game::flushTextFrame() {
    if (!m_textRenderers) return;
    for (auto& tr : *m_textRenderers) tr->flush();
}

void Game::changeState(GameState newState) {
    m_menuManager->changeState(newState);
    Sound* sound;
    switch (newState) {
    case STATE_MENU:
    case STATE_OPTIONS:
        m_inputManager->setContext(InputContext::MENU);
		m_window->setCursorCaptured(false);
		m_soundManager->applyReverbToAll(ReverbPreset::UNDERWATER);
        break;
    case STATE_PLAYING:
        m_inputManager->setContext(InputContext::GAME);
        m_window->setCursorCaptured(true);
        m_soundManager->applyReverbToAll(ReverbPreset::NONE);

        sound = m_soundManager->load("on&on", "res/sounds/on&on.wav", true, 0.5f, 1.0f);
        // sound->play();
        break;
    case STATE_PAUSED:
        m_inputManager->setContext(InputContext::PAUSED);
        m_window->setCursorCaptured(false);
        m_soundManager->applyReverbToAll(ReverbPreset::UNDERWATER);
        break;
    }
}

// ---------------------------------------------------------------------------
// Le chargement des modèles 3D a été extrait dans ModelLoader::load()
// (exécuté sur le thread de chargement, voir loadResources()).
// ---------------------------------------------------------------------------

void Game::adoptLoadedEntities() {
    // Vues non-propriétaires vers les entités possédées par le ModelLoader.
    m_modelEntity     = m_modelLoader->getModelEntity();
    m_fropyEntity     = m_modelLoader->getFropyEntity();
    m_humanEntity     = m_modelLoader->getHumanEntity();
    m_firstPersonArms = m_modelLoader->getFirstPersonArms();
    m_characterAnim   = m_modelLoader->getCharacterAnim();
}

void Game::stop() {
    glfwSetWindowShouldClose(m_window->getGLFWwindow(), GLFW_TRUE);
}
