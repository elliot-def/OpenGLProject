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
#include "TvScreen.h"
#include "CharacterAnimationController.h"
#include "Creature.h"
#include "Renderer.h"
#include "Log.h"

#include "constants/texture.h"
#include "constants/material.h"
#include "constants/door.h"
#include "constants/resource.h"

#include <cmath>
#include <string>
#include <unordered_map>

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
    m_glareShader = m_shaderManager->getShader("glare");
    glGenVertexArrays(1, &m_glareVao);
}

Scene::~Scene() = default;

// ── Construction du monde ────────────────────────────────────────────────────

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

    // ── Ambiance de la carte : ensolleilement (clé "sun" du JSON) ──────────
    // Direction + couleurs RGB du soleil → lumière directionnelle. C'est LE
    // réglage d'ambiance par map (cf. MapSun / LightManager::setDirLight).
    const MapSun& sun = m_map->getSun();
    m_lightManager->setDirLight({ sun.direction, sun.ambient, sun.diffuse, sun.specular });

    // ── Pièces + lumières ponctuelles de la carte ("rooms", "lights") ─────
    std::unordered_map<std::string, int> roomIndex;
    for (const auto& room : m_map->getRooms()) {
        roomIndex[room.name] = m_lightManager->addRoom(room.min, room.max);
    }
    Shader* lightShader = m_shaderManager->getShader("cube/lightsource");
    for (const auto& l : m_map->getLights()) {
        const size_t idx = m_lightManager->addPointLight(new LightSource(
            l.position, lightShader, l.ambient, l.diffuse, l.specular,
            l.attenuation.x, l.attenuation.y, l.attenuation.z, l.color));
        if (!l.room.empty()) {
            const auto it = roomIndex.find(l.room);
            if (it != roomIndex.end()) {
                m_lightManager->setLightRoom(idx, it->second);
            } else {
                LOG_WARN("[Map] Lumiere : piece inconnue \"%s\"", l.room.c_str());
            }
        }
    }

    // Soleil VISIBLE : tête de fropy JAUNE dans le ciel, placée à l'opposé du
    // rayonnement (le dir light pointe vers la scène, le soleil est en haut).
    // Shader "door" = couleur unie (uColor) → jaune sans texture, quelle que
    // soit l'ambiance de la map.
    glm::vec3 sunDir = sun.direction;
    if (glm::length(sunDir) < 1e-4f) sunDir = glm::vec3(0.0f, -1.0f, 0.0f);
    sunDir = glm::normalize(sunDir);
    const glm::vec3 mapCenter(0.0f, 0.0f, -10.0f);          // centre du village
    m_sunHead = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                              Constants::Resource::MODEL_FROPY_HEAD,
                                              m_textureManager);
    m_sunHead->setScale(24.0f);                              // tête géante, vue de loin
    m_sunHead->setPosition(mapCenter - sunDir * 60.0f);
    // Face (local +Z) tournée vers la scène : yaw = atan2(fz, fx) en degrés.
    m_sunHead->setDirection(Direction(glm::degrees(std::atan2(-sunDir.z, -sunDir.x)), 0.0));
    LOG_INFO("[Map] Soleil visible: tete de fropy jaune a (%.1f, %.1f, %.1f)",
             (mapCenter - sunDir * 60.0f).x, (mapCenter - sunDir * 60.0f).y,
             (mapCenter - sunDir * 60.0f).z);

    // ── Créature qui poursuit le joueur (corps sombre + tête de fropy) ─────
    m_creatureHead = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                   Constants::Resource::MODEL_FROPY_HEAD,
                                                   m_textureManager);
    m_creatureBodyShader = m_shaderManager->getShader("cube/lightsource");
    const glm::vec3 creatureSpawn = m_map->getSpawn("creature", glm::vec3(10.0f, 0.0f, -8.0f));
    m_creature = std::make_unique<Creature>(m_creatureHead.get(), creatureSpawn);
    LOG_INFO("[Creature] spawn (%.1f, %.1f, %.1f) : poursuit le joueur (%.1f u/s, s'arrete a 1.2 m)",
             creatureSpawn.x, creatureSpawn.y, creatureSpawn.z, m_creature->getSpeed());

    // [DIAG] auto-test de la poursuite (une fois au chargement) : la créature
    // doit se rapprocher du joueur. Repart ensuite de son spawn.
    {
        const glm::vec3 target(0.0f, 0.0f, 3.0f);
        const float d0 = glm::length(m_creature->getPosition() - target);
        for (int i = 0; i < 300; ++i) {
            m_creature->update(1.0f / 60.0f, target, nullptr);
        }
        const float d1 = glm::length(m_creature->getPosition() - target);
        LOG_INFO("[Creature][DIAG] auto-test poursuite: distance %.2f -> %.2f en 5s (doit decroitre)",
                 d0, d1);
        m_creature->setPosition(creatureSpawn);
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

    // Écrans TV ("tvs" dans la carte) : easter egg DVD animé, visible de près.
    m_tvScreens.clear();
    m_maskShader = m_shaderManager->getShader("image/mask");
    for (const auto& screen : m_map->getScreens()) {
        m_tvScreens.push_back(std::make_unique<TvScreen>(
            m_maskShader, screen.model, screen.width, screen.height));
    }

    // E (touche Grab, ou RB manette) : ouvrir/fermer la porte visee.
    if (m_inputManager) {
        m_inputManager->setInteractCallback([this]() { tryToggleDoor(); });
    }
}

// Ouvre/ferme la porte interactive la plus proche parmi celles que le joueur
// est en train de regarder (portee + cone de regard, voir Door::canInteract).
// La tete du soleil regarde la creature en permanence et l'eclaire d'un spot.
void Scene::updateSunGaze() {
    if (!m_sunHead || !m_creature) return;
    const glm::vec3 from = m_sunHead->getPosition();
    const glm::vec3 target = m_creature->getPosition() + glm::vec3(0.0f, 1.0f, 0.0f);   // torse
    const glm::vec3 d = glm::normalize(target - from);
    m_sunHead->setDirection(Direction(glm::degrees(std::atan2(d.z, d.x)), 0.0));
    m_sunHead->setPitchDeg(glm::degrees(std::asin(-d.y)));
    m_lightManager->aimSunSpot(from, target);
}

// Eblouissement : intensite selon l'angle entre le regard et le soleil, annulee
// si un obstacle (toit, mur) coupe la ligne camera -> soleil. Lissee pour
// eviter les sauts quand on passe une porte ou qu'on tourne la tete.
void Scene::updateSunGlare(float deltaTime) {
    m_glareTime += deltaTime;
    float target = 0.0f, targetDazzle = 0.0f;
    if (m_sunHead) {
        const glm::vec3 cam = m_camera->getPosition();
        const glm::vec3 toSun = m_sunHead->getPosition() - cam;
        const float dist = glm::length(toSun);
        const float facing = dist > 1e-3f ? glm::dot(m_camera->getFront(), toSun / dist) : 0.0f;
        const float wide = glm::smoothstep(std::cos(glm::radians(55.0f)), std::cos(glm::radians(6.0f)), facing);
        if (wide > 0.0f) {
            bool visible = true;
            const glm::vec3 dir = toSun / dist;
            for (float t = 0.5f; t < dist - 10.0f && visible; t += 1.5f) {   // -10 : on s'arrete avant la tete (pas de collider)
                visible = !m_collisionManager->testSphereAll(cam + dir * t, 0.15f).hit;
            }
            if (visible) {
                target = wide;
                targetDazzle = glm::smoothstep(std::cos(glm::radians(22.0f)), std::cos(glm::radians(2.0f)), facing);
            }
        }
    }
    const float k = std::min(1.0f, deltaTime * 6.0f);
    m_glare  += (target - m_glare) * k;
    m_dazzle += (targetDazzle - m_dazzle) * k;
}

void Scene::drawSunGlare() {
    if (!m_glareShader || !m_sunHead || !(m_glare >= 0.003f)) return;   // !(>=) : rejette aussi NaN
    const glm::mat4 proj = m_doorShader->getProjection();
    const glm::vec4 clip = proj * m_camera->getViewMatrix() * glm::vec4(m_sunHead->getPosition(), 1.0f);
    if (clip.w <= 0.01f) return;   // derriere la camera

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);   // melange "screen" : ne sature jamais au-dela de 1
    m_glareShader->use();
    m_glareShader->setVec2("sunNdc", glm::vec2(clip.x / clip.w, clip.y / clip.w));
    m_glareShader->setFloat("aspect", proj[1][1] / proj[0][0]);
    m_glareShader->setFloat("intensity", m_glare);
    m_glareShader->setFloat("dazzle", m_dazzle);
    m_glareShader->setFloat("time", m_glareTime);
    glBindVertexArray(m_glareVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

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

    // Écrans TV : animation du logo DVD uniquement à portée (optimisation).
    for (auto& tv : m_tvScreens) {
        tv->update(deltaTime, m_camera->getPosition());
    }

    m_player->update();

    // Créature : poursuite du joueur (glissement contre les murs via le
    // sweepSphere du CollisionManager, s'arrête à sa portée).
    if (m_creature) {
        // Cible = joueur (local ou distant) le plus proche en XZ.
        const glm::vec3 cpos = m_creature->getPosition();
        glm::vec3 target = m_player->getPosition();
        float best = glm::distance(glm::vec2(cpos.x, cpos.z), glm::vec2(target.x, target.z));
        for (const glm::vec3& p : m_remoteTargets) {
            const float d = glm::distance(glm::vec2(cpos.x, cpos.z), glm::vec2(p.x, p.z));
            if (d < best) { best = d; target = p; }
        }
        m_creature->update(deltaTime, target, m_collisionManager);
        updateSunGaze();
    }
    updateSunGlare(deltaTime);

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
    // Corps de la créature (cubes émissifs sombres) : même lot instancié.
    if (m_creature && m_creatureBodyShader) {
        m_creature->submitBody(m_cubeRenderer.get(), m_creatureBodyShader);
    }
    m_cubeRenderer->draw(m_camera, m_lightManager);

    // Soleil : tête de fropy JAUNE (shader "door" = couleur unie uColor,
    // éclairée par la lumière directionnelle de la map).
    if (m_sunHead && m_doorShader) {
        m_doorShader->use();
        m_doorShader->setVec3("uColor", glm::vec3(1.0f, 0.92f, 0.25f));
        m_sunHead->draw(m_doorShader);
    }

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
        // Créature : tête de fropy (le corps sombre est instancié plus haut).
        if (m_creatureHead) m_creatureHead->draw(m_modelShader);
    }

    // 5. Écrans TV : fond noir + logo DVD (blending, depth test actif — les
    // murs occultent correctement l'écran). Le logo n'est dessiné qu'à
    // proximité de la télé (cf. TvScreen::update).
    if (m_maskShader) {
        for (auto& tv : m_tvScreens) {
            tv->draw(m_camera->getViewMatrix(), m_maskShader->getProjection());
        }
    }

    // Humain 3ème personne (visible uniquement en vue 3P)
    if (m_humanEntity && m_skinnedShader && m_player->isThirdPerson()) {
        m_humanEntity->draw(m_skinnedShader);
    }

    // Vue 1ère personne : le playermodel est CACHÉ (aucun rendu du modèle
    // du joueur — ni corps ni bras). Il n'apparaît qu'en vue 3ème personne
    // (cf. bloc ci-dessus) ; les joueurs distants restent visibles dans les
    // deux vues (MultiplayerManager).

    drawSunGlare();
}
