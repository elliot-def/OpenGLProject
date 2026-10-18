#include "Creature.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "ModelEntity.h"
#include "CollisionManager.h"
#include "CubeRenderer.h"
#include "Shader.h"

// Couleur du corps (émissive via le shader "cube/lightsource") : silhouette
// quasi noire, la tête de fropy est la seule partie colorée.
static const glm::vec3 kBodyColor(0.02f, 0.02f, 0.03f);

Creature::Creature(ModelEntity* head, const glm::vec3& spawnPosition)
    : m_head(head), m_position(spawnPosition) {
    m_position.y = 0.0f;
    syncHead();
}

void Creature::setPosition(const glm::vec3& position) {
    m_position = position;
    m_position.y = 0.0f;
    syncHead();
}

void Creature::update(float deltaTime, const glm::vec3& playerPos,
                      CollisionManager* collisionManager) {
    if (!(deltaTime > 0.0f)) deltaTime = 0.0f;

    glm::vec3 to = playerPos - m_position;
    to.y = 0.0f;
    const float dist = glm::length(to);
    m_moving = false;

    if (dist > 0.001f) {
        const glm::vec3 dir = to / dist;

        // Cap : lissage angulaire (chemin le plus court) vers le joueur.
        const float targetAngle = std::atan2(dir.x, dir.z);
        const float curAngle    = std::atan2(m_faceDir.x, m_faceDir.z);
        float delta = targetAngle - curAngle;
        while (delta > 3.14159265f)  delta -= 6.2831853f;
        while (delta < -3.14159265f) delta += 6.2831853f;
        const float blend = 1.0f - std::exp(-deltaTime * kTurnRate);
        const float angle = curAngle + delta * blend;
        m_faceDir = glm::vec3(std::sin(angle), 0.0f, std::cos(angle));

        // Poursuite : s'arrête à kStopDistance. Le sweepSphere résout les
        // collisions contre la carte (glissement le long des murs).
        if (dist > kStopDistance) {
            glm::vec3 move = dir * (kSpeed * deltaTime);
            if (collisionManager) {
                // Sphère remontée de kStepHeight : le sol et les obstacles bas
                // (tapis, seuils) ne la bloquent plus, seuls les murs comptent.
                const glm::vec3 center = m_position + glm::vec3(0.0f, kBodyRadius + kStepHeight, 0.0f);
                move = collisionManager->sweepSphere(center, move, kBodyRadius) - center;
                move.y = 0.0f;
            }
            m_position += move;
            m_position.y = 0.0f;
            m_walkTime += deltaTime;
            m_moving = glm::length(move) > 0.0001f;
        }
    }

    syncHead();
}

void Creature::syncHead() {
    if (!m_head) return;
    m_head->setPosition(m_position + glm::vec3(0.0f, kHeadBaseY, 0.0f));
    m_head->setScale(kHeadScale);
    // Convention Direction (degrés) : getModelMatrix oriente le modèle via
    // atan2(dirVec.x, dirVec.z), et la face de fropy regarde son +Z local →
    // pour faire face à (fx,fz) il faut dirVec=(fx,fz), soit yaw=atan2(fz,fx).
    const double yawDeg = glm::degrees(std::atan2(m_faceDir.z, m_faceDir.x));
    m_head->setDirection(Direction(yawDeg, 0.0));
}

void Creature::submitBody(CubeRenderer* cubeRenderer, Shader* bodyShader) const {
    if (!cubeRenderer || !bodyShader) return;

    const float bodyYaw = std::atan2(m_faceDir.x, m_faceDir.z);
    const glm::mat4 base = glm::translate(glm::mat4(1.0f), m_position)
                         * glm::rotate(glm::mat4(1.0f), bodyYaw, glm::vec3(0.0f, 1.0f, 0.0f));

    const float swing = m_moving
        ? std::sin(m_walkTime * kWalkCycleHz * 6.2831853f) * kSwingDeg
        : 0.0f;

    struct Part { glm::vec3 pos; glm::vec3 size; float swingDeg; };
    const Part parts[] = {
        { {  0.00f, 1.02f, 0.00f }, { 0.42f, 0.80f, 0.26f },  0.0f },   // torse
        { {  0.00f, 1.47f, 0.00f }, { 0.16f, 0.14f, 0.16f },  0.0f },   // cou
        { { -0.14f, 0.36f, 0.00f }, { 0.15f, 0.72f, 0.20f },  swing },  // jambe G
        { {  0.14f, 0.36f, 0.00f }, { 0.15f, 0.72f, 0.20f }, -swing },  // jambe D
        { { -0.32f, 1.06f, 0.02f }, { 0.12f, 0.62f, 0.15f }, -swing },  // bras G
        { {  0.32f, 1.06f, 0.02f }, { 0.12f, 0.62f, 0.15f },  swing },  // bras D
    };

    for (const Part& p : parts) {
        const glm::mat4 model = base
            * glm::translate(glm::mat4(1.0f), p.pos)
            * glm::rotate(glm::mat4(1.0f), glm::radians(p.swingDeg), glm::vec3(1.0f, 0.0f, 0.0f))
            * glm::scale(glm::mat4(1.0f), p.size);
        cubeRenderer->submit(bodyShader, {}, model, kBodyColor);
    }
}
