#include "FirstPersonArms.h"
#include <iostream>
#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "LightManager.h"
#include "Animator.h"

#include "constants/firstpersonarms.h"
#include "constants/window.h"
#include "constants/file.h"
#include "File.h"
#include <nlohmann/json.hpp>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cfloat>   // FLT_MAX pour la AABB de debug
#include <cmath>    // isnan / isinf
#include <cstdio>

using json = nlohmann::json;

FirstPersonArms::FirstPersonArms(Camera* camera, LightManager* lightManager,
                                 const std::string& modelPath, TextureManager* textureManager)
    : m_camera(camera), m_lightManager(lightManager),
      m_model(std::make_unique<Model>(camera, lightManager, modelPath, textureManager)),
      m_animator(std::make_unique<Animator>()) {

    m_animator->setup(m_model.get());

    // Pose commune 1P/3P : applyViewmodelOffsets() relève et écarte les
    // deux bras. Le placement, l'échelle et la caméra restent différents
    // selon le mode, mais les angles de pose sont identiques.

    // Jouer l'idle par défaut (chercher "finger_gun_idle" par nom)
    const aiScene* scene = m_model->getScene();
    if (scene) {
        const auto& meshes  = m_model->getMeshes();
        const auto& boneMap = m_model->getBoneInfoMap();
        detectAnimations();
        if (m_idleAnimIndex >= 0) {
            m_animator->playAnimation(m_idleAnimIndex, true);
        } else if (scene->mNumAnimations > 0) {
            m_animator->playAnimation(0u, true);
        }
        // Calcule la pose immédiatement : un crossfade ultérieur prendra un
        // instantané valide (au lieu de matrices identité jamais calculées) et
        // le premier draw dispose déjà de la pose correcte.
        m_animator->update(0.0f);

        // Charger la config des bones à cacher par animation (res/armBones.json).
        // Remplace le hardcoding de upper_arm.R/L : chaque animation peut avoir
        // sa propre liste de bones invisibles en 1P.
        loadArmBonesConfig();

        // Trouver le bone racine du squelette (id=0, premier bone extrait
        // par Assimp → généralement le parent de toute la hiérarchie).
        // Sert au tilt dynamique de l'animation relax (kRelaxTiltDeg).
        // Cache aussi les IDs des avant-bras pour le collapse 1P.
        for (const auto& [name, info] : boneMap) {
            if (info.id == 0) {
                m_spineBoneName = name;
            }
        }
    } else {
    }

    // Texture blanche de fallback (1x1) si le modele n'a pas de textures
    unsigned char white[] = { 255, 255, 255, 255 };
    glGenTextures(1, &m_fallbackTexture);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void FirstPersonArms::loadArmBonesConfig() {
    // Construire la map nom → ID depuis la bone map du modèle
    const auto& boneMap = m_model->getBoneInfoMap();
    for (const auto& [name, info] : boneMap) {
        m_boneNameToId[name] = info.id;
    }

    // Charger le JSON de configuration
    File configFile(Constants::File::JSON_ARMBONES_PATH);
    if (!configFile.exists()) {
        return;
    }

    try {
        json j = json::parse(configFile.readAll());

        // Fonction helper : résout une liste de noms de bones → set d'IDs
        auto resolve = [&](const json& names) -> std::unordered_set<int> {
            std::unordered_set<int> ids;
            for (const auto& name : names) {
                auto it = m_boneNameToId.find(name.get<std::string>());
                if (it != m_boneNameToId.end()) {
                    ids.insert(it->second);
                } else {
                }
            }
            return ids;
        };

        for (auto& [key, val] : j.items()) {
            if (key == "_comment" || key == "_bones_disponibles") continue;
            if (!val.is_array()) {
                continue;
            }
            auto ids = resolve(val);
            if (key == "default") {
                m_defaultHiddenBones = std::move(ids);
            } else {
                m_hideRules.emplace_back(key, std::move(ids));
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << '\n';
    }
}

FirstPersonArms::~FirstPersonArms() {
    if (m_fallbackTexture)
        glDeleteTextures(1, &m_fallbackTexture);
}

void FirstPersonArms::update(float deltaTime, const glm::vec3& playerPos, bool isSprinting) {
    m_playerPos = playerPos;

    // Pose commune aux deux modes : les mêmes offsets d'élévation et
    // d'écartement sont conservés en 1P comme en 3P. Seuls le placement du
    // modèle, l'échelle et la matrice de vue changent entre les modes.
    // L'animation finger_gun_fire conserve donc aussi cette pose pendant le tir.
    if (!m_viewmodelOffsetsActive) {
        applyViewmodelOffsets();
        m_viewmodelOffsetsActive = true;
    }

    // ── Relax tilt : pencher l'animation de marche vers le bas ──────────
    // Appliqué/retiré dynamiquement : l'offset de rotation sur le bone
    // racine (id=0) n'est actif QUE pendant l'animation "relax".
    // L'offset persiste jusqu'au prochain update(), donc le tilt apparaît
    // dès la frame suivante (délai imperceptible).
    const aiAnimation* currentAfter = m_animator->getCurrentAnimation();
    const bool isRelax = currentAfter &&
        std::string(currentAfter->mName.C_Str()).find("relax") != std::string::npos;
    if (isRelax && !m_relaxTiltActive && !m_spineBoneName.empty()) {
        m_animator->addBoneOffset(m_spineBoneName,
            glm::rotate(glm::mat4(1.0f), glm::radians(kRelaxTiltDeg), glm::vec3(1.0f, 0.0f, 0.0f)));
        m_relaxTiltActive = true;
    } else if (!isRelax && m_relaxTiltActive) {
        m_animator->addBoneOffset(m_spineBoneName, glm::mat4(1.0f));
        m_relaxTiltActive = false;
    }

    // ── Idle vs marche : animations du FICHIER (pas de bobbing custom) ─────
    // Détection du mouvement par la variation de position du joueur entre 2
    // frames (m_wantsToMove n'est pas maintenu dans Player).
    bool isMoving = false;
    if (m_playerPosInitialized) {
        const glm::vec3 delta = playerPos - m_lastPlayerPos;
        const float horizSpeed = glm::length(glm::vec3(delta.x, 0.0f, delta.z))
                                 / std::max(deltaTime, 1e-5f);
        isMoving = horizSpeed > 0.05f; // seuil : immobile vs en mouvement
    }
    m_playerPosInitialized = true;
    m_lastPlayerPos = playerPos;

    // L'animation cible dépend de l'état : "relax" (marche) si le joueur
    // bouge, finger_gun_idle sinon. Ne relance rien si c'est déjà celle qui
    // joue (évite le reset de m_currentTime à chaque frame).
    const aiScene* scene = m_model->getScene();
    const aiAnimation* current = m_animator->getCurrentAnimation();
    const bool firing = m_fireAnimIndex >= 0 && scene &&
        current == scene->mAnimations[m_fireAnimIndex];
    const bool pushing = m_pushAnimIndex >= 0 && scene &&
        current == scene->mAnimations[m_pushAnimIndex];
    const bool grabbing = m_grabAnimIndex >= 0 && scene &&
        current == scene->mAnimations[m_grabAnimIndex];
    // Animation "d'action" (non-loop) en cours : on ne switch PAS idle↔marche
    // pendant qu'elle joue, et on revient au repos quand elle se termine.
    const bool actionAnim = firing || pushing || grabbing;

    // Debounce idle ↔ marche : évite les oscillations rapides quand le
    // joueur est poussé légèrement (cube → position oscille de qq mm →
    // isMoving alterne → crossfade permanent). Le cooldown s'écoule ici et
    // est réinitialisé à chaque switch effectif.
    m_animSwitchCooldown -= deltaTime;

    // Garde : si le modèle n'a pas chargé (scene null), aucun index n'est
    // valide — on laisse l'Animator faire son early-return dans update().
    const int targetAnim = restAnimIndex(isMoving);
    if (!actionAnim && scene && targetAnim >= 0 && m_animSwitchCooldown <= 0.0f) {
        const aiAnimation* target = scene->mAnimations[targetAnim];
        if (current != target) {
            m_animator->playAnimation(targetAnim, true);
            m_animSwitchCooldown = kAnimSwitchCooldown;
        }
    }

    m_animator->update(deltaTime);

    // After action anim finishes, return to rest.
    if (actionAnim && m_animator->isFinished() && targetAnim >= 0) {
        m_animator->playAnimation(targetAnim, true);
        m_animator->update(0.0f);
    }
}

void FirstPersonArms::triggerFire() {
    if (!m_animator) return;

    if (m_fireAnimIndex >= 0) {
        m_animator->playAnimation(m_fireAnimIndex, false, false);
    }
}

void FirstPersonArms::triggerPush() {
    if (m_pushAnimIndex >= 0 && m_animator) {
        m_animator->playAnimation(m_pushAnimIndex, false, false);
    } else {
    }
}

void FirstPersonArms::triggerGrab() {
    if (m_grabAnimIndex >= 0 && m_animator) {
        m_animator->playAnimation(m_grabAnimIndex, false, false);
    } else {
    }
}

void FirstPersonArms::applyViewmodelOffsets() {
    // Pose commune 1P/3P : bras DROIT déjà levé par l'anim finger_gun_idle
    // (doigts en "finger gun"), bras GAUCHE relevé en miroir par les offsets.
    //  - upper_arm.L : élévation (RotX +60°) PUIS écartement (RotZ +spread) → la
    //    main gauche arrive à ~(-0.16,-0.19,-0.89) espace caméra (fix_search.py).
    //  - forearm.L   : pli du coude (RotX +70°).
    //  - upper_arm.R : écartement opposé (RotZ −spread) uniquement → écarte la main
    //    droite de l'axe de visée (la pose elle-même vient de l'animation).
    // Signe OPPOSÉ au bras gauche : les repères locaux des deux bras sont en
    // miroir (même signe des deux côtés rapprocherait les mains — simulation
    // spread_test.py). L'axe local X du bras gauche pointe vers le
    // haut-extérieur (angles POSITIFS = élévation vers l'avant) ; l'axe local
    // Z fait l'abduction. Angles réglables dans constants/firstPersonArms.h.
    const float upperArm = glm::radians(Constants::FirstPersonArms::FP_ARMS_BONE_UPPER_ARM_DEG);
    const float forearm  = glm::radians(Constants::FirstPersonArms::FP_ARMS_BONE_FOREARM_DEG);
    const float spread   = glm::radians(Constants::FirstPersonArms::FP_ARMS_SPREAD_DEG);
    const float spacing  = Constants::FirstPersonArms::FP_ARMS_SPACING;
    const glm::vec3 axisX(1.0f, 0.0f, 0.0f);
    const glm::vec3 axisZ(0.0f, 0.0f, 1.0f);
    // La translation d'écartement est indépendante des rotations : modifier
    // FP_ARMS_SPACING déplace les bras sans changer les angles de pose.
    // Même ordre de composition que la simulation : off = T(spacing) @ Rx(élév) @ Rz(spread)
    m_animator->addBoneOffset("upper_arm.L",
        glm::translate(glm::mat4(1.0f), glm::vec3(spacing, 0.0f, 0.0f)) *
        glm::rotate(glm::rotate(glm::mat4(1.0f), upperArm, axisX), spread, axisZ));
    m_animator->addBoneOffset("forearm.L", glm::rotate(glm::mat4(1.0f), forearm, axisX));
    m_animator->addBoneOffset("upper_arm.R",
        glm::translate(glm::mat4(1.0f), glm::vec3(-spacing, 0.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), -spread, axisZ));
}

const std::vector<Mesh*>& FirstPersonArms::getMeshes() {
    return m_model->getMeshes();
}

void FirstPersonArms::draw(Shader* shader) {
    auto& meshes = m_model->getMeshes();
    const bool thirdPerson = m_camera && m_camera->isThirdPerson();

    glm::mat4 armModel;
    glm::mat4 view;
    glm::vec3 viewPos;

    if (thirdPerson) {
        // ── 3e personne : bras attachés au corps du joueur (world-space) ──
        // Le rig est un personnage debout (épaules y≈1.6, bras pendants). On le
        // place à la position du joueur, épaules alignées sur le torse (~1.4 m),
        // orienté dans la direction de vue du joueur (même convention que
        // ModelEntity : yaw = atan2(front.x, front.z) aligne +Z du rig sur le front).
        glm::vec3 front = m_camera->getFront();
        float yaw = atan2(front.x, front.z);
        armModel = glm::mat4(1.0f);
        armModel = glm::translate(armModel, m_playerPos + glm::vec3(0.0f, Constants::FirstPersonArms::FP_ARMS_3P_OFFSET_Y, 0.0f));
        armModel = glm::rotate(armModel, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        armModel = glm::scale(armModel, glm::vec3(Constants::FirstPersonArms::FP_ARMS_3P_SCALE));
        view = m_camera->getViewMatrix();
        viewPos = m_camera->getPosition();
    } else {
        // ── 1re personne : overlay viewmodel en espace caméra ──
        // Rotation Y 180° : +X rig (main GAUCHE du personnage) -> ecran GAUCHE
        // (-X) : gauche/droite corrects (pas d'effet miroir), et +Z rig (paumes)
        // pointe dans l'ecran (on voit le dos des mains, comme en vraie 1re
        // personne). PAS de rotation autour de X : le rig reste a l'endroit (Y
        // vers le haut). La pose idle (finger_gun_idle) a deja le bras droit
        // leve devant la poitrine ; les offsets (elevation bras gauche +
        // abduction des deux epaules) sont ajoutes par applyViewmodelOffsets().
        armModel = glm::mat4(1.0f);
        armModel = glm::translate(armModel, glm::vec3(
            Constants::FirstPersonArms::FP_ARMS_OFFSET_X,
            Constants::FirstPersonArms::FP_ARMS_OFFSET_Y,
            Constants::FirstPersonArms::FP_ARMS_OFFSET_Z));
        armModel = glm::rotate(armModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // Echelle non-uniforme (voir constants.h) : pose "rest" = bras le long
        // du corps (mains a x=+-0.75 rig). X=0.65 pousse les mains vers les
        // coins inferieurs, Y/Z=0.50 conservent hauteur et profondeur.
        armModel = glm::scale(armModel, glm::vec3(
            Constants::FirstPersonArms::FP_ARMS_SCALE_X,
            Constants::FirstPersonArms::FP_ARMS_SCALE_Y,
            Constants::FirstPersonArms::FP_ARMS_SCALE_Z));
        view = glm::mat4(1.0f);
        viewPos = glm::vec3(0.0f);
        // Overlay : les bras passent devant le monde → on vide le depth buffer
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    shader->use();

    shader->setMat4("model", armModel);
    shader->setMat4("view", view);
    shader->setMat4("projection", shader->getProjection());
    shader->setVec3("viewPos", viewPos);

    // Envoyer les matrices des bones au shader.
    // En 1P : on copie les matrices et on collapse les bones configurés dans
    // res/armBones.json pour l'animation courante (échelle quasi-nulle →
    // vertices invisibles).
    const auto& animBoneMats = m_animator->getFinalBoneMatrices();
    std::vector<glm::mat4> boneMats1P; // copie modifiable (1P seulement)
    const std::vector<glm::mat4>* boneMatsPtr = &animBoneMats;
    if (!thirdPerson) {
        boneMats1P.assign(animBoneMats.begin(), animBoneMats.end());

        // Trouver la règle qui match l'animation courante (match sous-chaîne)
        const std::unordered_set<int>* bonesToHide = &m_defaultHiddenBones;
        const aiAnimation* curAnim = m_animator->getCurrentAnimation();
        if (curAnim) {
            const std::string animName(curAnim->mName.C_Str());
            for (const auto& [pattern, boneIds] : m_hideRules) {
                if (animName.find(pattern) != std::string::npos) {
                    bonesToHide = &boneIds;
                    break;
                }
            }
        }

        // Collapser les bones listés
        for (int boneId : *bonesToHide) {
            if (boneId >= 0 && boneId < static_cast<int>(boneMats1P.size())) {
                boneMats1P[boneId] = glm::scale(boneMats1P[boneId], glm::vec3(1e-4f));
            }
        }
        boneMatsPtr = &boneMats1P;
    }
    // Envoi groupé des matrices de bones : un seul glUniformMatrix4fv pour
    // tout le tableau (au lieu de ~128 appels + 128 hachages de string).
    // count est borné par SHADER_MAX_BONES, la taille du tableau uniform côté
    // shader (skinned.vert) — envoyer plus écrirait hors du tableau.
    const auto& boneMats = *boneMatsPtr;
    size_t count = boneMats.size() < SHADER_MAX_BONES ? boneMats.size() : SHADER_MAX_BONES;
    shader->setMat4Array("uBoneMatrices[0]", boneMats.data(), static_cast<int>(count));

    if (thirdPerson) {
        // 3P : on utilise les lumières du monde (point lights + flashlight)
        m_lightManager->applyToShader(shader);
        // Scene sombre (horreur) : on garantit un ambient minimum sur les bras
        // pour que la texture de peau reste lisible. Ne touche QUE le shader
        // skinned (utilise uniquement par les bras), pas le reste de la scene.
        shader->setVec3("dirLight.ambient", glm::vec3(0.45f, 0.42f, 0.38f));
    } else {
        // 1P : eclairage local en espace camera (le viewmodel suit la camera).
        // IMPORTANT : renseigner direction/position/cutOff de TOUTES les
        // lumieres. Les zeros par defaut (0,0,0) font normalize(vec3(0)) -> NaN
        // dans CalcDirLight/CalcSpotLight, et 0*NaN=NaN -> TOUT le viewmodel
        // devient NOIR quelle que soit la texture. C'etait la cause du "bras noir".
        shader->setVec3("dirLight.direction", glm::vec3(0.0f, 0.0f, 1.0f)); // depuis l'ecran
        shader->setVec3("dirLight.ambient",  Constants::FirstPersonArms::FP_ARMS_SKIN_COLOR * 0.8f);
        shader->setVec3("dirLight.diffuse",  Constants::FirstPersonArms::FP_ARMS_SKIN_COLOR * 0.5f);
        shader->setVec3("dirLight.specular", glm::vec3(0.3f));

        // spotLight desactive mais avec des champs valides (pas de NaN)
        shader->setVec3("spotLight.position",  glm::vec3(0.0f, 0.0f, -0.5f));
        shader->setVec3("spotLight.direction", glm::vec3(0.0f, 0.0f, -1.0f));
        shader->setVec3("spotLight.ambient",   glm::vec3(0.0f));
        shader->setVec3("spotLight.diffuse",   glm::vec3(0.0f));
        shader->setVec3("spotLight.specular",  glm::vec3(0.0f));
        shader->setFloat("spotLight.constant",     1.0f);
        shader->setFloat("spotLight.linear",       0.09f);
        shader->setFloat("spotLight.quadratic",    0.032f);
        shader->setFloat("spotLight.cutOff",       glm::radians(10.0f));
        shader->setFloat("spotLight.outerCutOff",  glm::radians(15.0f));

        // Pas de point light en 1P : la dirLight frontale suffit
        shader->setInt("numberLightSources", 0);
    }

    // Fallback texture blanche
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);

    shader->setInt("texture_diffuse", 0);
    shader->setInt("texture_specular", 1);

    debugPrintFirstDraw(armModel, meshes);

    for (auto* mesh : meshes) {
        mesh->draw();
    }
}

void FirstPersonArms::detectAnimations() {
	const aiScene* scene = m_model->getScene();
	if (!scene) return;

	for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
		std::string name(scene->mAnimations[i]->mName.C_Str());

		if (name.find("finger_gun_idle") != std::string::npos)
			m_idleAnimIndex = i;
		if (name.find("finger_gun_fire") != std::string::npos)
			m_fireAnimIndex = i;
		if (name.find("push") != std::string::npos) {
			if (name.find(".L") == std::string::npos && name.find("_L") == std::string::npos)
				m_pushAnimIndex = i;
		}
		if (name.find("grab") != std::string::npos) {
			if (name.find(".L") != std::string::npos || name.find("_L") != std::string::npos)
				m_grabAnimIndex = i;
		}
		if (name.find("relax") != std::string::npos)
			m_walkAnimIndex = i;
	}
}

void FirstPersonArms::debugPrintFirstDraw(const glm::mat4& armModel,
                                           const std::vector<Mesh*>& meshes) {
	static bool s_printed = false;
	if (s_printed) return;
	s_printed = true;
}
