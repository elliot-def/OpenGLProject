#include "win_compat.h"

#include "Game.h"
#include "config.h"
#include "LoadingScreen.h"
#include "SteamManager.h"
#include "Animator.h"
#include "TextRenderer.h"
#include "Skybox.h"
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
	SharedQuad::destroy();
    delete m_humanEntity;
    delete m_fropyEntity;
    delete m_modelEntity;
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
        m_loadingScreen->draw(dt);
        m_loadingScreen->drawLabel();
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
    m_loaderWindow = m_window->createSharedContext();
    if (m_loaderWindow) {
        m_loadingDone = false;
        m_loadingThread = std::thread(&Game::loadModelsAsync, this);
        m_initPhase = InitPhase::LOADING;
        printf("[Game] Modeles en cours de chargement (thread separe)...\n");
    } else {
        // Fallback : chargement synchrone si le contexte partage échoue
        printf("[Game] Contexte partage indisponible, chargement synchrone.\n");
        loadModelsAsync();
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
                    m_loadingScreen->draw(dt);
                    m_loadingScreen->drawLabel();
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
                m_loadingScreen->draw(dt);
                m_loadingScreen->drawLabel();
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
            m_menuManager->draw();

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

    // Humain 3ème personne : suit la position et la direction du joueur.
    if (m_humanEntity) {
        m_humanEntity->setPosition(m_player->getPosition());
        m_humanEntity->setDirection(*m_player->getDirection());

        // ── Animations du personnage 3P ────────────────────────────────────
        // États : run (sprint), walk (marche), rest→idle (immobile : on joue
        // "Rest" pendant kHumanRestToIdleDelay avant de basculer sur l'idle), et
        // punch one-shot (jab sur R = KEY_PUSH) prioritaire qui reprend l'état
        // de mouvement une fois terminé.
        if (m_humanEntity->hasAnimations()) {
            const float dt = m_renderer->getDeltaTime();

            // Détection du mouvement par le delta de position horizontal
            static glm::vec3 lastHumanPos = m_player->getPosition();
            const glm::vec3 delta = m_player->getPosition() - lastHumanPos;
            const float horizSpeed = glm::length(glm::vec3(delta.x, 0.0f, delta.z))
                                     / std::max(dt, 1e-5f);
            const bool isMoving = horizSpeed > 0.1f;
            lastHumanPos = m_player->getPosition();

            const int idleIdx  = m_humanEntity->getIdleAnimIndex();
            const int walkIdx  = m_humanEntity->getWalkAnimIndex();
            const int runIdx   = m_humanEntity->getRunAnimIndex();
            const int punchIdx = m_humanEntity->getPunchAnimIndex();
            const int restIdx  = m_humanEntity->getRestAnimIndex();

            // ── Sélection de l'animation de repos (hors punch) ──
            static int lastAnimIdx = -1;
            static float restTimer = 0.0f;
            static constexpr float kHumanRestToIdleDelay = 2.5f; // "quelques temps" avant l'idle

            // ── Jab (touche R, front montant) : punch one-shot ──
            static bool prevRDown = false;
            static bool punching = false;
            const bool rDown = m_inputManager->getKey("Push")->getStatus();
            if (rDown && !prevRDown && !punching && punchIdx >= 0) {
                m_humanEntity->playAnimation(punchIdx, false);
                punching = true;
                // Marquer l'animation courante : à la fin du punch, la cible de
                // mouvement (walk/run/rest/idle) différera forcément de
                // lastAnimIdx → la machine rejoue la bonne animation au lieu de
                // rester figée sur la dernière pose du punch.
                lastAnimIdx = punchIdx;
            }
            prevRDown = rDown;

            // Fin du punch (animation non-loop terminée) : on reprend la
            // machine à états — la cible de mouvement rejouée diffère de
            // lastAnimIdx (= punch) donc elle reprend proprement.
            if (punching && m_humanEntity->getAnimator()->isFinished()) {
                punching = false;
            }

            int targetIdx = -1;
            if (!punching) {
                if (isMoving) {
                    restTimer = 0.0f;
                    targetIdx = (m_player->getIsSprinting() && runIdx >= 0) ? runIdx : walkIdx;
                } else {
                    // Immobile : "Rest" pendant le délai, puis idle
                    if (restIdx >= 0 && restTimer < kHumanRestToIdleDelay) {
                        targetIdx = restIdx;
                        restTimer += dt;
                    } else {
                        targetIdx = idleIdx;
                    }
                }

                // Ne (re)lancer que si l'animation cible diffère de l'actuelle
                if (targetIdx >= 0 && targetIdx != lastAnimIdx) {
                    m_humanEntity->playAnimation(targetIdx, true);
                    lastAnimIdx = targetIdx;
                }
            }

            m_humanEntity->updateAnimation(dt);
        }
    }

    // Mise à jour du bobbing des bras en première personne
    if (m_firstPersonArms) {
        m_firstPersonArms->update(m_renderer->getDeltaTime(), m_player->getPosition(), m_player->getIsSprinting());
    }
}

void Game::draw() {
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

    // ── HUD debug (personnage 3P) : liste des animations du modèle ────────
    // Liste TOUTES les animations du modèle avec leur index. La ligne de
    // l'animation EN COURS est surlignée en vert, et les animations détectées
    // comme idle/walk par ModelEntity::detectAnimations() sont marquées
    // "(idle)"/"(walk)" pour vérifier le choix (ex: "Walk" au lieu de "Walk-F").
    // Texte 2D dessiné sans depth test (comme les menus).
    if (m_humanEntity && m_humanEntity->hasAnimations() && m_player->isThirdPerson()
        && m_textRenderers && !m_textRenderers->empty()) {
        const aiScene* scene = m_humanEntity->getModel()->getScene();
        const Animator* animator = m_humanEntity->getAnimator();
        const int idleIdx  = m_humanEntity->getIdleAnimIndex();
        const int walkIdx  = m_humanEntity->getWalkAnimIndex();
        const int runIdx   = m_humanEntity->getRunAnimIndex();
        const int punchIdx = m_humanEntity->getPunchAnimIndex();
        const int restIdx  = m_humanEntity->getRestAnimIndex();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        TextRenderer* hud = (*m_textRenderers)[0].get();
        const float scale = 0.30f;
        const float lineH = 34.0f;
        float y = 20.0f;

        // En-tête : couleur neutre (le VERT est réservé à l'animation en cours)
        hud->renderText("Animations du modele (vert = en cours)",
                        20.0f, y, scale, 0.85f, 0.85f, 0.85f);
        y += lineH;

        // Garde anti-débordement : ne jamais lister plus de lignes que ne peut
        // afficher l'écran (human_1.glb n'en a que 13, mais un modèle futur
        // pourrait en avoir beaucoup plus).
        constexpr unsigned int MAX_HUD_ANIMS = 32;

        if (scene) {
            for (unsigned int i = 0; i < scene->mNumAnimations && i < MAX_HUD_ANIMS; i++) {
                const std::string name(scene->mAnimations[i]->mName.C_Str());
                std::string line = "[" + std::to_string(i) + "] " + name;
                if (idleIdx  >= 0 && i == static_cast<unsigned int>(idleIdx))  line += "  (idle)";
                if (walkIdx  >= 0 && i == static_cast<unsigned int>(walkIdx))  line += "  (walk)";
                if (runIdx   >= 0 && i == static_cast<unsigned int>(runIdx))   line += "  (run)";
                if (punchIdx >= 0 && i == static_cast<unsigned int>(punchIdx)) line += "  (punch)";
                if (restIdx  >= 0 && i == static_cast<unsigned int>(restIdx))  line += "  (rest)";

                // Vert si c'est l'animation en cours, sinon blanc/gris
                const bool isCurrent = animator && (scene->mAnimations[i] == animator->getCurrentAnimation());
                hud->renderText(line, 20.0f, y, scale,
                    isCurrent ? 0.35f : 0.85f,
                    isCurrent ? 0.95f : 0.85f,
                    isCurrent ? 0.45f : 0.85f);
                y += lineH;
            }
        }

        // Restaurer l'état GL : désactiver le blend (sinon le passage opaque de
        // la frame suivante serait dessiné avec le blending actif) + depth test.
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
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
// Chargement asynchrone des modèles 3D (exécuté sur un thread separe)
// ---------------------------------------------------------------------------

void Game::loadModelsAsync() {
    if (m_loaderWindow)
        glfwMakeContextCurrent(m_loaderWindow);

    printf("[Game]   → Chargement du backpack...\n");
    m_modelEntity = new ModelEntity(m_camera.get(), m_lightManager.get(), m_renderer.get(),
                                    "./res/models/backpack/backpack.obj", m_textureManager.get());

    printf("[Game]   → Chargement de fropy (low poly)...\n");
    m_fropyEntity = new ModelEntity(m_camera.get(), m_lightManager.get(), m_renderer.get(),
                                    "./res/models/fropy/fropy_low_poly.obj", m_textureManager.get());
    m_fropyEntity->setPosition(glm::vec3(3.0f, 5.0f, 0.0f));
    m_fropyEntity->setSpin(20.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    printf("[Game]   → Chargement des bras (rigges)...\n");
    m_firstPersonArms = std::make_unique<FirstPersonArms>(m_camera.get(), m_lightManager.get(),
                                             "./res/rigging/arm/arms_rig.glb", m_textureManager.get());
    m_inputManager->setFirstPersonArms(m_firstPersonArms.get());

    printf("[Game]   → Chargement de l'humain (rigge)...\n");
    m_humanEntity = new ModelEntity(m_camera.get(), m_lightManager.get(), m_renderer.get(),
                                    "./res/rigging/human/human_1.glb", m_textureManager.get());

    if (m_loaderWindow)
        glfwMakeContextCurrent(nullptr);

    m_loadingDone = true;
}

void Game::stop() {
    glfwSetWindowShouldClose(m_window->getGLFWwindow(), GLFW_TRUE);
}
