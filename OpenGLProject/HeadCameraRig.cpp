#include "HeadCameraRig.h"

#include <cctype>
#include <cmath>

#include "ModelEntity.h"
#include "Model.h"
#include "Animator.h"
#include "Camera.h"
#include "Player.h"
#include "LightManager.h"
#include "Log.h"

#include "constants/camera.h"

#include <glm/glm.hpp>

const std::string& HeadCameraRig::findHeadBone(ModelEntity* human) {
    if (human == m_cachedFor) return m_headBone;
    m_cachedFor = human;
    m_headBone.clear();
    // Premier bone dont le nom contient "head" (insensible a la casse), hors "HeadTop_End".
    for (const auto& [name, info] : human->getModel()->getBoneInfoMap()) {
        std::string lower = name;
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.find("head") != std::string::npos && lower.find("end") == std::string::npos) {
            m_headBone = name;
            break;
        }
    }
    return m_headBone;
}

void HeadCameraRig::update(ModelEntity* human, Camera* camera, Player* player, LightManager* lightManager) {
    if (!camera || !player || !lightManager) return;
    // Suivre l'os de tete du modele anime (1P ET 3P) : la lampe torche est
    // attachee a la tete, avec le meme decalage avant que la camera 1P
    // (CAMERA_FP_LOOK_OFFSET). En 1P, la camera suit aussi la tete.
    if (human && human->hasAnimations()) {
        Animator* anim = human->getAnimator();
        if (anim) {
            const std::string& headBone = findHeadBone(human);
            if (!headBone.empty()) {
                // Position de la tete en espace modele, convertie en monde
                glm::mat4 headMat = anim->getGlobalNodeTransform(headBone);
                glm::vec3 worldHead = glm::vec3(
                    human->getModelMatrix() * glm::vec4(glm::vec3(headMat[3]), 1.0f));

                glm::vec3 frontFlat = glm::normalize(glm::vec3(
                    camera->getFront().x, 0.0f, camera->getFront().z));
                // Meme decalage avant que la camera 1P : la source sort du
                // crane (plus de lumiere projetee a l'interieur du corps).
                glm::vec3 lookOffset = frontFlat * Constants::Camera::CAMERA_FP_LOOK_OFFSET;

                // ── Limite du lean avant de la tete ──────────────────────
                // L'animation de marche/sprint penche la tete vers l'avant.
                // Contre une hitbox, le corps s'arrete (collision) mais la
                // tete continue d'avancer -> la camera 1P traverse le mur. On
                // borne la composante AVANT de la tete par rapport au corps
                // (strafe et recul inchanges).
                {
                    const glm::vec3 playerPos = player->getPosition();
                    const glm::vec3 headRelXZ(worldHead.x - playerPos.x, 0.0f,
                                              worldHead.z - playerPos.z);
                    const float forwardRel = glm::dot(headRelXZ, frontFlat);
                    const float clampedForward = glm::min(forwardRel,
                        Constants::Camera::CAMERA_FP_HEAD_FORWARD_LIMIT);
                    const glm::vec3 lateralXZ = headRelXZ - frontFlat * forwardRel;
                    worldHead.x = playerPos.x + frontFlat.x * clampedForward + lateralXZ.x;
                    worldHead.z = playerPos.z + frontFlat.z * clampedForward + lateralXZ.z;
                }

                // ── Lampe torche attachee a la tete (1P et 3P) ──────────
                // update() (Scene) vient de la recaler sur les yeux ; on la
                // repositionne sur l'os de tete + offset avant du regard.
                lightManager->setFlashlightPosition(worldHead + lookOffset);

                // ── Camera 1P : suit la tete (X/Z ; le Y reste gere par
                // Camera::update()). Reapplique les offsets ecrases par le
                // setPosition : mouvement lisse + regard statique. ─────────
                if (!player->isThirdPerson()) {
                    glm::vec3 camPos = camera->getPosition();
                    glm::vec3 newCamPos = glm::vec3(worldHead.x, camPos.y, worldHead.z)
                        + camera->getSmoothedMovementOffset()
                        + lookOffset;

                    // Garde anti-ecran-bleu : une position non finie (NaN/inf,
                    // ex: transform de bone invalide) ou aberrante (> 10m du
                    // joueur) rendrait la camera invisible -> seule la couleur
                    // de clear est rastérisée. On retombe sur les yeux.
                    const bool finite = std::isfinite(newCamPos.x)
                        && std::isfinite(newCamPos.y) && std::isfinite(newCamPos.z);
                    const float distToPlayer = glm::length(newCamPos - player->getPosition());
                    if (finite && distToPlayer < 10.0f) {
                        camera->setPosition(newCamPos);
                    } else {
                        camera->setPosition(player->getEyePosition());
                        LOG_WARN("[Game] Position camera 1P invalide (bone='%s', "
                                 "finite=%d dist=%.2f), fallback sur les yeux",
                                 headBone.c_str(), finite ? 1 : 0, distToPlayer);
                    }
                }
            }

        }
    }

}
