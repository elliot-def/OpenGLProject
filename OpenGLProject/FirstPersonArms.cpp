#include "FirstPersonArms.h"
#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "LightManager.h"
#include "Animator.h"

#include "constants/firstpersonarms.h"
#include "constants/window.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cfloat>   // FLT_MAX pour la AABB de debug
#include <cmath>    // isnan / isinf
#include <cstdio>

FirstPersonArms::FirstPersonArms(Camera* camera, LightManager* lightManager,
                                 const std::string& modelPath, TextureManager* textureManager)
    : m_camera(camera), m_lightManager(lightManager),
      m_model(std::make_unique<Model>(camera, lightManager, modelPath, textureManager)),
      m_animator(std::make_unique<Animator>()) {

    m_animator->setup(m_model.get());

    // Pose commune 1P/3P : applyViewmodelOffsets() relève et écarte les
    // deux bras. Le placement, l'échelle et la caméra restent différents
    // selon le mode, mais les angles de pose sont identiques.

    // Pré-calculer les noms d'uniforms des bones
    for (int i = 0; i < MAX_BONES; i++) {
        m_boneUniformNames.push_back("uBoneMatrices[" + std::to_string(i) + "]");
    }

    // Jouer l'idle par défaut (chercher "finger_gun_idle" par nom)
    const aiScene* scene = m_model->getScene();
    if (scene) {
        const auto& meshes  = m_model->getMeshes();
        const auto& boneMap = m_model->getBoneInfoMap();
        printf("[FPArms] scene OK, animations=%u, meshes=%zu, bones=%zu\n",
               scene->mNumAnimations, meshes.size(), boneMap.size());
        if (meshes.empty()) {
            printf("[FPArms] ERREUR: 0 mesh extrait! (FBX charge mais aucun mesh a dessiner)\n");
        } else {
            printf("[FPArms] mesh 0: %zu vertices, %zu indices\n",
                   meshes[0]->getVertices().size(), meshes[0]->getIndices().size());
        }
        if (boneMap.empty()) {
            printf("[FPArms] ATTENTION: 0 bone extrait — le rig est-il present dans le FBX?\n");
        } else {
            const auto& firstBone = *boneMap.begin();
            printf("[FPArms] premier bone: \"%s\" (id=%d), MAX_BONES=%d\n",
                   firstBone.first.c_str(), firstBone.second.id, MAX_BONES);
            // Lister TOUS les noms de bones — ce sont les noms exacts à
            // passer à m_animator->addBoneOffset("<nom>", offset).
            for (const auto& [name, info] : boneMap) {
                printf("[FPArms]   bone: \"%s\" (id=%d)\n", name.c_str(), info.id);
            }
            if (boneMap.size() > static_cast<size_t>(MAX_BONES)) {
                printf("[FPArms] ATTENTION: %zu bones > MAX_BONES (%d) — "
                       "les bones au-dela seront ignores (vertus degenerees possibles)!\n",
                       boneMap.size(), MAX_BONES);
            }
        }
        for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
            std::string name(scene->mAnimations[i]->mName.C_Str());
            printf("[FPArms] anim %u: \"%s\"\n", i, name.c_str());
            // Idle 1P/3P : "finger_gun_idle" (bras droit déjà levé en position
            // de visée par l'animation elle-même — doigts en "finger gun", pose
            // bien plus naturelle que rest+offsets). L'anim "rest" = bras le
            // long du corps (inutilisable en 1P, les mains resteraient en bas).
            if (name.find("finger_gun_idle") != std::string::npos) {
                m_idleAnimIndex = i;
            }
            if (name.find("finger_gun_fire") != std::string::npos) {
                m_fireAnimIndex = i;
            }
        }
        if (m_idleAnimIndex >= 0) {
            m_animator->playAnimation(m_idleAnimIndex, true);
        } else if (scene->mNumAnimations > 0) {
            printf("[FPArms] finger_gun_idle pas trouve, fallback anim 0\n");
            m_animator->playAnimation(0, true);
        }
    } else {
        printf("[FPArms] ERREUR: scene NULL!\n");
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

    // Pose commune aux deux modes : les mêmes offsets d'élévation et
    // d'écartement sont conservés en 1P comme en 3P. Seuls le placement du
    // modèle, l'échelle et la matrice de vue changent entre les modes.
    // L'animation finger_gun_fire conserve donc aussi cette pose pendant le tir.
    if (!m_viewmodelOffsetsActive) {
        applyViewmodelOffsets();
        m_viewmodelOffsetsActive = true;
    }

    m_animator->update(deltaTime);

    // Après la fin de l'animation de tir (non-loop), retour à l'idle.
    // Animator::update() a déjà calculé les matrices de la dernière frame du
    // tir avant que isFinished() ne soit testé. Sans recalcul ici, cette pose
    // reste envoyée au GPU pendant une frame supplémentaire, puis l'idle est
    // calculé seulement à la frame suivante : c'est le petit à-coup visible à
    // la fin du tir. Les clés finales de fire correspondent à la première pose
    // de idle ; on recalcule donc immédiatement la pose idle à t=0 sans faire
    // avancer son temps.
    if (m_fireAnimIndex >= 0 && m_animator->isFinished() &&
        m_animator->getCurrentAnimationName().find("finger_gun_fire") != std::string::npos) {
        m_animator->playAnimation(m_idleAnimIndex >= 0 ? m_idleAnimIndex : 0, true);
        m_animator->update(0.0f);
    }
}

void FirstPersonArms::triggerFire() {
    if (m_fireAnimIndex >= 0 && m_animator) {
        m_animator->playAnimation(m_fireAnimIndex, false);
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

    // Envoyer les matrices des bones au shader (noms pré-calculés)
    const auto& boneMats = m_animator->getFinalBoneMatrices();
    size_t count = boneMats.size() < m_boneUniformNames.size() ? boneMats.size() : m_boneUniformNames.size();
    for (size_t i = 0; i < count; i++) {
        shader->setMat4(m_boneUniformNames[i].c_str(), boneMats[i]);
    }

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

    // Diagnostique au premier draw : où finissent les bras en espace caméra ?
    // Replique le calcul GPU sur CPU (boneTransform pondéré + armModel) et
    // affiche la AABB résultat : si tout est hors frustum, on voit le problème.
    static bool s_armsDebugPrinted = false;
    if (!s_armsDebugPrinted) {
        s_armsDebugPrinted = true;
        printf("[FPArms] draw() appele, meshes=%zu\n", meshes.size());
        if (!meshes.empty()) {
            const auto& dbgTex = meshes[0]->getTextureIDs();
            printf("[FPArms] mesh 0 textures=%zu (slot0=%u, slot1=%u) — slot0 doit etre la peau\n",
                   dbgTex.size(), dbgTex.size() > 0 ? dbgTex[0] : 0u, dbgTex.size() > 1 ? dbgTex[1] : 0u);
            const auto& boneMatsDbg = m_animator->getFinalBoneMatrices();

            // Cause classique d'invisibilité : matrices de bones NaN/Inf.
            // Les vertices deviennent NaN → aucun raster, rien a l'ecran.
            bool hasNaN = false;
            for (size_t i = 0; i < boneMatsDbg.size(); i++) {
                for (int c = 0; c < 4; c++) {
                    for (int r = 0; r < 4; r++) {
                        const float v = boneMatsDbg[i][c][r];
                        if (std::isnan(v) || std::isinf(v)) { hasNaN = true; break; }
                    }
                }
            }
            printf("[FPArms] bones=%zu, NaN/Inf dans boneMatrices: %s\n",
                   boneMatsDbg.size(), hasNaN ? "OUI !!!" : "non");
            if (!boneMatsDbg.empty()) {
                printf("[FPArms] bone[0] (root) col0: (%.4f, %.4f, %.4f, %.4f)\n",
                       boneMatsDbg[0][0][0], boneMatsDbg[0][1][0],
                       boneMatsDbg[0][2][0], boneMatsDbg[0][3][0]);
            }

            glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
            for (const Vertex& v : meshes[0]->getVertices()) {
                glm::mat4 bt(0.0f);
                float wsum = 0.0f;
                for (int i = 0; i < 4; i++) {
                    const int id = v.m_boneIDs[i];
                    const float w = v.m_weights[i];
                    wsum += w;
                    if (id >= 0 && id < static_cast<int>(boneMatsDbg.size()) && w > 0.0001f) {
                        bt += boneMatsDbg[id] * w;
                    }
                }
                if (wsum < 0.001f) bt = glm::mat4(1.0f);
                const glm::vec4 p = armModel * (bt * glm::vec4(v.getPositions(), 1.0f));
                mn = glm::min(mn, glm::vec3(p));
                mx = glm::max(mx, glm::vec3(p));
            }
            printf("[FPArms] AABB espace-camera: min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)\n",
                   mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);

            // Positions VISUELLES des articulations (espace rig) : la pose 1P
            // attendue (simulation fix_search.py) est main.D≈(-0.25,1.57,0.24)
            // et main.G≈(0.22,1.57,0.35) — comparer ce dump au log pour vérifier.
            static const char* kDbgBones[] = {
                "shoulder.R", "upper_arm.R", "forearm.R", "hand.R",
                "shoulder.L", "upper_arm.L", "forearm.L", "hand.L",
            };
            const auto& dbgMap = m_model->getBoneInfoMap();
            for (const char* nm : kDbgBones) {
                auto bit = dbgMap.find(nm);
                if (bit == dbgMap.end() || bit->second.id < 0 ||
                    bit->second.id >= static_cast<int>(boneMatsDbg.size())) {
                    continue;
                }
                // finalMat * O^-1 = transformée VISIBLE du bone (G, et G*offset
                // pour les bones avec offset). La colonne 3 = position du joint.
                const glm::mat4 vis = boneMatsDbg[bit->second.id] * glm::inverse(bit->second.offsetMatrix);
                printf("[FPArms]   joint %-12s rig=(%.3f, %.3f, %.3f)\n",
                       nm, vis[3][0], vis[3][1], vis[3][2]);
            }
            // Frustum perspective 60°V, near=0.1 : z doit etre dans [-100, -0.1],
            // |x| <= tan(30°)*|z|*aspect, |y| <= tan(30°)*|z|.
            const float aspect = (float)Constants::Window::WINDOW_WIDTH / (float)Constants::Window::WINDOW_HEIGHT;
            const float halfH = tanf(glm::radians(30.0f)) * std::max(std::abs(mx.z), std::abs(mn.z));
            const float halfW = halfH * aspect;
            printf("[FPArms] test frustum: z=[%.3f, %.3f] (ok si < -0.1), "
                   "|x|<%.2f ok=%d, |y|<%.2f ok=%d\n",
                   mn.z, mx.z, halfW, (std::max(std::abs(mx.x), std::abs(mn.x)) < halfW),
                   halfH, (std::max(std::abs(mx.y), std::abs(mn.y)) < halfH));
        }
        GLenum err = glGetError();
        while (err != GL_NO_ERROR) {
            printf("[FPArms] glGetError: 0x%04X\n", err);
            err = glGetError();
        }
    }

    for (auto* mesh : meshes) {
        mesh->draw();
    }
}
