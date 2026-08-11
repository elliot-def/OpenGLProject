#include "Player.h"
#include "Renderer.h"
#include "Direction.h"
#include "CollisionManager.h"
#include "Camera.h"

void Player::update() {
	updatePositionFromEnvironment(m_renderer->getDeltaTime());
}

void Player::draw(Shader* shader)
{
}

bool Player::canSprint() const {
    // Sprint autorisé uniquement si le joueur est au sol (grounded) ou en
    // no-clip (gravité désactivée) : pas de sprint en l'air.
    return m_collisionManager && (m_collisionManager->getIsPlayerGrounded() || !m_useGravity);
}

bool Player::getIsSprinting() const {
    // Filtre intégré : même si Shift est tenu, on ne sprinte pas en l'air.
    // Le sprint reprend automatiquement dès l'atterrissage (Shift tenu).
    return m_isSprinting && canSprint();
}

void Player::processDirectionKey(int direction) {
    // getIsSprinting() intègre déjà le filtre grounded/no-clip : en l'air,
    // on retombe sur la vitesse de marche même si la touche est tenue.
    // Le facteur post-atterrissage ralenti le joueur brievement apres une
    // chute ou un saut (CharacterAnimationController::getPostLandSpeedFactor).
    float velocity = getIsSprinting() ? Constants::Player::PLAYER_SPRINTING_SPEED : Constants::Player::PLAYER_WALKING_SPEED;
    velocity *= m_postLandSpeedFactor;
    float deltaTime = m_renderer->getDeltaTime();
    float distance = velocity * deltaTime;

    if (m_useGravity) {
        if (direction == EntityRelativeDirection::FORWARD)
            m_frameMovement += m_direction->getDirectionVectorKeepY() * distance;
        if (direction == EntityRelativeDirection::BACKWARD)
            m_frameMovement -= m_direction->getDirectionVectorKeepY() * distance;
        if (direction == EntityRelativeDirection::LEFT)
            m_frameMovement -= m_direction->rotateRight90KeepY() * distance;
        if (direction == EntityRelativeDirection::RIGHT)
            m_frameMovement += m_direction->rotateRight90KeepY() * distance;
        // UP : géré par processJump() en mode gravité (vrai saut à impulsion)
    }
    else {
        if (direction == EntityRelativeDirection::FORWARD)
            m_frameMovement += m_direction->getDirectionVector() * distance;
        if (direction == EntityRelativeDirection::BACKWARD)
            m_frameMovement -= m_direction->getDirectionVector() * distance;
        if (direction == EntityRelativeDirection::LEFT)
            m_frameMovement -= m_direction->rotateRight90KeepY() * distance;
        if (direction == EntityRelativeDirection::RIGHT)
            m_frameMovement += m_direction->rotateRight90KeepY() * distance;
        // Mode vol libre (sans gravité) : UP/DOWN permettent de monter/descendre
        if (direction == EntityRelativeDirection::UP)
            m_frameMovement += glm::vec3(0.0f, 1.0f, 0.0f) * distance;
        if (direction == EntityRelativeDirection::DOWN)
            m_frameMovement -= glm::vec3(0.0f, 1.0f, 0.0f) * distance;
    }
}

bool Player::isThirdPerson() const {
    return m_camera ? m_camera->isThirdPerson() : false;
}

void Player::processThirdPersonKey() {
    if (m_camera) {
        m_camera->toggleCameraMode();
    }
}

void Player::processJump() {
    // Délègue au CollisionManager : il appliquera l'impulsion uniquement
    // si le joueur est actuellement au sol (m_isPlayerGrounded).
    m_collisionManager->tryJump(Constants::Player::PLAYER_JUMP_VELOCITY);
}

void Player::processMouseMovements(double yaw, double pitch) {
	m_direction->addDelta(yaw, pitch);
}