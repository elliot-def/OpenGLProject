#include "win_compat.h"

#include "Game.h"
#include "config.h"
#include "LoadingScreen.h"
#include "SteamManager.h"
#include "ModelLoader.h"
#include "MultiplayerManager.h"
#include "LobbyChat.h"
#include "VoiceChat.h"
#include "CreatureVoice.h"
#include "QuestManager.h"
#include "LobbyManager.h"
#include "TextRenderer.h"
#include "FirstPersonArms.h"
#include "Animator.h"
#include "CharacterAnimationController.h"
#include "Scene.h"
#include "Log.h"
#include "ShaderManager.h"
#include "constants/camera.h"
#include "constants/file.h"
#include "constants/resource.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Controller.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
// Pointeur vers l'unique Game, pour le char callback GLFW. Le user pointer de
// la fenetre est deja utilise par le SoundManager (focus callback) qui
// l'ecrase des sa construction dans loadResources().
Game* g_activeGame = nullptr;
} // namespace

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

    // Le chat vocal utilise des sources/buffers OpenAL : on le detruit avant
    // le SoundManager (qui possede le device/contexte AL), sinon les sources
    // seraient supprimees sur un contexte AL deja detruit.
    m_lobbyManager.reset();
    m_voiceChat.reset();

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

    g_activeGame = nullptr;
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
        logPrintf("[Game] Mode hors-ligne force (-offline/-nosteam) : Steam ignore, manette via XInput.\n");
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

    // ── Saisie texte du chat du lobby ──
    // Le char callback GLFW alimente LobbyChat (cree plus tard dans
    // loadResources, d'ou le null-check).
    // NB : le user pointer de la fenetre est ECRASE par le SoundManager (focus
    // callback) des sa construction dans loadResources(), donc on ne peut pas
    // relire glfwGetWindowUserPointer ici (il ne renvoie plus le Game). On
    // passe par un pointeur statique : un lambda capturant ne peut pas devenir
    // un pointeur de fonction GLFW.
    g_activeGame = this;
    glfwSetCharCallback(m_window->getGLFWwindow(),
        [](GLFWwindow* /*window*/, unsigned int codepoint) {
            if (g_activeGame && g_activeGame->m_lobbyChat) {
                g_activeGame->m_lobbyChat->onChar(codepoint);
            }
        });

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

    constexpr int TOTAL_STEPS = 6;
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

    // Chat vocal du lobby (capture micro + lecture des voix). Cree AVANT les
    // menus : le menu Audio et les Options lisent/ecrivent ses reglages, et
    // OptionsMenu::loadJSON (constructeur) applique res/options.json dessus.
    m_voiceChat = std::make_unique<VoiceChat>(m_steamManager.get(),
                                              m_window->getGLFWwindow());
    // Voice lines de fropy (spatialisees a sa tete, cf. CreatureVoice).
    m_creatureVoice = std::make_unique<CreatureVoice>(m_soundManager.get());

    m_menuManager       = std::make_unique<MenuManager>(this, m_soundManager.get(), m_renderer.get(), m_textRenderers.get(), m_textureManager.get(), m_shaderManager.get(), m_cursorManager.get(), m_voiceChat.get());
    m_inputManager      = std::make_unique<InputManager>(this, m_menuManager.get(), m_window.get(), m_player.get());

    m_menuManager->setInputManager(m_inputManager.get());

    // Monde 3D : les objets du monde (cubes, lumières, skybox, entités)
    // vivent dans Scene ; Game garde les services ci-dessus.
    m_scene = std::make_unique<Scene>(m_camera.get(), m_player.get(),
                                      m_collisionManager.get(), m_lightManager.get(),
                                      m_renderer.get(), m_shaderManager.get(),
                                      m_textureManager.get(), m_inputManager.get());

    // Joueurs distants du lobby Steam (synchronisés en P2P). Le modèle local
    // (humanEntity) n'est pas encore chargé : setLocalHuman() sera appelé dans
    // adoptLoadedEntities().
    m_multiplayerManager = std::make_unique<MultiplayerManager>(
        m_steamManager.get(), m_camera.get(), m_lightManager.get(),
        m_renderer.get(), m_textureManager.get(), m_shaderManager.get(),
        m_player.get(), nullptr);

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
	m_textRenderers->at(0)->loadFont(Constants::Resource::FONT_ARMANA, 96.0f);
    m_textRenderers->at(1)->loadFont(Constants::Resource::FONT_GNOCCHI, 282.0f);
    // Icônes kenney (zone privee U+E000..) : manette Xbox + clavier/souris,
    // utilisees par la notification de bascule de source d'entree.
    m_textRenderers->at(2)->loadFontRange(Constants::Resource::FONT_KENNEY_STEAM, 48.0f, 0xE000, 67);
    m_textRenderers->at(3)->loadFontRange(Constants::Resource::FONT_KENNEY_KB_MOUSE, 48.0f, 0xE000, 243);

    // ── Chat du lobby : logs systeme + messages des joueurs (saisie Entree) ──
    m_lobbyChat = std::make_unique<LobbyChat>(m_shaderManager.get(),
                                              m_window->getGLFWwindow(),
                                              m_textRenderers.get());
    // Lobby d'avant-partie : l'hote choisit difficulte / carte / mode, puis lance.
    m_lobbyManager = std::make_unique<LobbyManager>(m_steamManager.get(), m_window->getGLFWwindow(),
        m_shaderManager.get(), m_textRenderers.get(),
        [this](const char* mapPath, const MatchSettings& s) {
            m_scene->setMatchParams(s.creatureEnabled(), s.creatureSpeedMul(), s.creatureSenseMul());
            m_scene->loadMap(mapPath);
            m_questManager->beginMap(m_scene.get(), s.tasks(), s.seed);   // lobby : aucun poste, aucune tache
        });
    // Taches de la partie (mini-jeux sur les postes "quest:..." de la carte).
    m_questManager = std::make_unique<QuestManager>(m_window->getGLFWwindow(), m_shaderManager.get(), m_textRenderers.get());
    m_questManager->setOnAllDone([this]() { m_lobbyManager->onVictory(); });
    m_scene->setButtonCallback([this](const std::string& name) {
        if (!m_questManager->handleButton(name)) m_lobbyManager->onButton(name);
    });
    m_scene->setPlayerHitCallback([this]() {
        m_questManager->close();   // attrape en plein mini-jeu : il se ferme
        m_lobbyManager->onPlayerHit();
    });
    if (m_steamManager && m_steamManager->isInitialized()) {
        m_lobbyChat->setLocalName(m_steamManager->getLocalPersonaName());
    }
    // Envoi des messages : diffusion P2P a tous les membres du lobby.
    m_lobbyChat->setOnSend([this](const std::string& text) {
        if (m_multiplayerManager) m_multiplayerManager->sendChatMessage(text);
    });
    // Reception des messages des pairs -> historique du chat.
    if (m_multiplayerManager) {
        m_multiplayerManager->setOnChatMessage(
            [this](uint64_t /*senderID*/, const std::string& senderName,
                   const std::string& text) {
                if (m_lobbyChat) m_lobbyChat->addUserMessage(senderName, text);
            });
        // Reception des paquets voix -> chat vocal (lecture OpenAL).
        m_multiplayerManager->setOnVoicePacket(
            [this](uint64_t senderID, const uint8_t* data, uint32_t size) {
                if (m_voiceChat) m_voiceChat->onVoicePacket(senderID, data, size);
            });
    }

    if (!loadingTick()) return;

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    m_socket->connectToServerAsync(ServerInfo());

    m_loadingScreen->setStep(++step, TOTAL_STEPS);
    if (!loadingTick()) return;

    // Salle du lobby par defaut ; -day / -night / -batiment chargent directement une carte (debug).
    const char* directMap = nullptr;
    for (int i = 1; i < m_argc; ++i) {
        if (!m_argv[i]) continue;
        if (std::strcmp(m_argv[i], "-night") == 0 || std::strcmp(m_argv[i], "--night") == 0) directMap = Constants::Resource::MAP_VILLAGE_NIGHT;
        if (std::strcmp(m_argv[i], "-day") == 0 || std::strcmp(m_argv[i], "--day") == 0) directMap = Constants::Resource::MAP_VILLAGE_DAY;
        if (std::strcmp(m_argv[i], "-batiment") == 0 || std::strcmp(m_argv[i], "--batiment") == 0) directMap = Constants::Resource::MAP_BATIMENT;
    }
    m_lobbyManager->setOnLobbyMap(directMap == nullptr);
    m_scene->loadMap(directMap ? directMap : LobbyManager::lobbyMapPath());
    m_questManager->beginMap(m_scene.get(), MatchSettings().tasks(), 0u);   // carte directe (debug) : taches par defaut

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
            // Debut d'une nouvelle session : le chat repart d'un historique
            // vide (on peut arriver ici depuis le menu apres une session en
            // lobby, ou d'un premier lancement).
            if (m_lobbyChat) {
                m_lobbyChat->clear();
                m_lobbyChat->addSystemMessage("Lobby cree - invite tes amis !");
            }
            if (m_multiplayerManager) m_multiplayerManager->onLobbyChanged();
            if (m_voiceChat) m_voiceChat->onLobbyChanged();
            m_steamManager->openInviteDialog();
        });

        m_steamManager->setOnLobbyEntered([this](CSteamID lobbyID) {
            LOG_INFO("[Game] Connecte au lobby %llu !", lobbyID.ConvertToUint64());
            if (m_lobbyChat) {
                // Nouvelle session : on efface l'historique de la session
                // precedente AVANT le message de bienvenue. C'est aussi ce
                // clear qui neutralise le callback LobbyLeft en retard de
                // l'ancien lobby (voir setOnLobbyLeft).
                m_lobbyChat->clear();
                m_lobbyChat->addSystemMessage("Vous avez rejoint le lobby");
            }
            if (m_multiplayerManager) m_multiplayerManager->onLobbyChanged();
            if (m_voiceChat) m_voiceChat->onLobbyChanged();
            // Invitation acceptée depuis le menu : entrer directement en jeu
            // pour rejoindre les autres joueurs.
            if (m_menuManager->getCurrentState() == STATE_MENU) {
                changeState(STATE_PLAYING);
            }
        });

        m_steamManager->setOnLobbyLeft([this]() {
            LOG_INFO("[Game] Quitte le lobby (etat du jeu : %d ; 0=menu 1=jeu 2=pause 3=options).",
                     static_cast<int>(m_menuManager->getCurrentState()));
            // Pas de clear/message de chat ici : le callback LobbyLeft d'un
            // ANCIEN lobby peut arriver APRES l'entree dans le nouveau
            // (leave+rejoin — voir SteamManager::onGameLobbyJoinRequested) et
            // effacerait le message de bienvenue de la session en cours. Le
            // chat est reinitialise au DEBUT de chaque session
            // (setOnLobbyCreated / setOnLobbyEntered) et au retour au menu
            // (Game::changeState STATE_MENU).
            if (m_multiplayerManager) m_multiplayerManager->onLobbyLeft();
            if (m_voiceChat) m_voiceChat->onLobbyLeft();
        });

        // Logs systeme : un joueur (autre que le local) rejoint/quitte le
        // lobby (y compris un joueur qui revient apres etre parti : le flag
        // Entered se declenche a nouveau).
        m_steamManager->setOnLobbyMemberUpdate(
            [this](CSteamID /*lobbyID*/, CSteamID memberID, uint32_t flags) {
                if (!m_lobbyChat || !m_steamManager) return;
                const std::string name = m_steamManager->getPersonaName(memberID);
                if (flags & k_EChatMemberStateChangeEntered) {
                    m_lobbyChat->addSystemMessage(name + " a rejoint le lobby");
                }
                if (flags & (k_EChatMemberStateChangeLeft |
                             k_EChatMemberStateChangeDisconnected |
                             k_EChatMemberStateChangeKicked)) {
                    m_lobbyChat->addSystemMessage(name + " a quitte le lobby");
                }
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
    // Skybox : creee par Scene::loadMap selon la carte (jour / nuit).

    // Prépare la musique de jeu en streaming (Ogg Vorbis via stb_vorbis).
    // Seul le fichier compressé (~quelques Mo) est lu ici : le décodage PCM se
    // fait par petits morceaux dans SoundManager::update(), donc plus de freeze
    // au premier "Jouer" (contrairement à l'ancien chargement du .wav complet).
    m_soundManager->loadMusic(Constants::Resource::SOUND_MUSIC_ON_AND_ON);

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

            // Le monde continue de simuler derrière le menu : le personnage
            // retombe (gravité) et son état reste diffusé aux autres joueurs
            // du lobby. Sans cette diffusion, les pairs effacent notre modèle
            // après ~5 s de silence P2P (STALE_TIMEOUT).
            updateWorld(m_renderer->getDeltaTime());
            
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

    // Chat du lobby : quand la saisie est active, les touches du jeu
    // (WASD, Echap...) sont neutralisees et alimentent le chat a la place.
    const bool chatCapturesInput = m_lobbyChat && m_lobbyChat->isInputActive()
                                   && m_steamManager && m_steamManager->isInLobby();
    // Panneau d'options du lobby ouvert : il capture le clavier (comme le chat).
    const bool lobbyPanelOpen = m_lobbyManager && m_lobbyManager->isPanelOpen();
    // Mini-jeu de tache ouvert : il capture aussi les commandes (le monde continue).
    const bool questOpen = m_questManager && m_questManager->isOpen();
    if (!chatCapturesInput && !lobbyPanelOpen && !questOpen) {
        m_inputManager->update();
    }
    if (m_lobbyManager) m_lobbyManager->update(m_renderer->getDeltaTime());
    if (m_questManager) m_questManager->update(m_renderer->getDeltaTime());

    // Monde 3D (physique/gravité, caméra, lumières) + audio + synchro P2P.
    // En jeu uniquement ; dans les menus, Game::run() appelle updateWorld()
    // pour que la simulation continue.
    updateWorld(m_renderer->getDeltaTime());

    // Camera 1P et lampe torche attachees a l'os de tete du modele anime.
    m_headCameraRig.update(m_humanEntity, m_camera.get(), m_player.get(), m_lightManager.get());

    // Chat du lobby (saisie Entree/Echap + messages entrants).
    if (m_lobbyChat && m_steamManager && m_steamManager->isInLobby() && !lobbyPanelOpen) {
        m_lobbyChat->update(m_renderer->getDeltaTime());
    }

    // Chat vocal : capture micro, VAD/PTT, envoi et lecture des voix distantes.
    if (m_voiceChat) {
        m_voiceChat->update(m_renderer->getDeltaTime());
    }
}

// ---------------------------------------------------------------------------
// Simulation du monde, commune au jeu et aux menus ouverts : physique
// (gravité, chute, collisions), caméra, lumières, audio 3D et synchro P2P.
// Appelée quand un menu est ouvert pour que le personnage continue de tomber
// et reste visible des autres joueurs du lobby (sans diffusion de son état,
// les pairs effacent son modèle après ~5 s de silence, STALE_TIMEOUT).
// ---------------------------------------------------------------------------
void Game::updateWorld(float dt) {
    // Monde 3D : cubes, collisions, joueur, caméra, lumières, entités.
    if (m_multiplayerManager) m_scene->setRemoteTargets(m_multiplayerManager->getRemotePositions());
    // Voix = bruit entendu par fropy : joueur local (solo compris) et joueurs distants.
    if (m_voiceChat) {
        if (m_voiceChat->isLocalTransmitting()) {
            m_scene->addNoise(m_player->getPosition(), Constants::Player::PLAYER_VOICE_NOISE_RADIUS);
        }
        glm::vec3 speakerPos;
        for (uint64_t id : m_voiceChat->getSpeakingPlayers()) {
            if (m_multiplayerManager && m_multiplayerManager->getRemotePosition(id, speakerPos)) {
                m_scene->addNoise(speakerPos, Constants::Player::PLAYER_VOICE_NOISE_RADIUS);
            }
        }
    }
    m_scene->update(dt);
    if (m_creatureVoice) m_creatureVoice->update(dt, m_scene->getCreature(), m_camera->getPosition(), m_collisionManager.get());

    m_soundManager->setListenerTransform(m_camera->getPosition(), m_camera->getFront(), m_camera->getUp());
    m_soundManager->update();

    // Joueurs distants : diffuser l'état local + recevoir/appliquer les états
    // distants (création paresseuse de leur modèle Megan).
    if (m_multiplayerManager) {
        m_multiplayerManager->update(dt);
    }
}

void Game::draw() {
    beginTextFrame();  // vide le batch de glyphes de la frame

    // Monde 3D : skybox, opaques, transparences.
    m_scene->draw();

    // Joueurs distants du lobby (Megan, skinned) : rendus au-dessus du monde.
    if (m_multiplayerManager) {
        m_multiplayerManager->draw();
    }

    // ── HUD debug (personnage 3P) : liste des animations du modele ────────
    // Désactivé par défaut ; bascule avec F3 (InputManager -> toggleDebugHUD).
    // Coûteux en FPS (concats std::string + ~32 lignes de texte par frame).
    if (m_debugHUD && m_humanEntity && m_player->isThirdPerson() && m_textRenderers) {
        CharacterAnimationController::drawDebugHUD(m_humanEntity, *m_textRenderers);
    }

    // Notification de bascule clavier/souris <-> manette (icones kenney)
    drawInputNotification();

    // Chat du lobby : panneau + logs systeme + messages des joueurs.
    if (m_lobbyChat && m_steamManager && m_steamManager->isInLobby()) {
        m_lobbyChat->draw();
    }

    // Joueurs qui parlent (solo ou multi) : logo de son + pseudo, au-dessus du chat.
    if (m_lobbyChat && m_voiceChat) {
        std::vector<std::string> speakers;
        const bool steam = m_steamManager && m_steamManager->isInitialized();
        if (m_voiceChat->isLocalTransmitting()) {
            const std::string me = steam ? m_steamManager->getLocalPersonaName() : "";
            speakers.push_back(me.empty() ? std::string("Vous") : me);
        }
        if (steam) {
            for (uint64_t id : m_voiceChat->getSpeakingPlayers()) {
                speakers.push_back(m_steamManager->getPersonaName(CSteamID(id)));
            }
        }
        m_lobbyChat->setSpeakers(std::move(speakers));
        m_lobbyChat->drawSpeakers();
    }

    // Taches : liste (HUD) + mini-jeu ouvert. Puis lobby : options de la partie (droite),
    // panneau de l'hote, invite du bouton vise (masquee pendant un mini-jeu).
    if (m_questManager) m_questManager->draw();
    const bool questOpen = m_questManager && m_questManager->isOpen();
    if (m_lobbyManager) m_lobbyManager->draw(questOpen ? std::string() : m_scene->getAimedButtonLabel());

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
    switch (newState) {
    case STATE_MENU:
    case STATE_OPTIONS:
        // Retour au menu = fin de session : le chat repart vide au prochain
        // jeu (solo depuis "Jouer", ou nouveau lobby via "Multijoueur"/
        // invitation). Le lobby Steam persiste, seul l'historique est vide.
        if (m_lobbyChat) m_lobbyChat->clear();
        // Fermer la saisie du chat (les touches redeviennent celles du menu).
        if (m_lobbyChat) m_lobbyChat->closeInput();
        // Chat vocal coupe hors jeu (micro ferme, voix coupees).
        if (m_voiceChat) m_voiceChat->shutdown();
        m_inputManager->setContext(InputContext::MENU);
		m_window->setCursorCaptured(false);
		m_soundManager->applyReverbToAll(ReverbPreset::UNDERWATER);
        m_soundManager->stopMusic();
        break;
    case STATE_PLAYING:
        m_inputManager->setContext(InputContext::GAME);
        m_window->setCursorCaptured(true);
        m_soundManager->applyReverbToAll(ReverbPreset::NONE);

        // Musique en streaming : démarre/reprend sans chargement bloquant.
        m_soundManager->playMusic();
        break;
    case STATE_PAUSED:
        // Fermer la saisie du chat (Echap du jeu ne doit pas rester capture).
        if (m_lobbyChat) m_lobbyChat->closeInput();
        // Chat vocal coupe en pause (micro ferme, voix coupees).
        if (m_voiceChat) m_voiceChat->shutdown();
        m_inputManager->setContext(InputContext::PAUSED);
        m_window->setCursorCaptured(false);
        m_soundManager->applyReverbToAll(ReverbPreset::UNDERWATER);
        m_soundManager->pauseMusic();
        break;
    }
}

// ---------------------------------------------------------------------------
// Le chargement des modèles 3D a été extrait dans ModelLoader::load()
// (exécuté sur le thread de chargement, voir loadResources()).
// ---------------------------------------------------------------------------

void Game::adoptLoadedEntities() {
    // Vues non-propriétaires vers les entités possédées par le ModelLoader.
    m_humanEntity     = m_modelLoader->getHumanEntity();
    m_firstPersonArms = m_modelLoader->getFirstPersonArms();

    // Transmet les vues à la Scene (qui les utilise pour update/draw).
    m_scene->adoptEntities(m_humanEntity,
                           m_modelLoader->getCharacterAnim());

    // Le MultiplayerManager lit l'animation du modèle local pour la diffuser.
    if (m_multiplayerManager) {
        m_multiplayerManager->setLocalHuman(m_humanEntity);
    }
}

void Game::stop() {
    glfwSetWindowShouldClose(m_window->getGLFWwindow(), GLFW_TRUE);
}
