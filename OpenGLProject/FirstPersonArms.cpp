#include "FirstPersonArms.h"
#include "Model.h"
#include "Log.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "LightManager.h"
#include "Animator.h"

#include "constants/firstpersonarms.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cctype>
#include <cmath>

FirstPersonArms::FirstPersonArms(Camera* camera, LightManager* lightManager,
                                 const std::string& modelPath, TextureManager* textureManager)
    : m_camera(camera), m_lightManager(lightManager),
      m_model(std::make_unique<Model>(camera, lightManager, modelPath, textureManager)),
      m_animator(std::make_unique<Animator>()) {

    m_animator->setup(m_model.get());

    // Pose commune 1P/3P : applyViewmodelOffsets() releve et ecarte les
    // deux bras. Le placement, l'echelle et la camera restent differents
    // selon le mode, mais les angles de pose sont identiques.

    // Jouer l'idle par defaut (chercher "finger_gun_idle" par nom)
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
        // Calcule la pose immediatement : un crossfade ulterieur prendra un
        // instantane valide (au lieu de matrices identite jamais calculees) et
        // le premier draw dispose deja de la pose correcte.
        m_animator->update(0.0f);

        // Trouver le bone racine du squelette (id=0, premier bone extrait
        // par Assimp → generalement le parent de toute la hierarchie).
        // Sert au tilt dynamique de l'animation relax (kRelaxTiltDeg).
        // Cache aussi les IDs des avant-bras pour le collapse 1P.
        for (const auto& [name, info] : boneMap) {
            if (info.id == 0) {
                m_spineBoneName = name;
            }
        }
    }

    // Texture blanche de fallback (1x1) si le modele n'a pas de textures
    unsigned char white[] = { 255, 255, 255, 255 };
    glGenTextures(1, &m_fallbackTexture);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

FirstPersonArms::~FirstPersonArms() {
    if (m_fallbackTexture)
        glDeleteTextures(1, &m_fallbackTexture);
}

void FirstPersonArms::update(float deltaTime, const glm::vec3& playerPos, bool isSprinting) {
    m_playerPos = playerPos;

    // Pose commune aux deux modes : les memes offsets d'elevation et
    // d'ecartement sont conserves en 1P comme en 3P. Seuls le placement du
    // modele, l'echelle et la matrice de vue changent entre les modes.
    // L'animation finger_gun_fire conserve donc aussi cette pose pendant le tir.
    if (!m_viewmodelOffsetsActive) {
        applyViewmodelOffsets();
        m_viewmodelOffsetsActive = true;
    }

    // ── Relax tilt : pencher l'animation de marche vers le bas ──────────
    // Applique/retire dynamiquement : l'offset de rotation sur le bone
    // racine (id=0) n'est actif QUE pendant l'animation "relax".
    // L'offset persiste jusqu'au prochain update(), donc le tilt apparait
    // des la frame suivante (delai imperceptible).
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
    // Detection du mouvement par la variation de position du joueur entre 2
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

    // L'animation cible depend de l'etat : "relax" (marche) si le joueur
    // bouge, finger_gun_idle sinon. Ne relance rien si c'est deja celle qui
    // joue (evite le reset de m_currentTime a chaque frame).
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

    // Debounce idle ↔ marche : evite les oscillations rapides quand le
    // joueur est pousse legerement (cube → position oscille de qq mm →
    // isMoving alterne → crossfade permanent). Le cooldown s'ecoule ici et
    // est reinitialise a chaque switch effectif.
    m_animSwitchCooldown -= deltaTime;

    // Garde : si le modele n'a pas charge (scene null), aucun index n'est
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
    }
}

void FirstPersonArms::triggerGrab() {
    if (m_grabAnimIndex >= 0 && m_animator) {
        m_animator->playAnimation(m_grabAnimIndex, false, false);
    }
}

void FirstPersonArms::applyViewmodelOffsets() {
    // Pose commune 1P/3P : bras DROIT deja leve par l'anim finger_gun_idle
    // (doigts en "finger gun"), bras GAUCHE releve en miroir par les offsets.
    //  - upper_arm.L : elevation (RotX +60°) PUIS ecartement (RotZ +spread) → la
    //    main gauche arrive a ~(-0.16,-0.19,-0.89) espace camera (fix_search.py).
    //  - forearm.L   : pli du coude (RotX +70°).
    //  - upper_arm.R : ecartement oppose (RotZ −spread) uniquement → ecarte la main
    //    droite de l'axe de visee (la pose elle-meme vient de l'animation).
    // Signe OPPOSE au bras gauche : les reperes locaux des deux bras sont en
    // miroir (meme signe des deux cotes rapprocherait les mains — simulation
    // spread_test.py). L'axe local X du bras gauche pointe vers le
    // haut-exterieur (angles POSITIFS = elevation vers l'avant) ; l'axe local
    // Z fait l'abduction. Angles reglables dans constants/firstPersonArms.h.
    const float upperArm = glm::radians(Constants::FirstPersonArms::FP_ARMS_BONE_UPPER_ARM_DEG);
    const float forearm  = glm::radians(Constants::FirstPersonArms::FP_ARMS_BONE_FOREARM_DEG);
    const float spread   = glm::radians(Constants::FirstPersonArms::FP_ARMS_SPREAD_DEG);
    const float spacing  = Constants::FirstPersonArms::FP_ARMS_SPACING;
    const glm::vec3 axisX(1.0f, 0.0f, 0.0f);
    const glm::vec3 axisZ(0.0f, 0.0f, 1.0f);
    // La translation d'ecartement est independante des rotations : modifier
    // FP_ARMS_SPACING deplace les bras sans changer les angles de pose.
    // Meme ordre de composition que la simulation : off = T(spacing) @ Rx(elev) @ Rz(spread)
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
        // ── 3e personne : bras attaches au corps du joueur (world-space) ──
        glm::vec3 front = m_camera->getFront();
        float yaw = atan2(front.x, front.z);
        armModel = glm::mat4(1.0f);
        armModel = glm::translate(armModel, m_playerPos + glm::vec3(0.0f, Constants::FirstPersonArms::FP_ARMS_3P_OFFSET_Y, 0.0f));
        armModel = glm::rotate(armModel, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        armModel = glm::scale(armModel, glm::vec3(Constants::FirstPersonArms::FP_ARMS_3P_SCALE));
        view = m_camera->getViewMatrix();
        viewPos = m_camera->getPosition();
    } else {
        // ── 1re personne : overlay viewmodel en espace camera ──
        armModel = glm::mat4(1.0f);
        armModel = glm::translate(armModel, glm::vec3(
            Constants::FirstPersonArms::FP_ARMS_OFFSET_X,
            Constants::FirstPersonArms::FP_ARMS_OFFSET_Y,
            Constants::FirstPersonArms::FP_ARMS_OFFSET_Z));
        armModel = glm::rotate(armModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
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

    // ── Filtrage des bones : ne garder que bras, coudes/avant-bras et mains ──
    // On collapse (echelle ~0) tous les bones SAUF ceux dont le nom contient
    // "upper_arm", "forearm" ou "hand" (mais PAS "handIK"). Torse, epaules,
    // doigts, IK et camera sont ainsi invisibles dans les deux modes (1P/3P).
    const auto& animBoneMats = m_animator->getFinalBoneMatrices();
    std::vector<glm::mat4> boneMatsFiltered;
    boneMatsFiltered.assign(animBoneMats.begin(), animBoneMats.end());

    // Construire le set des IDs de bones a GARDER visibles (one-shot, au
    // premier draw ou si la bone map change).
    if (!m_armBonesBuilt) {
        for (const auto& [name, info] : m_model->getBoneInfoMap()) {
            std::string lower = name;
            for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            // Garder : upper_arm, forearm, hand (mais PAS handIK)
            const bool isUpperArm = lower.find("upper_arm") != std::string::npos;
            const bool isForearm  = lower.find("forearm") != std::string::npos;
            const bool isHand     = lower.find("hand.") != std::string::npos;
            if (isUpperArm || isForearm || isHand) {
                m_armBoneIds.insert(info.id);
            }
        }
        m_armBonesBuilt = true;
    }

    // Collapser tous les bones qui ne sont PAS dans le set des bras
    for (int i = 0; i < static_cast<int>(boneMatsFiltered.size()); i++) {
        if (m_armBoneIds.find(i) == m_armBoneIds.end()) {
            boneMatsFiltered[i] = glm::scale(boneMatsFiltered[i], glm::vec3(1e-4f));
        }
    }

    // Envoi groupe des matrices de bones
    size_t count = boneMatsFiltered.size() < SHADER_MAX_BONES ? boneMatsFiltered.size() : SHADER_MAX_BONES;
    shader->setMat4Array("uBoneMatrices[0]", boneMatsFiltered.data(), static_cast<int>(count));

    if (thirdPerson) {
        // 3P : on utilise les lumieres du monde (point lights + flashlight)
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
