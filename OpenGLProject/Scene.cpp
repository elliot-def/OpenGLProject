#include "Scene.h"

#include "Mesh.h"
#include "Cube.h"
#include "CubeRenderer.h"
#include "Door.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "Texture.h"
#include "TextureManager.h"
#include "LightSource.h"
#include "LightManager.h"
#include "Player.h"
#include "Camera.h"
#include "CollisionManager.h"
#include "InputManager.h"
#include "ModelEntity.h"
#include "Skybox.h"
#include "CharacterAnimationController.h"
#include "Renderer.h"
#include "Log.h"

#include "constants/texture.h"
#include "constants/material.h"
#include "constants/door.h"

#include <string>

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
    m_doorShader    = m_shaderManager->getShader("door");
    m_outlineShader = m_shaderManager->getShader("outline");

    // Rendu instancié des cubes de lumière : cube unitaire partagé (edge=1,
    // origine) + lots par shader. Le contexte GL est courant ici
    // (loadResources, thread principal), donc la création du VAO/VBO/EBO du
    // cube unitaire est sûre.
    m_cubeRenderer = std::make_unique<CubeRenderer>();
}

Scene::~Scene() = default;

// ── Construction du monde ────────────────────────────────────────────────────

void Scene::loadLights() {
    Shader* lightShader = m_shaderManager->getShader("cube/lightsource");

    // Indices des lumières (ordre d'ajout) : conservés pour l'affectation
    // aux pièces plus bas (setLightRoom).
    const size_t idxRed = m_lightManager->addPointLight(new LightSource(
        glm::vec3(1, 0.5, 2), lightShader,
        glm::vec3(0.2f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(5.0f, 0.0f, 0.0f)));
    (void)idxRed;   // extérieure (devant maison 1) : reste libre

    const size_t idxGreen = m_lightManager->addPointLight(new LightSource(
        glm::vec3(3, 0.5, -2), lightShader,
        glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(0.0f, 5.0f, 0.0f)));

    // Lumière blanche dédiée aux murs brique : l' incidence rasant rend le
    // relief du normal mapping bien visible. Bornée à la pièce maison 1
    // plus bas (la lumière ne traverse pas les murs).
    const size_t idxWhite = m_lightManager->addPointLight(new LightSource(
        glm::vec3(-1.5f, 2.2f, -1.2f), lightShader,
        glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.5f, 0.5f, 0.5f), 1.0f, 0.09f, 0.032f,
        glm::vec3(1.0f, 1.0f, 1.0f)));

    // ── Pièces intérieures : les 3 maisons de village.json ───────────────
    // Bornes = image du volume intérieur de house.json par la transform de
    // l'include (position + yaw, cf. village.json). Chaque maison a ses
    // propres lumières bornées à SON intérieur : la lumière ne traverse
    // pas les murs ET aucune maison n'est laissée dans le noir.
    const int room1 = m_lightManager->addRoom(glm::vec3(-3.0f,  0.0f, -5.7f),
                                               glm::vec3( 3.0f,  2.5f, -0.1f));   // maison 1 : (0,0,0), yaw 0
    const int room2 = m_lightManager->addRoom(glm::vec3(14.1f,  0.0f, -19.0f),
                                               glm::vec3(19.7f,  2.5f, -13.0f));  // maison 2 : (14,0,-16), yaw -90
    const int room3 = m_lightManager->addRoom(glm::vec3(-19.7f, 0.0f, -19.0f),
                                               glm::vec3(-14.1f, 2.5f, -13.0f));  // maison 3 : (-14,0,-16), yaw 90

    m_lightManager->setLightRoom(idxWhite, room1);
    m_lightManager->setLightRoom(idxGreen, room1);

    // Lumières intérieures des maisons 2 et 3 : images des lumières blanche
    // et verte de la maison 1 par la transform de l'include.
    // maison 2 : (x,z) ↦ (-z, x) + (14,0,-16) ; maison 3 : (x,z) ↦ (z, -x) + (-14,0,-16)
    const size_t idxWhite2 = m_lightManager->addPointLight(new LightSource(
        glm::vec3(15.2f, 2.2f, -17.5f), lightShader,
        glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.5f, 0.5f, 0.5f), 1.0f, 0.09f, 0.032f,
        glm::vec3(1.0f, 1.0f, 1.0f)));
    const size_t idxGreen2 = m_lightManager->addPointLight(new LightSource(
        glm::vec3(16.0f, 0.5f, -13.0f), lightShader,
        glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(0.0f, 5.0f, 0.0f)));
    const size_t idxWhite3 = m_lightManager->addPointLight(new LightSource(
        glm::vec3(-15.2f, 2.2f, -14.5f), lightShader,
        glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.5f, 0.5f, 0.5f), 1.0f, 0.09f, 0.032f,
        glm::vec3(1.0f, 1.0f, 1.0f)));
    const size_t idxGreen3 = m_lightManager->addPointLight(new LightSource(
        glm::vec3(-16.0f, 0.5f, -19.0f), lightShader,
        glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.09f, 0.032f,
        glm::vec3(0.0f, 5.0f, 0.0f)));

    m_lightManager->setLightRoom(idxWhite2, room2);
    m_lightManager->setLightRoom(idxGreen2, room2);
    m_lightManager->setLightRoom(idxWhite3, room3);
    m_lightManager->setLightRoom(idxGreen3, room3);
}

// ── Carte (JSON) ────────────────────────────────────────────────────────────
// Charge la carte (sol, murs, toit à deux pans, portes) depuis un fichier
// JSON via le MapLoader : géométrie à taille réelle (UV en mètres), portes
// interactives (contour + ouverture 90° avec E). Les collisions statiques + BVH
// sont enregistrées par le loader ; les colliders dynamiques des portes sont
// ajoutés ici (dirty-flag dans update()).
void Scene::loadMap(const char* jsonPath) {
    m_wallShader      = m_shaderManager->getShader("cube/wall");
    m_severallightsShader = m_shaderManager->getShader("cube/severallights");
    m_containerTexture = m_textureManager->getTexture("container");
    m_parquetTexture   = m_textureManager->getTexture("parquet");
    m_leafTexture      = m_textureManager->getTexture("leaf");
    m_greyRoofTexture  = m_textureManager->getTexture("grey_roof");
    m_rietTexture      = m_textureManager->getTexture("riet");

    // Textures brickwall en .jpg : chargées directement via stb_image car le
    // TextureManager ne parcourt que les .png (dossier/fichier.png).
    m_wallDiffuse = std::make_unique<Texture>("./res/textures/brickwall/brickwall.jpg",
        Constants::Texture::FIRST_TEXTURE_ID + 100, Constants::Material::STONE, false);
    m_wallNormal  = std::make_unique<Texture>("./res/textures/brickwall/brickwall_normal.jpg",
        Constants::Texture::FIRST_TEXTURE_ID + 101, Constants::Material::STONE, false);

    m_map = std::make_unique<MapLoader>(m_collisionManager);
    if (!m_map->load(jsonPath)) {
        LOG_ERROR("[Scene] Echec de chargement de la carte : %s", jsonPath);
    }

    // Colliders dynamiques des portes : l'AABB suit la rotation via
    // updateDynamic dans update() (même principe que backpack/fropy plus bas).
    m_lastDoorMatrices.clear();
    auto& doors = m_map->getDoors();
    for (size_t i = 0; i < doors.size(); ++i) {
        const std::string key = "door" + std::to_string(i);
        m_collisionManager->addDynamicMesh(key, { doors[i]->getMesh() },
                                           doors[i]->getModelMatrix());
        m_lastDoorMatrices.push_back(doors[i]->getModelMatrix());
    }

    // E (touche Grab, ou RB manette) : ouvrir/fermer la porte visee.
    if (m_inputManager) {
        m_inputManager->setInteractCallback([this]() { tryToggleDoor(); });
    }
}

// Ouvre/ferme la porte interactive la plus proche parmi celles que le joueur
// est en train de regarder (portee + cone de regard, voir Door::canInteract).
void Scene::tryToggleDoor() {
    if (!m_map) return;

    Door* best = nullptr;
    float bestDist = 0.0f;
    for (auto& door : m_map->getDoors()) {
        if (!door->canInteract(m_camera->getPosition(), m_camera->getFront())) {
            continue;
        }
        const float dist = glm::length(door->getCenter() - m_camera->getPosition());
        if (!best || dist < bestDist) {
            best = door.get();
            bestDist = dist;
        }
    }
    if (best) {
        // Le joueur pousse la porte : elle bascule du cote oppose a sa position.
        best->toggle(m_camera->getPosition());
    }
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
    // Portes : glow d'interaction (portee + regard), animation d'ouverture /
    // fermeture, et collider dynamique si la matrice a change (dirty-flag).
    if (m_map) {
        auto& doors = m_map->getDoors();
        for (size_t i = 0; i < doors.size(); ++i) {
            Door* door = doors[i].get();
            door->setHighlighted(door->canInteract(m_camera->getPosition(),
                                                   m_camera->getFront()));
            door->update(deltaTime);

            const glm::mat4 mat = door->getModelMatrix();
            if (mat != m_lastDoorMatrices[i]) {
                m_collisionManager->updateDynamic("door" + std::to_string(i),
                                                  { door->getMesh() }, mat);
                m_lastDoorMatrices[i] = mat;
            }
        }
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

    // Cubes de lumière : rendu INSTANCIÉ (un seul glDrawElementsInstanced par
    // shader), au lieu d'un draw call + set d'uniforms par cube.
    m_cubeRenderer->clear();
    for (auto* light : m_lightManager->getLightSources()) {
        Cube* lightCube = light->getCube().get();
        m_cubeRenderer->submit(lightCube->getShader(), {}, lightCube->getModelMatrix(),
                               light->getLightColor());
    }
    m_cubeRenderer->draw(m_camera, m_lightManager);

    // 2. Carte — matériau brique (murs + pignons du toit) : shader dédié "cube/wall"
    // (TBN reconstruit par dérivées dans le fragment shader). Mêmes uniforms
    // de lumière que severallights, donc LightManager::applyToShader
    // fonctionne tel quel. UV en mètres : la texture se répète 1×/m.
    if (m_map && m_wallShader && m_wallDiffuse && m_wallNormal) {
        m_wallShader->use();
        m_wallShader->setMat4("view", m_camera->getViewMatrix());
        m_wallShader->setMat4("projection", m_wallShader->getProjection());
        m_wallShader->setVec3("viewPos", m_camera->getPosition());
        m_lightManager->applyToShader(m_wallShader);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_wallDiffuse->getID());
        m_wallShader->setInt("material.diffuse", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_wallNormal->getID());
        m_wallShader->setInt("material.normal", 1);
        m_wallShader->setFloat("material.shininess", m_wallDiffuse->getShininess());

        for (const auto& part : m_map->getParts()) {
            if (part.material != MapMaterial::BrickWall) continue;
            m_wallShader->setMat4("model", part.model);
            part.mesh->draw();
        }
    }

    // 3. Carte — matériaux à texture unique (caisses, sol parquet, terre/feuilles,
    // toit, tapis d'entrée)
    // : shader "cube/severallights" en mode mesh unique (useInstanceModel=0,
    // matrice "model" en uniform). 1 passe par matériau (1 seul bind de texture).
    if (m_map && m_severallightsShader) {
        m_severallightsShader->use();
        m_severallightsShader->setFloat("useInstanceModel", 0.0f);
        m_severallightsShader->setMat4("view", m_camera->getViewMatrix());
        m_severallightsShader->setMat4("projection", m_severallightsShader->getProjection());
        m_severallightsShader->setVec3("viewPos", m_camera->getPosition());
        m_lightManager->applyToShader(m_severallightsShader);

        const struct { MapMaterial material; Texture* texture; } texturedMaterials[] = {
            { MapMaterial::Container, m_containerTexture },
            { MapMaterial::Parquet,   m_parquetTexture   },
            { MapMaterial::Leaf,      m_leafTexture      },
            { MapMaterial::GreyRoof,  m_greyRoofTexture  },
            { MapMaterial::Riet,      m_rietTexture      },
        };
        for (const auto& entry : texturedMaterials) {
            if (!entry.texture) continue;
            entry.texture->applyToShader(m_severallightsShader);
            for (const auto& part : m_map->getParts()) {
                if (part.material != entry.material) continue;
                m_severallightsShader->setMat4("model", part.model);
                part.mesh->draw();
            }
        }
    }

    // 4. Portes interactives : matiere unie + contour (outline) pulse autour de
    // la silhouette quand elles sont interactives. Les
    // uniforms de lumiere sont communs a toutes les portes (1 seul apply).
    if (m_doorShader && m_map && !m_map->getDoors().empty()) {
        m_doorShader->use();
        m_doorShader->setMat4("view", m_camera->getViewMatrix());
        m_doorShader->setMat4("projection", m_doorShader->getProjection());
        m_doorShader->setVec3("viewPos", m_camera->getPosition());
        m_lightManager->applyToShader(m_doorShader);
        for (auto& door : m_map->getDoors()) {
            door->draw(m_doorShader, m_outlineShader, m_camera);
        }
    }

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
