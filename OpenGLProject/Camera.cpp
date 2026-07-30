#include "Camera.h"
#include "CollisionManager.h"

Camera::Camera(glm::vec3 position, Direction* direction) :
    m_position(position),
    m_direction(direction),
    m_upVector(glm::vec3(0.0f, 1.0f, 0.0f)),
    m_renderer(nullptr)
{
}

void Camera::update(Entity* entity) {
    m_front = entity->getDirectionVector();

    if (m_isThirdPerson) {
        // Snap au moment du toggle (evite le lerp depuis l'ancienne position 1P)
        bool snap = m_wasFirstPerson;
        m_wasFirstPerson = false;

        // Position desiree : orbite autour du joueur selon son angle de visee
        glm::vec3 playerEye = entity->getPosition() + Constants::PLAYER_EYE_HEIGHT;
        glm::vec3 desiredPos = playerEye - m_front * Constants::CAMERA_THIRD_PERSON_DISTANCE;

        // Sphere trace du joueur vers la position desiree
        glm::vec3 targetPos = desiredPos;

        if (m_collisionManager) {
            glm::vec3 dir = desiredPos - playerEye;
            float totalDist = glm::length(dir);
            const float step = 0.15f;
            const float camRadius = 0.2f;

            if (totalDist > 0.001f) {
                dir /= totalDist;
                float traveled = 0.0f;
                glm::vec3 bestPos = playerEye;

                while (traveled < totalDist) {
                    float nextStep = (traveled + step < totalDist) ? step : (totalDist - traveled);
                    glm::vec3 testPos = bestPos + dir * nextStep;
                    if (m_collisionManager->testSphereAll(testPos, camRadius).hit) {
                        break;
                    }
                    bestPos = testPos;
                    traveled += nextStep;
                }
                targetPos = bestPos;

                // Distance minimale pour eviter que la camera colle au joueur contre un mur
                float distToPlayer = glm::length(targetPos - playerEye);
                const float minDist = 0.4f;
                if (distToPlayer < minDist && totalDist > 0.001f) {
                    targetPos = playerEye - m_front * minDist;
                }
            } else {
                targetPos = playerEye;
            }
        }

        // Lerp pour lisser le mouvement de la camera
        if (snap) {
            m_position = targetPos;
        } else {
            float dt = m_renderer ? m_renderer->getDeltaTime() : 0.016f;
            const float smoothSpeed = 20.0f;
            float t = glm::clamp(smoothSpeed * dt, 0.0f, 1.0f);
            m_position = glm::mix(m_position, targetPos, t);
        }
    } else {
        m_wasFirstPerson = true;
        // Premiere personne : camera a hauteur des yeux
        m_position = entity->getPosition() + Constants::PLAYER_EYE_HEIGHT;
    }
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_upVector);
}
