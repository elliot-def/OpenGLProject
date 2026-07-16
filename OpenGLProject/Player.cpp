#include "Player.h"
#include "Renderer.h"
#include "Direction.h"
#include "CollisionManager.h"

void Player::update() {
	updatePositionFromEnvironment(m_renderer->getDeltaTime());
}

void Player::draw(Shader* shader)
{
}

void Player::processDirectionKey(int direction) {
    float velocity = m_isSprinting ? Constants::PLAYER_SPRINTING_SPEED : Constants::PLAYER_WALKING_SPEED;
    float deltaTime = m_renderer->getDeltaTime();
    float distance = velocity * deltaTime;

    if (direction == EntityRelativeDirection::FORWARD)
        m_frameMovement += m_direction->getDirectionVector() * distance;
    if (direction == EntityRelativeDirection::BACKWARD)
        m_frameMovement -= m_direction->getDirectionVector() * distance;
    if (direction == EntityRelativeDirection::LEFT)
        m_frameMovement -= m_direction->rotateRight90KeepY() * distance;
    if (direction == EntityRelativeDirection::RIGHT)
        m_frameMovement += m_direction->rotateRight90KeepY() * distance;

    // En mode "sans gravité", ces touches permettent de monter/descendre à la volée
    if (direction == EntityRelativeDirection::UP)
        m_frameMovement += glm::vec3(0.0f, 1.0f, 0.0f) * distance;
    if (direction == EntityRelativeDirection::DOWN)
        m_frameMovement -= glm::vec3(0.0f, 1.0f, 0.0f) * distance;
}

void Player::processMouseMovements(double yaw, double pitch) {
	m_direction->addDelta(yaw, pitch);
}