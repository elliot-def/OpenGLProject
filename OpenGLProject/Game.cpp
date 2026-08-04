#include "win_compat.h"

#include "Game.h"
#include "config.h"
#include "SteamManager.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
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
	SharedQuad::destroy();
    delete m_humanEntity;
    delete m_fropyEntity;
    delete m_modelEntity;
    m_socket->stop();
    if (m_steamManager) {
        m_steamManager->shutdown();
    }
    glfwTerminate(); // pas besoin de delete, les unique_ptr nettoient tout seuls
}

void Game::initialize() {
    m_window            = std::make_unique<Window>();
    m_renderer          = std::make_unique<Renderer>();
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

	m_textRenderers->at(0)->loadFont("res/fonts/armana/Amarna-Bold.ttf", 96.0f);
    m_textRenderers->at(1)->loadFont("res/fonts/Gnocchi.ttf", 282.0f);

    m_socket->connectToServerAsync(ServerInfo());

    Texture* containerTexture = m_textureManager->getTexture("container");

    Shader* cubeShader = m_shaderManager->getShader("cube/severallights");
    Shader* lightShader    = m_shaderManager->getShader("cube/lightsource");

	std::vector<Texture*> crateTextures = { containerTexture };

    // Lumiere 1 - Rouge forte
    
    m_lightManager->addPointLight(new LightSource(
        glm::vec3(1, 0.5, 2),            // position
        lightShader,
        m_player.get(),
        glm::vec3(0.2f, 0.0f, 0.0f),     // ambient rouge
        glm::vec3(1.0f, 0.0f, 0.0f),     // diffuse ROUGE INTENSE
        glm::vec3(1.0f, 1.0f, 1.0f),     // specular
        1.0f,                             // constant
        0.09f,                            // linear (porte ~50 units)
        0.032f,                           // quadratic
        glm::vec3(5.0f, 0.0f, 0.0f)      // lightColor
    ));

    // Lumiere 2 - Verte forte
    m_lightManager->addPointLight(new LightSource(
        glm::vec3(3, 0.5, -2),           // position
        lightShader,
        m_player.get(),
        glm::vec3(0.0f, 0.2f, 0.0f),     // ambient vert
        glm::vec3(0.0f, 1.0f, 0.0f),     // diffuse VERT INTENSE
        glm::vec3(1.0f, 1.0f, 1.0f),     // specular
        1.0f,                            // constant
        0.09f,                           // linear
        0.032f,                          // quadratic
        glm::vec3(0.0f, 5.0f, 0.0f)      // lightColor
    ));
    
    
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0, 0), 1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, 0, -2), 1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0.5, 2), 1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes[2]->setSpin(10.0f, glm::vec3(1.0f, 0.0f, 0.0f));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, -12, 0), 24.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    
    m_modelEntity = new ModelEntity(m_camera.get(), m_lightManager.get(), m_renderer.get(), "./res/models/backpack/backpack.obj", m_textureManager.get());

    // Fropy — modèle décoratif
    m_fropyEntity = new ModelEntity(m_camera.get(), m_lightManager.get(), m_renderer.get(), "./res/models/fropy/fropy.obj", m_textureManager.get());
    m_fropyEntity->setPosition(glm::vec3(3.0f, 5.0f, 0.0f));
    m_fropyEntity->setSpin(20.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    // Bras en premiere personne
    // .glb: texture embarquee, autosuffisant (pas de chemins absolus casses
    // comme le .fbx qui pointait vers G:\Models\ps2\...). 52 joints = MAX_BONES.
    m_firstPersonArms = std::make_unique<FirstPersonArms>(m_camera.get(), m_lightManager.get(), 
                                             "./res/rigging/arm/arms_rig.glb", m_textureManager.get());
    // Branche le clic gauche sur l'animation de tir des bras
    m_inputManager->setFirstPersonArms(m_firstPersonArms.get());

    // Modèle humain (3ème personne) — chargé en bind pose, sans animation pour le moment
    m_humanEntity = new ModelEntity(m_camera.get(), m_lightManager.get(), m_renderer.get(),
                                    "./res/rigging/human/human_1.glb", m_textureManager.get());

    // Decor statique, une seule fois
    m_collisionManager->addStaticMesh(m_cubes[0]->getMesh(), m_cubes[0]->getTransformation()->getMatrix(), "cube1");
    m_collisionManager->addStaticMesh(m_cubes[1]->getMesh(), m_cubes[1]->getTransformation()->getMatrix(), "cube2");
    //m_collisionManager->addStaticMesh(m_cubes[2]->getMesh(), m_cubes[2]->getTransformation()->getMatrix(), "cube3");
    m_collisionManager->addStaticMesh(m_cubes[3]->getMesh(), m_cubes[3]->getTransformation()->getMatrix(), "cube4");

    m_collisionManager->buildBVH();

    // ---- Steam - Initialisation ----
    m_steamManager = std::make_unique<SteamManager>();
    if (m_steamManager->init()) {
        // Configurer les callbacks AVANT parseCommandLine
        m_steamManager->setOnInviteReceived([this](CSteamID lobbyID) {
            printf("[Game] Invitation reçue, tentative de rejoindre le lobby %llu...\n",
                   lobbyID.ConvertToUint64());
            m_steamManager->joinLobby(lobbyID);
        });

        m_steamManager->setOnLobbyCreated([this](CSteamID lobbyID) {
            printf("[Game] Lobby créé avec succès, ouverture de l'invitation...\n");
            m_steamManager->openInviteDialog();
        });

        m_steamManager->setOnLobbyEntered([this](CSteamID lobbyID) {
            printf("[Game] Connecté au lobby %llu !\n", lobbyID.ConvertToUint64());
            if (m_socket) {
                // TODO: utiliser le lobby pour établir la connexion réseau
                // (par exemple via SetLobbyGameServer)
            }
        });

        m_steamManager->setOnLobbyLeft([this]() {
            printf("[Game] Quitté le lobby.\n");
        });

        // Vérifier les invitations passées en ligne de commande
        if (m_argc > 0 && m_argv != nullptr) {
            m_steamManager->parseCommandLine(m_argc, m_argv);
        }
    } else {
        printf("[Game] Steam non disponible, fonctionnement hors-ligne.\n");
    }

    glGetString(GL_VERSION) ? std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl
        : throw std::runtime_error("Impossible de r�cup�rer la version OpenGL");


}

void Game::run() {
    while (!m_window->getShouldClose()) {
        m_renderer->handleFrameTiming();

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
    m_collisionManager->updateDynamic("fropy", m_fropyEntity->getModel()->getMeshes(), m_fropyEntity->getModelMatrix());
    m_fropyEntity->update();

    // Humain 3ème personne : suit la position et la direction du joueur.
    // PAS de collision dynamique : le modèle est l'avatar du joueur, il ne doit
    // pas se collisionner lui-même (éjection infinie du joueur hors de la map).
    if (m_humanEntity) {
        m_humanEntity->setPosition(m_player->getPosition());
        m_humanEntity->setDirection(*m_player->getDirection());
    }

    // Mise à jour du bobbing des bras en première personne
    if (m_firstPersonArms) {
        m_firstPersonArms->update(m_renderer->getDeltaTime(), m_player->getPosition(), m_player->getIsSprinting());
    }
}

void Game::draw() {
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
        m_humanEntity->draw(m_shaderManager->getShader("model"));
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
    // En 3P, le modèle humain les remplace.
    if (m_firstPersonArms && !m_player->isThirdPerson()) {
        m_firstPersonArms->draw(m_shaderManager->getShader("skinned"));
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
        sound->play();
        break;
    case STATE_PAUSED:
        m_inputManager->setContext(InputContext::PAUSED);
        m_window->setCursorCaptured(false);
        m_soundManager->applyReverbToAll(ReverbPreset::UNDERWATER);
        break;
    }
}

void Game::stop() {
    glfwSetWindowShouldClose(m_window->getGLFWwindow(), GLFW_TRUE);
}
