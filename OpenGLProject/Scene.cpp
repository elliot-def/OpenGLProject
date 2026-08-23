#include "Scene.h"

#include "Mesh.h"
#include "Cube.h"
#include "CubeRenderer.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "Texture.h"
#include "TextureManager.h"
#include "LightSource.h"
#include "LightManager.h"
#include "Player.h"
#include "Camera.h"
#include "CollisionManager.h"
#include "ModelEntity.h"
#include "Skybox.h"
#include "CharacterAnimationController.h"
#include "Renderer.h"
#include "Log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

Scene::Scene(Camera* camera, Player* player, CollisionManager* collisionManager,
             LightManager* lightManager, Renderer* renderer, ShaderManager* shaderManager,
             TextureManager* textureManager, InputManager* inputManager)
    : m_camera(camera), m_player(player), m_collisionManager(collisionManager),
      m_lightManager(lightManager), m_renderer(renderer), m_shaderManager(shaderManager),
      m_textureManager(textureManager), m_inputManager(inputManager) {
    // Shaders cachés UNE SEULE FOIS ici (pas de getShader() par frame dans le
    // rendu). Le ShaderManager est déjà construit quand Scene est créée.
    m_skyboxShader  = m_shaderManager->getShader("skybox");
    m_modelShader   = m_shaderManager->getShader("model");
    m_skinnedShader = m_shaderManager->getShader("skinned");

    // Rendu instancié des cubes : cube unitaire partagé (edge=1, origine) +
    // lots par shader. Le contexte GL est courant ici (loadResources, thread
    // principal), donc la création du VAO/VBO/EBO du cube unitaire est sûre.
    m_cubeRenderer = std::make_unique<CubeRenderer>();
}

Scene::~Scene() = default;

// ── Construction du monde ────────────────────────────────────────────────────

void Scene::loadLights() {
    Shader* lightShader = m_shaderManager->getShader("cube/lightsource");

    m_lightManager->addPointLight(new LightSource(
        glm::vec3(1, 0.5, 2), lightShader,
        glm::vec3(0.2f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(5.0f, 0.0f, 0.0f)));

    m_lightManager->addPointLight(new LightSource(
        glm::vec3(3, 0.5, -2), lightShader,
        glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(0.0f, 5.0f, 0.0f)));
}

void Scene::loadCubes() {
    Texture* containerTexture = m_textureManager->getTexture("container");
    Shader*  cubeShader       = m_shaderManager->getShader("cube/severallights");
    std::vector<Texture*> crateTextures = { containerTexture };

    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0, 0),  1.0f, cubeShader, crateTextures, m_renderer));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, 0, -2), 1.0f, cubeShader, crateTextures, m_renderer));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0.5, 2), 1.0f, cubeShader, crateTextures, m_renderer));
    m_cubes[2]->setSpin(10.0f, glm::vec3(1.0f, 0.0f, 0.0f));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, -12, 0), 24.0f, cubeShader, crateTextures, m_renderer));

    // Collisions statiques : AABB locale du cube unitaire (±0.5) transformée
    // par la matrice modèle (translate·scale) → world AABB = centre ± edge/2,
    // identique à l'ancien mesh par-cube. Le cube unitaire est partagé.
    Mesh* unitCube = m_cubeRenderer->getUnitCubeMesh();
    m_collisionManager->addStaticMesh(unitCube, m_cubes[0]->getModelMatrix(), "cube1");
    m_collisionManager->addStaticMesh(unitCube, m_cubes[1]->getModelMatrix(), "cube2");
    m_collisionManager->addStaticMesh(unitCube, m_cubes[3]->getModelMatrix(), "cube4");
    m_collisionManager->buildBVH();
}

void Scene::createSkybox(const char* path) {
    m_skybox = std::make_unique<Skybox>(path);
}

void Scene::adoptEntities(ModelEntity* modelEntity, ModelEntity* fropyEntity,
                          ModelEntity* humanEntity, CharacterAnimationController* characterAnim) {
    m_modelEntity    = modelEntity;
    m_fropyEntity    = fropyEntity;
    m_humanEntity    = humanEntity;
    m_characterAnim  = characterAnim;
}

// ── Mise à jour du monde ────────────────────────────────────────────────────

void Scene::update(float deltaTime) {
    for (auto& cube : m_cubes) {
        cube->update();
    }

    // Collision dynamique du cube tournant (cube3) : l'OBB est recomputée à
    // partir de l'AABB locale du cube unitaire + la matrice modèle (rotation
    // incluse), donc la hitbox suit fidèlement le spin.
    if (m_cubes.size() > 2) {
        m_collisionManager->updateDynamic("cube3",
            { m_cubeRenderer->getUnitCubeMesh() },
            m_cubes[2]->getModelMatrix());
    }

    m_player->update();
    m_camera->update(m_player);
    m_lightManager->update();

    // Collisions dynamiques : AABB world recalculée uniquement si la matrice
    // modèle de l'entité a changé (dirty-flag).
    if (m_modelEntity) {
        const glm::mat4 mat = m_modelEntity->getModelMatrix();
        if (m_backpackDirty || mat != m_lastBackpackMatrix) {
            m_collisionManager->updateDynamic("backpack", m_modelEntity->getMeshes(), mat);
            m_lastBackpackMatrix = mat;
            m_backpackDirty = false;
        }
    }
    if (m_fropyEntity) {
        const glm::mat4 mat = m_fropyEntity->getModelMatrix();
        if (m_fropyDirty || mat != m_lastFropyMatrix) {
            m_collisionManager->updateDynamic("fropy_low_poly", m_fropyEntity->getMeshes(), mat);
            m_lastFropyMatrix = mat;
            m_fropyDirty = false;
        }
    }
    if (m_fropyEntity) {
        m_fropyEntity->update();
    }

    // Humain 3ème personne : suit la position et la direction du joueur.
    if (m_humanEntity) {
        m_humanEntity->setPosition(m_player->getPosition());
        m_humanEntity->setDirection(*m_player->getDirection());

        // Animation du personnage 3P : deleguee au CharacterAnimationController
        // Sauf en no-clip (gravite desactivee) → on force l'idle.
        const bool noClip = !m_player->isGravityEnabled();
        if (m_characterAnim) {
            if (!noClip) {
                // Sortie de no-clip : resynchroniser la machine a etats
                // (m_lastPos/m_lastYaw perimes -> faux saut/turn/fall qui
                // laissaient la camera 1P dans une pose incorrecte).
                if (m_wasNoClip) {
                    m_characterAnim->resetState(
                        m_player->getPosition(),
                        static_cast<float>(m_humanEntity->getDirection()->getYaw()));
                }
                m_characterAnim->update(m_player->getPosition(), deltaTime,
                                        m_player->getIsSprinting(),
                                        m_collisionManager->getIsPlayerGrounded());
                // Transmettre le facteur de vitesse post-atterrissage au joueur
                m_player->setPostLandSpeedFactor(m_characterAnim->getPostLandSpeedFactor());
            } else {
                // Entree en no-clip : forcer l'idle UNE SEULE FOIS. L'appeler
                // chaque frame remettait m_currentTime a 0 et gelait l'animation
                // sur sa premiere frame (pose de transition) au lieu de la
                // laisser atteindre sa pose neutre.
                if (!m_wasNoClip) {
                    const int idle = m_humanEntity->getIdleAnimIndex();
                    if (idle >= 0) m_humanEntity->playAnimation(idle, true);
                }
                m_humanEntity->updateAnimation(deltaTime);
            }
        }
        m_wasNoClip = noClip;
    }
}

// ── Rendu du monde ──────────────────────────────────────────────────────────

void Scene::draw() {
    // 0. Ciel : rendu en premier (depth LEQUAL + mask off), la géométrie de
    // la scène le recouvre ensuite naturellement grâce au depth test.
    if (m_skybox && m_skyboxShader) {
        m_skybox->draw(m_skyboxShader, m_camera);
    }

    // 1. Opaques
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Cubes + lumières : rendu INSTANCIÉ. Un seul glDrawElementsInstanced par
    // shader (severallights pour les cubes texturés, lightsource pour les
    // cubes de lumière), au lieu d'un draw call + set d'uniforms par cube.
    m_cubeRenderer->clear();
    for (auto& cube : m_cubes) {
        m_cubeRenderer->submit(cube->getShader(), cube->getTextures(), cube->getModelMatrix());
    }
    for (auto* light : m_lightManager->getLightSources()) {
        Cube* lightCube = light->getCube().get();
        m_cubeRenderer->submit(lightCube->getShader(), {}, lightCube->getModelMatrix(),
                               light->getLightColor());
    }
    m_cubeRenderer->draw(m_camera, m_lightManager);

    if (m_modelShader) {
        if (m_modelEntity) m_modelEntity->draw(m_modelShader);
        if (m_fropyEntity) m_fropyEntity->draw(m_modelShader);
    }

    // Humain 3ème personne (visible uniquement en vue 3P)
    if (m_humanEntity && m_skinnedShader && m_player->isThirdPerson()) {
        m_humanEntity->draw(m_skinnedShader);
    }

    // Vue 1ère personne : corps Mixamo (BRAS uniquement, le torse et les
    // jambes sont masqués pour ne pas interférer avec la caméra).
    // Alpha blending actif : le shader applique un fade progressif base sur
    // la distance a la camera (smoothstep dans skinned.frag, uniform
    // uNearFadeStart/End). Depth write reste actif (contrairement aux
    // transparences standards) pour que les bras occultent correctement
    // le monde et soient occultes par les murs.
    if (!m_player->isThirdPerson() && m_humanEntity && m_skinnedShader &&
        !m_humanEntity->getMeshes().empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_humanEntity->drawFirstPerson(m_skinnedShader);
        glDisable(GL_BLEND);
    }
}
