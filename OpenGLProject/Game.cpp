#include "win_compat.h"

#include "Game.h"
#include "config.h"
#include "LoadingScreen.h"
#include "SteamManager.h"
#include "ModelLoader.h"
#include "TextRenderer.h"
#include "FirstPersonArms.h"
#include "Animator.h"
#include "CharacterAnimationController.h"
#include "Scene.h"
#include "Log.h"
#include "ShaderManager.h"
#include "constants/camera.h"
#include "constants/file.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Controller.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
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

    // La Scene possède les cubes et le skybox (ressources GPU) : même raison,
    // on la détruit avant glfwTerminate().
    m_scene.reset();

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
    // ── Mode hors-ligne forcé : argument -offline / -nosteam ──
    // Steam n'est alors ni initialisé ni lancé : aucune capture de la manette
    // par Steam Input, XInput/GLFW reste seul maître.
    m_offlineMode = false;
    for (int i = 1; i < m_argc; ++i) {
        if (m_argv[i] &&
            (std::strcmp(m_argv[i], "-offline") == 0 ||
             std::strcmp(m_argv[i], "--offline") == 0 ||
             std::strcmp(m_argv[i], "-nosteam") == 0 ||
             std::strcmp(m_argv[i], "--nosteam") == 0)) {
            m_offlineMode = true;
            break;
        }
    }

    // ── Diagnostic manette : liste ce que GLFW/XInput voit AVANT
    // SteamAPI_InitEx. Si Steam Input est actif pour ce jeu, il peut masquer
    // la manette du XInput : la comparaison avant/apres le montre.
    Controller::logDevices("AVANT Steam (pre-SteamAPI_InitEx)");

    // Steam d'abord (bloquant) : lance Steam si absent, retente 10× 1s.
    // La fenêtre n'est créée qu'après → l'overlay pourra hooker le contexte.
    // En mode hors-ligne forcé, on saute complètement l'initialisation Steam.
    m_steamManager = std::make_unique<SteamManager>();
    bool steamOk = false;
    if (m_offlineMode) {
        printf("[Game] Mode hors-ligne force (-offline/-nosteam) : Steam ignore, manette via XInput.\n");
    } else {
        steamOk = m_steamManager->init();
    }

    Controller::logDevices(m_offlineMode
        ? "APRES (mode hors-ligne force, Steam ignore)"
        : (steamOk ? "APRES Steam (init OK)" : "APRES Steam (init echec / hors-ligne)"));

    if (steamOk) {
        LOG_INFO("[Game] Steam initialise, creation de la fenetre...");
    } else {
        // LOG_INFO concatene son 1er argument dans un litteral de chaine :
        // on passe donc le texte via %s (une expression ternaire ne peut pas
        // servir de fmt).
        LOG_INFO("%s", m_offlineMode
            ? "[Game] Mode hors-ligne force, Steam ignore."
            : "[Game] Steam non disponible, fonctionnement hors-ligne.");
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

    // Monde 3D : les objets du monde (cubes, lumières, skybox, entités)
    // vivent dans Scene ; Game garde les services ci-dessus.
    m_scene = std::make_unique<Scene>(m_camera.get(), m_player.get(),
                                      m_collisionManager.get(), m_lightManager.get(),
                                      m_renderer.get(), m_shaderManager.get(),
                                      m_textureManager.get(), m_inputManager.get());

    // 0 : Amarna (texte des menus + notification), 1 : Gnocchi (titres),
    // 2 : icones manette kenney, 3 : icones clavier/souris kenney
    m_textRenderers->emplace_back(std::make_unique<TextRenderer>(m_shaderManager.get()));
    m_textRenderers->emplace_back(std::make_unique<TextRenderer>(m_shaderManager.get()));
    m_textRenderers->emplace_back(std::make_unique<TextRenderer>(m_shaderManager.get()));
    m_textRenderers->emplace_back(std::make_unique<TextRenderer>(m_shaderManager.get()));

    // Injecter le TextRenderer dans le LoadingScreen pour les labels d'étape
    m_loadingScreen->setTextRenderer((*m_textRenderers)[0].get());

    m_loadingScreen->setStep(++step, TOTAL_STEPS);

    // Charger les polices avant le tick pour que le label s'affiche
	m_textRenderers->at(0)->loadFont("res/fonts/armana/Amarna-Bold.ttf", 96.0f);
    m_textRenderers->at(1)->loadFont("res/fonts/Gnocchi.ttf", 282.0f);
    // Icônes kenney (zone privee U+E000..) : manette Xbox + clavier/souris,
    // utilisees par la notification de bascule de source d'entree.
    m_textRenderers->at(2)->loadFontRange("res/fonts/kenney/kenney_input_steam_controller.ttf", 48.0f, 0xE000, 67);
    m_textRenderers->at(3)->loadFontRange("res/fonts/kenney/kenney_input_keyboard_&_mouse.ttf", 48.0f, 0xE000, 243);

    if (!loadingTick()) return;

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    m_socket->connectToServerAsync(ServerInfo());

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    m_scene->loadLights();

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    m_scene->loadCubes();

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    if (m_steamManager && m_steamManager->isInitialized()) {
        m_steamManager->setOnInviteReceived([this](CSteamID lobbyID) {
            LOG_INFO("[Game] Invitation recue, tentative de rejoindre le lobby %llu...",
                     lobbyID.ConvertToUint64());
            m_steamManager->joinLobby(lobbyID);
        });

        m_steamManager->setOnLobbyCreated([this](CSteamID lobbyID) {
            LOG_INFO("[Game] Lobby cree avec succes, ouverture de l'invitation...");
            m_steamManager->openInviteDialog();
        });

        m_steamManager->setOnLobbyEntered([this](CSteamID lobbyID) {
            LOG_INFO("[Game] Connecte au lobby %llu !", lobbyID.ConvertToUint64());
            if (m_socket) {
                // TODO: utiliser le lobby pour établir la connexion réseau
            }
        });

        m_steamManager->setOnLobbyLeft([this]() {
            LOG_INFO("[Game] Quitte le lobby.");
        });

        if (m_argc > 0 && m_argv != nullptr) {
            m_steamManager->parseCommandLine(m_argc, m_argv);
        }
    } else {
        LOG_INFO("[Game] Steam non disponible, fonctionnement hors-ligne.");
    }

    const GLubyte* glVersion = glGetString(GL_VERSION);
    if (glVersion) {
        LOG_INFO("OpenGL version: %s", glVersion);
    } else {
        throw std::runtime_error("Impossible de recuperer la version OpenGL");
    }

    m_loadingScreen->setStep(++step, TOTAL_STEPS);

    // ── Skybox nocturne : cubemap chargé dans le contexte principal ──
    m_scene->createSkybox(Constants::File::SKYBOX_NIGHT_PATH);

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
        LOG_INFO("[Game] Modeles en cours de chargement (thread separe)...");
    } else {
        // Fallback : chargement synchrone si le contexte partage échoue
        LOG_INFO("[Game] Contexte partage indisponible, chargement synchrone.");
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
                        LOG_INFO("[Game] Chargement termine !");
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
            // Notification de bascule clavier/souris <-> manette (au-dessus des menus)
            drawInputNotification();
            flushTextFrame();
            // Easter egg DVD : dessiné APRÈS le flush du texte pour passer devant lui
            m_menuManager->drawOverlays();

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
        LOG_INFO("%s", socketEvent.data.c_str());
    }

    m_inputManager->update();

    // Monde 3D : cubes, collisions, joueur, caméra, lumières, entités.
    m_scene->update(m_renderer->getDeltaTime());

    // En 1P, placer la camera sur la position de la tete du modele anime.
    if (!m_player->isThirdPerson() && m_humanEntity && m_humanEntity->hasAnimations()) {
        Animator* anim = m_humanEntity->getAnimator();
        if (anim) {
            // Chercher le bone de tete dans la boneMap (insensible a la casse)
            std::string headBone;
            for (const auto& [name, info] : m_humanEntity->getModel()->getBoneInfoMap()) {
                std::string lower = name;
                for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (lower.find("head") != std::string::npos
                    && lower.find("end") == std::string::npos) {
                    headBone = name;
                    break;
                }
            }
            if (!headBone.empty()) {
                // Position de la tete en espace modele, convertie en monde
                glm::mat4 headMat = anim->getGlobalNodeTransform(headBone);
                glm::vec3 worldHead = glm::vec3(
                    m_humanEntity->getModelMatrix() * glm::vec4(glm::vec3(headMat[3]), 1.0f));
                // Ne suivre que X et Z (le Y reste gere par Camera::update()).
                // Reapplique les offsets ecrases par le setPosition :
                //  - mouvement lisse (direction de marche × lerp)
                //  - regard statique (direction du regard, permanent)
                glm::vec3 camPos = m_camera->getPosition();
                glm::vec3 frontFlat = glm::normalize(glm::vec3(
                    m_camera->getFront().x, 0.0f, m_camera->getFront().z));
                glm::vec3 lookOffset = frontFlat * Constants::Camera::CAMERA_FP_LOOK_OFFSET;
                glm::vec3 newCamPos = glm::vec3(worldHead.x, camPos.y, worldHead.z)
                    + m_camera->getSmoothedMovementOffset()
                    + lookOffset;

                // Garde anti-ecran-bleu : une position non finie (NaN/inf,
                // ex: transform de bone invalide) ou aberrante (> 10m du
                // joueur) rendrait la camera invisible -> seule la couleur de
                // clear est rastérisée. On retombe sur les yeux du joueur.
                const bool finite = std::isfinite(newCamPos.x)
                    && std::isfinite(newCamPos.y) && std::isfinite(newCamPos.z);
                const float distToPlayer = glm::length(newCamPos - m_player->getPosition());
                if (finite && distToPlayer < 10.0f) {
                    m_camera->setPosition(newCamPos);
                } else {
                    m_camera->setPosition(m_player->getEyePosition());
                    LOG_WARN("[Game] Position camera 1P invalide (bone='%s', "
                             "finite=%d dist=%.2f), fallback sur les yeux",
                             headBone.c_str(), finite ? 1 : 0, distToPlayer);
                }
            }

            // Diagnostic temporaire (ecran bleu 1P) : position cam/joueur
            // toutes les ~5s pour verifier que la camera reste dans la scene.
            static int fpDiagCounter = 0;
            if (++fpDiagCounter % 300 == 0) {
                glm::vec3 p = m_player->getPosition();
                glm::vec3 c = m_camera->getPosition();
                LOG_INFO("[Game] 1P diag: cam=(%.2f, %.2f, %.2f) player=(%.2f, %.2f, %.2f) "
                         "thirdPerson=%d headBone='%s' human=%s",
                         c.x, c.y, c.z, p.x, p.y, p.z,
                         m_player->isThirdPerson() ? 1 : 0, headBone.c_str(),
                         m_humanEntity ? "ok" : "null");
            }
        }
    }

    m_soundManager->setListenerTransform(m_camera->getPosition(), m_camera->getFront(), m_camera->getUp());
    m_soundManager->update();
}

void Game::draw() {
    beginTextFrame();  // vide le batch de glyphes de la frame

    // Monde 3D : skybox, opaques, transparences.
    m_scene->draw();

    // ── HUD debug (personnage 3P) : liste des animations du modele ────────
    // Désactivé par défaut ; bascule avec F3 (InputManager -> toggleDebugHUD).
    // Coûteux en FPS (concats std::string + ~32 lignes de texte par frame).
    if (m_debugHUD && m_humanEntity && m_player->isThirdPerson() && m_textRenderers) {
        CharacterAnimationController::drawDebugHUD(m_humanEntity, *m_textRenderers);
    }

    // Notification de bascule clavier/souris <-> manette (icones kenney)
    drawInputNotification();

    // Tout le texte de la frame (HUD, menus...) est dessiné en UN SEUL draw
    // call batche, au-dessus du reste de la scène.
    flushTextFrame();
}

// Dessine la notification de changement de source d'entree (manette <->
// clavier/souris) avec les icones de la police kenney. A appeler entre
// beginTextFrame() et flushTextFrame().
void Game::drawInputNotification() {
    if (!m_inputManager || !m_textRenderers || m_textRenderers->size() < 4) return;
    m_inputManager->getNotification()->draw(
        (*m_textRenderers)[2].get(),  // icones manette kenney (steam_controller)
        (*m_textRenderers)[3].get(),  // icones clavier/souris kenney
        (*m_textRenderers)[0].get()); // texte standard (Amarna)
}

void Game::beginTextFrame() {
    if (!m_textRenderers) return;
    for (auto& tr : *m_textRenderers) tr->beginFrame();
}

void Game::flushTextFrame() {
    if (!m_textRenderers) return;
    for (auto& tr : *m_textRenderers) tr->flush();
}

void Game::toggleDebugHUD() {
    m_debugHUD = !m_debugHUD;
    LOG_INFO("[Game] Debug HUD %s", m_debugHUD ? "ON" : "OFF");
}

void Game::changeState(GameState newState, bool restoreFocus) {
    m_menuManager->changeState(newState, restoreFocus);
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

    // Transmet les vues à la Scene (qui les utilise pour update/draw).
    m_scene->adoptEntities(m_modelEntity, m_fropyEntity, m_humanEntity,
                           m_modelLoader->getCharacterAnim());
}

void Game::stop() {
    glfwSetWindowShouldClose(m_window->getGLFWwindow(), GLFW_TRUE);
}
