#include "Entity.h"
#include "Player.h"
#include "Shader.h"
#include "CollisionManager.h"
#include "Renderer.h"
#include "Direction.h"
#include "Shader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

Entity::Entity(Renderer* renderer, CollisionManager* collisionManager)
	: m_position(glm::vec3(0.0f, 0.0f, 3.0f)),
	  m_direction(new Direction(-90.0f, 0.0f)),
	  m_renderer(renderer),
	  m_collisionManager(collisionManager)
{
	// Initialisation des attributs du joueur si nécessaire

}

Entity::~Entity() {

}

void Entity::update()
{
	// Logique de mise à jour du joueur (mouvement, interactions, etc.)

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
