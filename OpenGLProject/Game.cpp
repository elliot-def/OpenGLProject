#include "Game.h"
#include "config.h"
#include <vector>


Game::Game() {
    if (!glfwInit()) {
        throw std::runtime_error("Impossible d'initialiser GLFW");
    }
    initialize();
}

Game::~Game() {
    m_socket->stop();
    glfwTerminate(); // pas besoin de delete, les unique_ptr nettoient tout seuls
}

void Game::initialize() {
    m_soundManager   = std::make_unique<SoundManager>();
    m_window         = std::make_unique<Window>(m_soundManager.get());
    m_renderer       = std::make_unique<Renderer>();
    m_camera         = std::make_unique<Camera>();
    m_socket         = std::make_unique<Socket>();
    m_textureManager = std::make_unique<TextureManager>();
    m_shaderManager  = std::make_unique<ShaderManager>(m_camera.get());
    m_player         = std::make_unique<Player>(m_renderer.get());
    m_lightManager   = std::make_unique<LightManager>(m_renderer.get(), m_player.get());
    m_textRenderers  = std::make_unique<std::vector<std::unique_ptr<TextRenderer>>>();
    m_menuManager    = std::make_unique<MenuManager>(this, m_soundManager.get(), m_renderer.get(), m_textRenderers.get(), m_textureManager.get(), m_shaderManager.get());
    m_inputManager   = std::make_unique<InputManager>(this, m_menuManager.get(), m_window.get(), m_player.get());

    m_menuManager->setInputManager(m_inputManager.get());

    m_textRenderers->emplace_back(std::make_unique<TextRenderer>(m_shaderManager.get()));
    m_textRenderers->emplace_back(std::make_unique<TextRenderer>(m_shaderManager.get()));

	m_textRenderers->at(0)->loadFont("res/fonts/armana/Amarna-Bold.ttf", 96.0f);
    m_textRenderers->at(1)->loadFont("res/fonts/Gnocchi.ttf", 282.0f);

    m_socket->connectToServerAsync(ServerInfo(Constants::SERVER_IP, Constants::SERVER_PORT));

    Texture* containerTexture = m_textureManager->getTexture("container");

    Shader* cubeShader = m_shaderManager->getShader("cube/severallights");
    Shader* lightShader    = m_shaderManager->getShader("cube/lightsource");

	std::vector<Texture*> crateTextures = { containerTexture };

	m_soundManager->setMasterVolume(Constants::DEFAULT_MASTER_VOLUME); // Volume maître à 20%

    // Lumière 1 - Rouge forte
    m_lightManager->addPointLight(new LightSource(
        glm::vec3(1, 0.5, 2),            // position
        lightShader,
        m_player.get(),
        glm::vec3(0.2f, 0.0f, 0.0f),     // ambient rouge
        glm::vec3(1.0f, 0.0f, 0.0f),     // diffuse ROUGE INTENSE
        glm::vec3(1.0f, 1.0f, 1.0f),     // specular
        1.0f,                             // constant
        0.09f,                            // linear (portée ~50 unités)
        0.032f,                           // quadratic
        glm::vec3(5.0f, 0.0f, 0.0f)      // lightColor
    ));

    // Lumière 2 - Verte forte
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

	m_modelEntity = new ModelEntity(m_camera.get(), m_lightManager.get(), m_renderer.get(), "./res/models/backpack/backpack.obj", m_textureManager.get());

    glGetString(GL_VERSION) ? std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl
        : throw std::runtime_error("Impossible de récupérer la version OpenGL");


}

void Game::run() {
    while (!m_window->getShouldClose()) {
        m_renderer->handleFrameTiming();
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
            throw std::runtime_error("État du jeu inconnu");
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


    m_cubes.at(2).get()->getTransformation()->rotate(glm::vec3(1, 0, 0), 10.0f * static_cast<float>(fmod(m_renderer->getDeltaTime(), 360.0)));
    for (auto& cube : m_cubes) {
        cube->update();
    }
    for (auto& alphacube : m_alphacubes) {
        alphacube->update();
    }
    m_lightManager->update();

    m_soundManager->setListenerTransform(m_camera->getPosition(), m_camera->getFront(), m_camera->getUp());
    m_soundManager->update();
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
}

void Game::changeState(GameState newState) {
    m_menuManager->changeState(newState);
    switch (newState) {
    case STATE_MENU:
    case STATE_OPTIONS:
        m_inputManager->setContext(InputContext::MENU);
		m_window->setCursorCaptured(false);
        break;
    case STATE_PLAYING:
        m_inputManager->setContext(InputContext::GAME);
        m_window->setCursorCaptured(true);
        break;
    case STATE_PAUSED:
        m_inputManager->setContext(InputContext::PAUSED);
        m_window->setCursorCaptured(false);
        break;
    }
}

void Game::stop() {
    glfwSetWindowShouldClose(m_window->getGLFWwindow(), GLFW_TRUE);
}
