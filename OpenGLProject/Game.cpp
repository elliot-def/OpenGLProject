// APIENTRY / WIN32_LEAN_AND_MEAN doivent etre forces AVANT tout header projet /
// systeme pour eviter C4005 (glad.h vs minwindef.h) et la pollution macros.
// Voir OpenGLProject/win_compat.h pour la justification detaillee.
#include "win_compat.h"

#include "Game.h"
#include "config.h"

#include <glad/glad.h>  // GL_TRUE/GL_FALSE/glDepthMask - utilise dans glDepthMask pour l'outline/transparents
#include <vector>


Game::Game() {
    if (!glfwInit()) {
        throw std::runtime_error("Impossible d'initialiser GLFW");
    }
    initialize();
}

Game::~Game() {
	SharedQuad::destroy();
    m_socket->stop();
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
    m_lightManager      = std::make_unique<LightManager>(m_renderer.get(), m_player.get());
    m_textRenderers     = std::make_unique<std::vector<std::unique_ptr<TextRenderer>>>();
    m_menuManager       = std::make_unique<MenuManager>(this, m_soundManager.get(), m_renderer.get(), m_textRenderers.get(), m_textureManager.get(), m_shaderManager.get(), m_cursorManager.get());
    m_inputManager      = std::make_unique<InputManager>(this, m_menuManager.get(), m_window.get(), m_player.get());

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

	// m_soundManager->setMasterVolume(Constants::DEFAULT_MASTER_VOLUME); // Volume ma�tre � 20%

    // Lumi�re 1 - Rouge forte
    /*
    m_lightManager->addPointLight(new LightSource(
        glm::vec3(1, 0.5, 2),            // position
        lightShader,
        m_player.get(),
        glm::vec3(0.2f, 0.0f, 0.0f),     // ambient rouge
        glm::vec3(1.0f, 0.0f, 0.0f),     // diffuse ROUGE INTENSE
        glm::vec3(1.0f, 1.0f, 1.0f),     // specular
        1.0f,                             // constant
        0.09f,                            // linear (port�e ~50 unit�s)
        0.032f,                           // quadratic
        glm::vec3(5.0f, 0.0f, 0.0f)      // lightColor
    ));

    // Lumi�re 2 - Verte forte
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
    */
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0, 0), 1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, 0, -2), 1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0.5, 2), 1.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, -12, 0), 24.0f, cubeShader, crateTextures, m_renderer.get(), m_lightManager.get(), m_player.get()));

	m_modelEntity = new ModelEntity(m_camera.get(), m_lightManager.get(), m_renderer.get(), "./res/models/backpack/backpack.obj", m_textureManager.get());

    // ── Câblage des avant-bras riggés (first-person) ─────────────────────────────────
    // Charge le mesh skinned depuis l'asset que l'utilisateur vient de deployer dans
    // res/rigging/arm/. On tente .glb d'abord (binaire compact), puis .fbx en fallback.
    // Si le chargement echoue (fichier absent / format invalide), on log + on continue
    // le jeu sans bras — pas de plantage.
    {
        std::vector<std::string> candidates = {
            "./res/rigging/arm/arms_rig.glb",
            "./res/rigging/arm/arms_rig.fbx"
        };
        for (const auto& path : candidates) {
            try {
                m_armsModel = std::make_unique<Model>(m_camera.get(), m_lightManager.get(), path, m_textureManager.get());
                if (m_armsModel->getScene() == nullptr || m_armsModel->getMeshes().empty()) {
                    std::cerr << "[Game] Asset skinned invalide (no scene/meshes) : " << path << std::endl;
                    m_armsModel.reset();
                    continue;
                }
                m_armsAnimator = std::make_unique<Animator>();
                m_armsAnimator->setup(m_armsModel->getRootNode(),
                                       m_armsModel->getBoneInfoMap(),
                                       m_armsModel->getAnimations());
                std::cout << "[Game] Avant-bras rigues charges depuis " << path << std::endl;
                std::cout << "       bones=" << m_armsAnimator->getBoneCount()
                          << ", anims=" << m_armsModel->getAnimations().size() << std::endl;
                if (!m_armsModel->getAnimations().empty()) {
                    m_armsAnimator->playClip("finger_gun_idle");
                    m_armsAnimator->update(0.0f); // Bake premiere frame : evite un flash T-pose d'1 frame au demarrage.
                    std::cout << "       clip par defaut : " << m_armsModel->getAnimations()[0].name << std::endl;
                }
                break;
            }
            catch (const std::exception& e) {
                std::cerr << "[Game] Echec chargement skinned " << path << " : " << e.what() << std::endl;
                m_armsModel.reset();
                m_armsAnimator.reset();
            }
        }
        if (!m_armsModel) {
            std::cerr << "[Game] Aucun asset skinned chargeable — avant-bras desactives pour cette session." << std::endl;
        }
    }

    // ── Câblage concret de l'outline (silhouette) ─────────────────────────────
    // Charge le shader dédié et l'applique sur chaque entité 3D pour validation
    // visuelle. Le ShaderManager étant owned par Game, le shader vit pour toute
    // la durée du programme — pas de risque de dangling pour setOutlineShader.
    // Si le shader "outline" est introuvable (ex : ShaderManager n'a pas pu le
    // compiler), on log + on skip l'outline plutôt que de planter l'init du jeu.
    Shader* outlineShader = nullptr;
    try {
        outlineShader = m_shaderManager->getShader("outline");
    }
    catch (const std::out_of_range& e) {
        std::cerr << "[Game] Outline shader indisponible : " << e.what()
                  << " — outline desactive pour cette session." << std::endl;
    }
    if (outlineShader) {
        for (auto& cube : m_cubes) {
            cube->setOutlineShader(outlineShader);
            cube->setOutlineEnabled(true);
        }
        for (auto& acube : m_alphacubes) {
            acube->setOutlineShader(outlineShader);
            acube->setOutlineEnabled(true);
        }
        m_modelEntity->setOutlineShader(outlineShader);
        m_modelEntity->setOutlineEnabled(true);
    }

    // D�cor statique � une seule fois
    m_collisionManager->addStaticMesh(m_cubes[0]->getMesh(), m_cubes[0]->getTransformation()->getMatrix(), "cube1");
    m_collisionManager->addStaticMesh(m_cubes[1]->getMesh(), m_cubes[1]->getTransformation()->getMatrix(), "cube2");
    //m_collisionManager->addStaticMesh(m_cubes[2]->getMesh(), m_cubes[2]->getTransformation()->getMatrix(), "cube3");
    m_collisionManager->addStaticMesh(m_cubes[3]->getMesh(), m_cubes[3]->getTransformation()->getMatrix(), "cube4");

    m_collisionManager->buildBVH();

    glGetString(GL_VERSION) ? std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl
        : throw std::runtime_error("Impossible de r�cup�rer la version OpenGL");


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
            throw std::runtime_error("�tat du jeu inconnu");
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
    
    // m_collisionManager->updateDynamic("cube1", { m_cubes[0]->getMesh() }, m_cubes[0]->getTransformation()->getMatrix());
    // m_collisionManager->updateDynamic("cube2", { m_cubes[1]->getMesh() }, m_cubes[1]->getTransformation()->getMatrix());
    m_collisionManager->updateDynamic("cube3", { m_cubes[2]->getMesh() }, m_cubes[2]->getTransformation()->getMatrix());

    m_player->update();

    m_lightManager->update();

    m_soundManager->setListenerTransform(m_camera->getPosition(), m_camera->getFront(), m_camera->getUp());
    m_soundManager->update();

    m_collisionManager->updateDynamic("backpack", m_modelEntity->getModel()->getMeshes(), m_modelEntity->getModelMatrix());

    // Tick de l'animation des avant-bras rigues : avance la timeline du clip
    // courant. Si m_armsAnimator est nullptr (asset jamais charge), no-op.
    if (m_armsAnimator) {
        m_armsAnimator->update(static_cast<float>(m_renderer->getDeltaTime()));
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

    // 3. Avant-bras first-person : delegation complete au helper dedie.
    // Toute la math (skinned shader + uBoneMatrices + matrice camera-relative
    // + clear depth + lumieres du decor) est encapsulee dans ArmsRenderer::drawFP.
    // Le glDisable(GL_DEPTH_TEST) + glClear(GL_DEPTH_BUFFER_BIT) sont deja faits
    // a l'interieur de drawFP() — pas de duplication ici (la redondance passait
    // le depth buffer en etat incoherent entre les deux appels).
    ArmsRenderer::drawFP(m_camera.get(),
                         m_shaderManager.get(),
                         m_lightManager.get(),
                         m_armsModel.get(),
                         m_armsAnimator.get());
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
