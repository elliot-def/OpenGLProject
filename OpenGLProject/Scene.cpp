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
#include "FogParticles.h"
#include "SkySystem.h"
#include "Log.h"

#include "constants/texture.h"
#include "constants/material.h"
#include "constants/door.h"
#include "constants/resource.h"
#include "constants/creature.h"
#include "Model.h"
#include "Animator.h"
#include <algorithm>

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
    m_sky = std::make_unique<SkySystem>(m_camera, m_lightManager, m_renderer, m_textureManager,
                                        m_shaderManager, m_collisionManager);
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
    // Rechargement (lobby -> partie) : on repart d'un monde vide. Sans effet au
    // premier chargement.
    m_collisionManager->clear();
    m_lightManager->clearMapLights();
    m_map.reset();
    m_hitCooldown = 0.0f;
    m_aimedButton = -1;

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
    m_leavesShader = m_shaderManager->getShader("leaves");
    for (int i = 0; i < 3; ++i) {
        m_leafTextures[i] = std::make_unique<Texture>(
            "./res/models/trees/leaf_sheet_" + std::to_string(i) + ".png",
            Constants::Texture::FIRST_TEXTURE_ID + 110 + i, Constants::Material::STONE, false);
    }
    m_treeTexture = std::make_unique<Texture>("./res/models/trees/tree_palette.png",
        Constants::Texture::FIRST_TEXTURE_ID + 102, Constants::Material::STONE, false);
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
    m_lightManager->setFog(m_map->getFog().color, m_map->getFog().density);
    // Brume en particules seulement s'il y a du brouillard (pas de jour).
    if (m_map->getFog().density > 1e-4f) {
        m_fogParticles = std::make_unique<FogParticles>(m_shaderManager->getShader("fogparticles"));
    } else {
        m_fogParticles.reset();
    }
    // Portee de vue de fropy : valeur de la carte, sinon visibilite du brouillard.
    m_creatureSight = m_map->getCreatureSightRange() > 0.0f ? m_map->getCreatureSightRange()
                                                           : m_lightManager->getFogVisibility();
    // Ciel de la carte (jour / nuit)
    if (!m_map->getSkybox().empty()) createSkybox(m_map->getSkybox().c_str());

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

    // Tete de fropy dans le ciel (soleil / lune) et sa spot.
    m_sky->load(sun, m_map->getSunHead());

    // ── Créature qui poursuit le joueur (modele articule de fropy) ─────────
    // Options de partie : pas de fropy sur la carte (lobby) ou en mode
    // Exploration ; sinon difficulte (vitesse, vue, ouie).
    m_creature.reset();
    if (m_map->isCreatureEnabled() && m_matchCreature) {
        ensureCreatureModel();
        const glm::vec3 creatureSpawn = m_map->getSpawn("creature", glm::vec3(10.0f, 0.0f, -8.0f));
        m_creature = std::make_unique<Creature>(m_creatureModel.get(), creatureSpawn);
        m_creature->setDifficulty(m_matchSpeedMul, m_matchSenseMul);
        if (m_map->hasPatrolBounds()) m_creature->setPatrolBounds(m_map->getPatrolMin(), m_map->getPatrolMax());
        LOG_INFO("[Creature] spawn (%.1f, %.1f, %.1f) : poursuit le joueur (%.1f u/s, s'arrete a 1.2 m)",
                 creatureSpawn.x, creatureSpawn.y, creatureSpawn.z, m_creature->getSpeed());
    }

    // Boutons de la carte = fropys qui tournent : UN modele partage, dessine a chaque bouton.
    if (!m_map->getButtons().empty() && !m_buttonModel) {
        m_buttonModel = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                      Constants::Resource::MODEL_FROPY, m_textureManager);
        m_buttonModel->setSpin(30.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    buildPartIndex();

    // Joueur au point d'apparition "player" de la carte (s'il existe).
    m_player->setPosition(m_map->getSpawn("player", m_player->getPosition()));
    m_collisionManager->setVerticalVelocity(0.0f);

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
std::string Scene::getAimedButtonLabel() const {
    if (!m_map || m_aimedButton < 0 || m_aimedButton >= static_cast<int>(m_map->getButtons().size())) return "";
    return m_map->getButtons()[m_aimedButton].label;
}

// Origine de la visee (boutons, portes) : les yeux du joueur. En 3e personne la
// camera est CAMERA_THIRD_PERSON_DISTANCE derriere : mesurer la portee depuis elle
// mettait tout bouton hors de portee. La direction reste celle de la camera (elle
// passe par les yeux du joueur en 3P).
static glm::vec3 interactOrigin(const Camera* camera, const Player* player) {
    if (camera->isThirdPerson() && player) return player->getPosition() + Constants::Player::PLAYER_EYE_HEIGHT;
    return camera->getPosition();
}

// Modele articule de fropy : charge UNE fois (garde d'une carte a l'autre), 3 clips Mixamo
// (repos, marche, course) rejoues par rotations seules (squelette des clips, cf. Animator).
void Scene::ensureCreatureModel() {
    if (m_creatureModel) return;
    m_creatureModel = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                    Constants::Resource::MODEL_FROPY_RIG, m_textureManager);
    Model* model = m_creatureModel->getModel();
    const float h = model->getBoundingBox().getSize().y;
    if (h > 0.001f) m_creatureModel->setScale(Constants::Creature::MODEL_HEIGHT / h);
    const std::string dir = Constants::Resource::MIXAMO_ANIM_DIR;
    model->loadExternalAnimations({ dir + "idle.fbx", dir + "walking.fbx", dir + "standard run.fbx" });
    const int base = static_cast<int>(model->getNumAnimations() - model->getNumExternalAnimations());
    m_creatureModel->setIdleAnimIndex(base + 0);
    m_creatureModel->setWalkAnimIndex(base + 1);
    m_creatureModel->setRunAnimIndex(base + 2);
    if (Animator* anim = m_creatureModel->getAnimator()) anim->setRotationOnlyRetarget(true);
    m_creatureModel->playAnimation(base + 0, true);
    m_creatureAnim = base + 0;
    LOG_INFO("[Creature] modele articule charge (%zu os, echelle %.4f)", model->getBoneInfoMap().size(), m_creatureModel->getScale());
}

// Animation de fropy selon sa vitesse reelle : repos / marche (ronde) / course (poursuite),
// cadence calee sur la vitesse pour que les pieds ne glissent pas.
void Scene::updateCreatureAnimation(float deltaTime) {
    if (!m_creature || !m_creatureModel) return;
    using namespace Constants::Creature;
    const float v = m_creature->getMoveSpeed();
    const int want = v < 0.15f ? m_creatureModel->getIdleAnimIndex()
                   : (v < RUN_THRESHOLD ? m_creatureModel->getWalkAnimIndex() : m_creatureModel->getRunAnimIndex());
    if (want != m_creatureAnim) {
        m_creatureModel->playAnimation(want, true);
        m_creatureAnim = want;
    }
    if (Animator* anim = m_creatureModel->getAnimator()) {
        const float natural = want == m_creatureModel->getRunAnimIndex() ? RUN_ANIM_SPEED
                            : (want == m_creatureModel->getWalkAnimIndex() ? WALK_ANIM_SPEED : 0.0f);
        anim->setSpeedScale(natural > 0.0f ? std::clamp(v / natural, 0.6f, 1.8f) : 1.0f);
    }
    m_creatureModel->updateAnimation(deltaTime);
}

// Fropy ouvre les portes comme un joueur : une porte FERMEE qui barre sa trajectoire
// immediate (DOOR_LOOKAHEAD m vers son prochain point), a portee et dans son regard
// (meme regle que la touche E : Door::canInteract). Elle bascule loin de lui (poussee).
void Scene::openDoorsForCreature() {
    if (!m_creature || !m_map || !m_creature->isSteering()) return;
    const glm::vec3 p = m_creature->getPosition();
    glm::vec3 to = m_creature->getSteerTarget() - p;
    to.y = 0.0f;
    const float len = glm::length(to);
    if (len < 1e-3f) return;
    const glm::vec3 ahead = p + to * (std::min(len, Constants::Creature::DOOR_LOOKAHEAD) / len);
    const glm::vec3 eye = p + glm::vec3(0.0f, Constants::Creature::EYE_HEIGHT, 0.0f);
    for (auto& door : m_map->getDoors()) {
        if (!door->getIsOpen() && door->blocksSegment(p, ahead) && door->canInteract(eye, m_creature->getFaceDir())) {
            door->toggle(p);
        }
    }
}

const std::vector<MapButton>& Scene::getMapButtons() const {
    static const std::vector<MapButton> none;
    return m_map ? m_map->getButtons() : none;
}

void Scene::setButtonEnabled(const std::string& name, bool enabled) {
    if (!m_map) return;
    for (MapButton& b : m_map->getButtons()) {
        if (b.name == name) b.enabled = enabled;
    }
}

void Scene::tryToggleDoor() {
    if (!m_map) return;
    const glm::vec3 eye = interactOrigin(m_camera, m_player);

    // Bouton vise : prioritaire sur les portes.
    if (m_aimedButton >= 0 && m_buttonCallback) {
        m_buttonCallback(m_map->getButtons()[m_aimedButton].name);
        return;
    }

    Door* best = nullptr;
    float bestDist = 0.0f;
    for (auto& door : m_map->getDoors()) {
        if (!door->canInteract(eye, m_camera->getFront())) {
            continue;
        }
        const float dist = glm::length(door->getCenter() - eye);
        if (!best || dist < bestDist) {
            best = door.get();
            bestDist = dist;
        }
    }
    if (best) {
        // Le joueur pousse la porte : elle bascule du cote oppose a sa position.
        best->toggle(eye);
    }
}

// Texture d'un modèle OBJ (cache par chemin ; "" = blanc, teinté par la couleur Kd).
Texture* Scene::getModelTexture(const std::string& path) {
    const std::string key = path.empty() ? "./res/textures/white.png" : path;
    auto it = m_modelTextures.find(key);
    if (it == m_modelTextures.end()) {
        it = m_modelTextures.emplace(key, std::make_unique<Texture>(key,
            Constants::Texture::FIRST_TEXTURE_ID + 130 + static_cast<int>(m_modelTextures.size()),
            Constants::Material::STONE, false)).first;
    }
    return it->second.get();
}

// Index des parties de carte par materiau + sphere englobante monde (culling).
void Scene::buildPartIndex() {
    m_partsByMaterial.assign(16, {});
    m_partSpheres.clear();
    const auto& parts = m_map->getParts();
    for (size_t i = 0; i < parts.size(); ++i) {
        m_partsByMaterial[static_cast<int>(parts[i].material)].push_back(i);
        glm::vec3 mn(1e30f), mx(-1e30f);
        for (const Vertex& v : parts[i].mesh->getVertices()) {
            const glm::vec3 w = glm::vec3(parts[i].model * glm::vec4(v.getPositions(), 1.0f));
            mn = glm::min(mn, w);
            mx = glm::max(mx, w);
        }
        const glm::vec3 c = (mn + mx) * 0.5f;
        m_partSpheres.push_back(glm::vec4(c, glm::length(mx - c)));
    }
}

bool Scene::partVisible(size_t i) const {
    if (i >= m_partSpheres.size()) return true;
    const glm::vec4& sp = m_partSpheres[i];
    for (const glm::vec4& p : m_frustum) {
        if (glm::dot(glm::vec3(p), glm::vec3(sp)) + p.w < -sp.w) return false;   // entierement derriere un plan
    }
    return true;
}

void Scene::createSkybox(const char* path) {
    m_skybox = std::make_unique<Skybox>(path);
}

void Scene::adoptEntities(ModelEntity* humanEntity, CharacterAnimationController* characterAnim) {
    m_humanEntity    = humanEntity;
    m_characterAnim  = characterAnim;
}

// ── Mise à jour du monde ────────────────────────────────────────────────────

void Scene::update(float deltaTime) {
    // Portes : glow d'interaction (portee + regard), animation d'ouverture /
    // fermeture, et collider dynamique si la matrice a change (dirty-flag).
    const glm::vec3 eye = interactOrigin(m_camera, m_player);
    if (m_map) {
        auto& doors = m_map->getDoors();
        for (size_t i = 0; i < doors.size(); ++i) {
            Door* door = doors[i].get();
            door->setHighlighted(door->canInteract(eye, m_camera->getFront()));
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

    // Bouton vise : le plus proche a portee (2.5 m) dans le cone de regard.
    m_aimedButton = -1;
    if (m_map) {
        float bestDist = 2.5f;
        const auto& buttons = m_map->getButtons();
        for (size_t i = 0; i < buttons.size(); ++i) {
            if (!buttons[i].enabled) continue;
            const glm::vec3 to = buttons[i].position - eye;
            const float d = glm::length(to);
            if (d < 1e-4f || d > bestDist) continue;
            if (glm::dot(to / d, glm::normalize(m_camera->getFront())) < 0.8f) continue;
            bestDist = d;
            m_aimedButton = static_cast<int>(i);
        }
    }

    // Créature : poursuite du joueur (glissement contre les murs via le
    // sweepSphere du CollisionManager, s'arrête à sa portée).
    if (m_creature) {
        // L'IA de la creature gere elle-meme cible, vision (portee = brouillard +
        // ligne de vue), perte d'agro et rondes. Cibles : joueur local + distants.
        std::vector<glm::vec3> targets;
        targets.reserve(1 + m_remoteTargets.size());
        targets.push_back(m_player->getPosition());
        targets.insert(targets.end(), m_remoteTargets.begin(), m_remoteTargets.end());
        m_creature->update(deltaTime, targets, m_creatureSight, m_noises, m_collisionManager);
        updateCreatureAnimation(deltaTime);
        openDoorsForCreature();

        // Fropy trop pres du joueur (hors noclip) : coup. Invulnerabilite courte, fropy etourdi.
        m_hitCooldown = std::max(0.0f, m_hitCooldown - deltaTime);
        const glm::vec3 cp = m_creature->getPosition(), pp = m_player->getPosition();
        if (m_hitCooldown <= 0.0f && m_player->isGravityEnabled() && m_playerHitCallback &&
            glm::length(glm::vec2(cp.x - pp.x, cp.z - pp.z)) < Constants::Player::CREATURE_ATTACK_RANGE &&
            std::abs(pp.y - cp.y) < 2.0f) {
            m_hitCooldown = Constants::Player::PLAYER_HIT_COOLDOWN;
            m_creature->stun(Constants::Player::CREATURE_STUN_TIME);
            m_playerHitCallback();
        }
    }
    m_noises.clear();   // les bruits ne durent qu'une frame (re-emis tant que ca dure)
    // Ciel : la tete regarde la creature (torse) ; eblouissement.
    {
        glm::vec3 gaze(0.0f);
        if (m_creature) gaze = m_creature->getPosition() + glm::vec3(0.0f, 1.0f, 0.0f);
        m_sky->update(deltaTime, m_creature ? &gaze : nullptr);
    }
    if (m_fogParticles) m_fogParticles->update(deltaTime, m_camera->getPosition());

    m_camera->update(m_player);
    m_lightManager->update();

    // Fropys-boutons (lobby) : rotation + leger grossissement du bouton vise.
    if (m_buttonModel && m_map && !m_map->getButtons().empty()) m_buttonModel->update();

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
    // Plans du frustum (Gribb-Hartmann) pour le culling des parties de carte.
    {
        const glm::mat4 pv = (m_severallightsShader ? m_severallightsShader->getProjection() : glm::mat4(1.0f))
                           * m_camera->getViewMatrix();
        for (int i = 0; i < 3; ++i) {
            for (int sgn = 0; sgn < 2; ++sgn) {
                glm::vec4 p;
                for (int c = 0; c < 4; ++c) p[c] = pv[c][3] + (sgn ? -pv[c][i] : pv[c][i]);
                m_frustum[i * 2 + sgn] = p / glm::length(glm::vec3(p));
            }
        }
    }
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

    // Tete du ciel (couleur unie, sans brouillard).
    m_sky->drawHead(m_doorShader);

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

        for (size_t pi : m_partsByMaterial[static_cast<int>(MapMaterial::BrickWall)]) {
            const MapPart& part = m_map->getParts()[pi];
            if (!partVisible(pi)) continue;
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
            { MapMaterial::Tree,      m_treeTexture.get() },
        };
        for (const auto& entry : texturedMaterials) {
            if (!entry.texture) continue;
            entry.texture->applyToShader(m_severallightsShader);
            m_severallightsShader->setFloat("uPlainTexture", entry.material == MapMaterial::Tree ? 1.0f : 0.0f);
            for (size_t pi : m_partsByMaterial[static_cast<int>(entry.material)]) {
                const MapPart& part = m_map->getParts()[pi];
                if (!partVisible(pi)) continue;
                m_severallightsShader->setMat4("model", part.model);
                if (entry.material == MapMaterial::Tree) m_severallightsShader->setVec3("uTreeTint", part.tint);
                part.mesh->draw();
            }
        }
    }

    // 3.ter Modèles OBJ (bâtiment) : opaques puis transparents (vitres, alpha < 1 : blending,
    // sans écriture de profondeur). Texture directe (UV du fichier, pas d'anti-répétition).
    if (m_map && m_severallightsShader) {
        m_severallightsShader->use();
        m_severallightsShader->setFloat("useInstanceModel", 0.0f);
        m_severallightsShader->setFloat("uPlainTexture", 1.0f);
        for (int pass = 0; pass < 2; ++pass) {
            if (pass == 1) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            }
            for (size_t pi : m_partsByMaterial[static_cast<int>(MapMaterial::Textured)]) {
                const MapPart& part = m_map->getParts()[pi];
                if (!partVisible(pi)) continue;
                const bool glass = part.alpha < 0.99f;
                if (glass != (pass == 1)) continue;
                if (Texture* tex = getModelTexture(part.texturePath)) tex->applyToShader(m_severallightsShader);
                m_severallightsShader->setMat4("model", part.model);
                m_severallightsShader->setVec3("uTreeTint", part.color);
                m_severallightsShader->setFloat("uAlpha", glass ? part.alpha : 0.0f);
                part.mesh->draw();
            }
        }
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        m_severallightsShader->setFloat("uAlpha", 0.0f);
        m_severallightsShader->setFloat("uPlainTexture", 0.0f);
    }

    // 3.bis Feuilles au sol (alpha test, geometrie deja en repere monde).
    if (m_map && m_leavesShader) {
        m_leavesShader->use();
        m_leavesShader->setMat4("view", m_camera->getViewMatrix());
        m_leavesShader->setMat4("projection", m_leavesShader->getProjection());
        m_lightManager->applyToShader(m_leavesShader);   // dirLight
        m_leavesShader->setInt("uTex", 0);
        m_leavesShader->setVec3("viewPos", m_camera->getPosition());
        for (int i = 0; i < MapLoader::LEAF_SHEETS; ++i) {
            const Mesh* mesh = m_map->getLeafMesh(i);
            if (!mesh || !m_leafTextures[i]) continue;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_leafTextures[i]->getID());
            mesh->draw();
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
        if (m_buttonModel && m_map) {
            for (size_t i = 0; i < m_map->getButtons().size(); ++i) {
                const MapButton& b = m_map->getButtons()[i];
                if (!b.enabled) continue;
                m_buttonModel->setPosition(b.position);
                m_buttonModel->setScale(b.scale * (static_cast<int>(i) == m_aimedButton ? 1.25f : 1.0f));
                m_buttonModel->draw(m_modelShader);
            }
        }
    }
    // Créature : modele articule de fropy (skinning).
    if (m_creature && m_creatureModel && m_skinnedShader) m_creatureModel->draw(m_skinnedShader);

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

    // Brume en particules (transparente : apres tout l'opaque, avant l'eblouissement)
    if (m_fogParticles) {
        m_fogParticles->draw(m_camera->getViewMatrix(), m_leavesShader ? m_leavesShader->getProjection() : glm::mat4(1.0f),
                             m_camera->getPosition());
    }

    m_sky->drawGlare(m_doorShader);
}
