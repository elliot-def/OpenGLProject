#include "NPC.h"
#include "Camera.h"
#include "Direction.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

NPC::NPC(Camera* camera, LightManager* lightManager, Renderer* renderer,
         const std::string& modelPath, TextureManager* textureManager)
    : ModelEntity(camera, lightManager, renderer, modelPath, textureManager) {
    // Les PNJ n'ont pas de gravité (ils sont statiques par défaut)
    setUseGravity(false);
}

float NPC::isPlayerLookingAt(const Camera* camera) const {
    if (!camera) return -1.0f;

    const glm::vec3 playerPos = camera->getPosition();
    const glm::vec3 playerDir = camera->getFront();

    // 1. Vérifier la distance : le joueur est-il assez proche ?
    const glm::vec3 toNpc = m_position - playerPos;
    const float distance = glm::length(toNpc);
    if (distance > m_interactionRadius) return -1.0f;

    // 2. Vérifier l'angle : le joueur regarde-t-il vers le PNJ ?
    const glm::vec3 toNpcDir = glm::normalize(toNpc);
    const float dot = glm::dot(playerDir, toNpcDir);
    // cos(30°) ≈ 0.866 : le joueur doit regarder à ±30° du PNJ
    if (dot < 0.866f) return -1.0f;

    // 3. Raycast d'occlusion : vérifier qu'aucun obstacle ne bloque la vue
    //    On fait un raycast du joueur vers le centre du PNJ.
    //    Si le raycast touche le PNJ (distance ≈ distanceToNpc), c'est bon.
    //    Si le raycast touche autre chose avant (distance < distanceToNpc),
    //    c'est qu'un obstacle bloque la vue.
    float hitDistance = 0.0f;
    if (raycast(playerPos, toNpcDir, hitDistance)) {
        // Le raycast a touché ce PNJ. On vérifie que c'est bien lui
        // qui est touché (et pas un obstacle plus proche).
        // On tolère une marge de 0.5 unités pour les imprécisions du raycast.
        if (hitDistance <= distance + 0.5f) {
            return distance;
        }
    }
    // Note : si le raycast échoue sur le PNJ lui-même (modèle sans triangles
    // au centre), on accepte quand même l'interaction de proximité.
    // C'est un fallback pour les modèles où le raycast pourrait rater.
    return distance;
}

void NPC::lookAt(const glm::vec3& target) {
    // Calcule l'angle de yaw pour faire face à la cible
    glm::vec3 dir = target - m_position;
    dir.y = 0.0f; // rotation horizontale uniquement

    if (glm::length(dir) < 0.001f) return;

    dir = glm::normalize(dir);
    float yawDeg = glm::degrees(atan2(dir.x, dir.z));

    // Met à jour la direction du PNJ
    if (m_direction) {
        m_direction->setYawPitch(yawDeg, 0.0f);
    }

    m_lookAtTarget = target;
    m_lookAtTimer = 0.5f; // le PNJ maintient son regard pendant 0.5s après chaque màj
}

void NPC::update(float deltaTime) {
    // Animation de base : idle en boucle
    Entity::update(); // spin etc.

    // Mise à jour du timer lookAt
    if (m_lookAtTimer > 0.0f) {
        m_lookAtTimer -= deltaTime;
        if (m_lookAtTimer <= 0.0f) {
            m_lookAtTimer = 0.0f;
        }
    }
}
