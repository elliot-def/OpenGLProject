#include "Entity.h"
#include "Player.h"
#include "Shader.h"
#include "CollisionManager.h"
#include "Renderer.h"
#include "Direction.h"
#include "Shader.h"
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

Entity::Entity(Renderer* renderer, CollisionManager* collisionManager)
	: m_position(glm::vec3(0.0f, 0.0f, 3.0f)),
	  m_direction(new Direction(-90.0, 0.0)),
	  m_ownsDirection(true),
	  m_renderer(renderer),
	  m_collisionManager(collisionManager)
{
	// Initialisation des attributs du joueur si nécessaire

}

Entity::~Entity() {
	if (m_ownsDirection)
		delete m_direction;
}

void Entity::update()
{
	// Spin sur place (rotation continue)
	if (m_spinSpeedDeg != 0.0f && m_renderer) {
		m_spinAngle = std::fmod(m_spinAngle + m_spinSpeedDeg * m_renderer->getDeltaTime(), 360.0f);
	}
}

void Entity::draw(Shader* shader) {}

void Entity::updatePositionFromEnvironment(float deltaTime) {
    // Sauvegarde la direction de deplacement (normalisee, horizontale seulement
    // pour ne pas influencer la hauteur de la camera) avant de reset m_frameMovement.
    // Utilise par Camera pour decaler la vue 1P dans la direction de marche.
    // Test sur la composante HORIZONTALE : un deplacement purement vertical (vol en
    // noclip) donnait normalize(0) = NaN, qui restait ensuite dans le lissage camera.
    const glm::vec3 horizontal(m_frameMovement.x, 0.0f, m_frameMovement.z);
    if (glm::length(horizontal) > 0.0001f) {
        m_lastMovementDirection = glm::normalize(horizontal);
    } else {
        m_lastMovementDirection = glm::vec3(0.0f);
    }

    // No-clip (gravite desactivee) : deplacement LIBRE, aucune collision (murs,
    // sol, objets traverses). En quittant le no-clip, la depenetration normale
    // (pushPlayerAway, ci-dessous) ressort le joueur d'un mur dans lequel il serait.
    if (!m_useGravity) {
        m_position += m_frameMovement;
        m_frameMovement = glm::vec3(0.0f);
        m_collisionManager->setVerticalVelocity(0.0f);   // pas de chute residuelle a la reprise de la gravite
        return;
    }

    // 1. On résout tout le mouvement de la frame d'un coup (Clavier + Gravité éventuelle)
    m_position = m_collisionManager->resolvePlayerMovement(
        m_position,
        m_frameMovement,
        deltaTime,
        m_useGravity // On passe l'attribut du joueur ici !
    );

    // On vide le mouvement accumulé pour la frame suivante
    m_frameMovement = glm::vec3(0.0f);

    // 2. Dépénétration active (Si un bloc mobile pousse le joueur)
    glm::vec3 pushedPos = m_collisionManager->pushPlayerAway(m_position);
    m_position = pushedPos;
}

glm::vec3 Entity::getDirectionVector() const { return m_direction->getDirectionVector(); } // Direction regardee

glm::mat4 Entity::getSpinRotation() const {
    if (m_spinSpeedDeg == 0.0f)
        return glm::mat4(1.0f);
    return glm::rotate(glm::mat4(1.0f), glm::radians(m_spinAngle), m_spinAxis);
}
