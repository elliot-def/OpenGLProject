#include "Scene.h"

#include "Mesh.h"
#include "Cube.h"
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
#include "NPC.h"
#include "DialogManager.h"
#include "Renderer.h"
#include "Log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

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
}

Scene::~Scene() = default;

// ── Construction du monde ────────────────────────────────────────────────────

void Scene::loadLights() {
    Shader* lightShader = m_shaderManager->getShader("cube/lightsource");

    m_lightManager->addPointLight(new LightSource(
        glm::vec3(1, 0.5, 2), lightShader, m_player,
        glm::vec3(0.2f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(5.0f, 0.0f, 0.0f)));

    m_lightManager->addPointLight(new LightSource(
        glm::vec3(3, 0.5, -2), lightShader, m_player,
        glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(0.0f, 5.0f, 0.0f)));
}

void Scene::loadCubes() {
    Texture* containerTexture = m_textureManager->getTexture("container");
    Shader*  cubeShader       = m_shaderManager->getShader("cube/severallights");
    std::vector<Texture*> crateTextures = { containerTexture };

    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0, 0),  1.0f, cubeShader, crateTextures, m_renderer, m_lightManager, m_player));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, 0, -2), 1.0f, cubeShader, crateTextures, m_renderer, m_lightManager, m_player));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(1, 0.5, 2), 1.0f, cubeShader, crateTextures, m_renderer, m_lightManager, m_player));
    m_cubes[2]->setSpin(10.0f, glm::vec3(1.0f, 0.0f, 0.0f));
    m_cubes.push_back(std::make_unique<Cube>(glm::vec3(0, -12, 0), 24.0f, cubeShader, crateTextures, m_renderer, m_lightManager, m_player));

    m_collisionManager->addStaticMesh(m_cubes[0]->getMesh(), m_cubes[0]->getTransformation()->getMatrix(), "cube1");
    m_collisionManager->addStaticMesh(m_cubes[1]->getMesh(), m_cubes[1]->getTransformation()->getMatrix(), "cube2");
    m_collisionManager->addStaticMesh(m_cubes[3]->getMesh(), m_cubes[3]->getTransformation()->getMatrix(), "cube4");
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

void Scene::adoptNPC(ModelEntity* npcEntity) {
    m_npcEntity = npcEntity;
    m_npcDirty = true;

    // Configurer le dialog du PNJ (si c'est bien un NPC)
    NPC* npc = dynamic_cast<NPC*>(npcEntity);
    if (npc) {
        npc->setDialog(DialogTree::createExample());
        LOG_INFO("[Scene] Dialog du PNJ configure (%zu noeuds)",
                 npc->getDialog().getRoot() ? 1 : 0);
    }
}

// ── Mise à jour du monde ────────────────────────────────────────────────────

void Scene::update(float deltaTime) {
    for (auto& cube : m_cubes) {
        cube->update();
    }
    for (auto& alphacube : m_alphacubes) {
        alphacube->update();
    }

    // Collision dynamique du cube tournant (cube3)
    if (m_cubes.size() > 2) {
        m_collisionManager->updateDynamic("cube3",
            { m_cubes[2]->getMesh() },
            m_cubes[2]->getTransformation()->getMatrix());
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
        if (m_characterAnim) {
            if (m_player->isGravityEnabled()) {
                m_characterAnim->update(m_player->getPosition(), deltaTime,
                                        m_player->getIsSprinting(),
                                        m_collisionManager->getIsPlayerGrounded());
            } else {
                const int idle = m_humanEntity->getIdleAnimIndex();
                if (idle >= 0) m_humanEntity->playAnimation(idle, true);
                m_humanEntity->updateAnimation(deltaTime);
            }
        }
    }

    // ── PNJ ───────────────────────────────────────────────────────────────
    if (m_npcEntity) {
        // Animation idle en continu
        m_npcEntity->updateAnimation(deltaTime);
        m_npcEntity->update();

        // Si dialog actif, faire regarder le PNJ vers le joueur
        if (m_dialogManager.isActive()) {
            NPC* npc = dynamic_cast<NPC*>(m_npcEntity);
            if (npc) {
                npc->lookAt(m_player->getPosition());
            }
        }

        // Collision dynamique du PNJ
        const glm::mat4 mat = m_npcEntity->getModelMatrix();
        if (m_npcDirty || mat != m_lastNPCMatrix) {
            m_collisionManager->updateDynamic("npc", m_npcEntity->getMeshes(), mat);
            m_lastNPCMatrix = mat;
            m_npcDirty = false;
        }
    }

    // Mise à jour du dialog manager (effet machine à écrire)
    if (m_dialogManager.isActive()) {
        m_dialogManager.update(deltaTime);
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

    for (auto& cube : m_cubes) {
        cube->draw();
    }
    m_lightManager->draw();

    if (m_modelShader) {
        if (m_modelEntity) m_modelEntity->draw(m_modelShader);
        if (m_fropyEntity) m_fropyEntity->draw(m_modelShader);
    }

    // Humain 3ème personne (visible uniquement en vue 3P)
    if (m_humanEntity && m_skinnedShader && m_player->isThirdPerson()) {
        m_humanEntity->draw(m_skinnedShader);
    }

    // PNJ (toujours visible, skinned si riggé)
    if (m_npcEntity && m_skinnedShader) {
        m_npcEntity->draw(m_skinnedShader);
    }

    // 2. Transparences
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Tri des cubes transparents du plus loin au plus proche
    std::sort(m_alphacubes.begin(), m_alphacubes.end(),
        [this](const std::unique_ptr<Cube>& a, const std::unique_ptr<Cube>& b) {
            const float da = glm::length(m_camera->getPosition() - a->getCenter());
            const float db = glm::length(m_camera->getPosition() - b->getCenter());
            return da > db; // plus loin d'abord
        });

    for (auto& alphacube : m_alphacubes) {
        alphacube->draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Vue 1ère personne : corps Mixamo (BRAS uniquement, le torse et les
    // jambes sont masqués pour ne pas interférer avec la caméra).
    if (!m_player->isThirdPerson() && m_humanEntity && m_skinnedShader &&
        !m_humanEntity->getMeshes().empty()) {
        m_humanEntity->drawFirstPerson(m_skinnedShader);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Interaction PNJ / Dialog
// ─────────────────────────────────────────────────────────────────────────────

void Scene::tryInteract() {
    // Si déjà en dialog, avancer ou fermer
    if (m_dialogManager.isActive()) {
        m_dialogManager.advance();
        return;
    }

    // Chercher un PNJ à proximité que le joueur regarde
    if (!m_npcEntity || !m_camera) return;

    NPC* npc = dynamic_cast<NPC*>(m_npcEntity);
    if (!npc || !npc->hasDialog()) return;

    float distance = npc->isPlayerLookingAt(m_camera);
    if (distance > 0.0f) {
        m_dialogManager.startDialog(npc);
        LOG_INFO("[Scene] Dialog commence avec le PNJ (distance=%.1f)", distance);
    }
}

void Scene::handleDialogChoice(int index) {
    if (m_dialogManager.isActive()) {
        m_dialogManager.handleChoice(index);
    }
}

void Scene::advanceDialog() {
    if (m_dialogManager.isActive()) {
        m_dialogManager.advance();
    }
}

void Scene::cancelDialog() {
    if (m_dialogManager.isActive()) {
        m_dialogManager.cancelDialog();
        LOG_INFO("[Scene] Dialog annule");
    }
}

bool Scene::isDialogActive() const {
    return m_dialogManager.isActive();
}

bool Scene::isNPCInSight() const {
    if (m_dialogManager.isActive()) return false;
    if (!m_npcEntity || !m_camera) return false;

    NPC* npc = dynamic_cast<NPC*>(m_npcEntity);
    if (!npc) return false;

    return npc->isPlayerLookingAt(m_camera) > 0.0f;
}

void Scene::renderDialog(TextRenderer* renderer, int screenW, int screenH) const {
    m_dialogManager.render(renderer, screenW, screenH);
}
