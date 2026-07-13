#include "Player.h"
#include "CollisionManager.h"

void Player::update()
{
}

void Player::draw(Shader* shader)
{
}

void Player::processDirectionKey(int direction) {
    // Vitesse de déplacement du joueur
    float velocity;

    float deltaTime = m_renderer->getDeltaTime();

    if (m_isSprinting) {
        velocity = Constants::PLAYER_SPRINTING_SPEED * deltaTime;
	}
    else {
        velocity = Constants::PLAYER_WALKING_SPEED* deltaTime;
    }

    glm::vec3 desiredMovement(0.0f);

    if (direction == EntityRelativeDirection::FORWARD)
        desiredMovement += m_direction->getDirectionVector() * velocity;
    if (direction == EntityRelativeDirection::BACKWARD)
        desiredMovement -= m_direction->getDirectionVector() * velocity;
    if (direction == EntityRelativeDirection::LEFT)
        desiredMovement -= m_direction->rotateRight90KeepY() * velocity;
    if (direction == EntityRelativeDirection::RIGHT)
        desiredMovement += m_direction->rotateRight90KeepY() * velocity;
    if (direction == EntityRelativeDirection::UP)
        desiredMovement += glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
    if (direction == EntityRelativeDirection::DOWN)
        desiredMovement -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity;

    m_position = m_collisionManager->resolvePlayerMovement(
        m_position,
        desiredMovement,       // ton vecteur de déplacement calculé normalement
		deltaTime
    );
}

void Player::processMouseMovements(double yaw, double pitch) {
	m_direction->addDelta(yaw, pitch);
}