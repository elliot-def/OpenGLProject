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
