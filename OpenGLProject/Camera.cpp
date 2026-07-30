#include "Camera.h"

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
        // Troisieme personne : camera derriere le joueur, regardant vers lui
        glm::vec3 behind = entity->getPosition() - m_front * Constants::CAMERA_THIRD_PERSON_DISTANCE;
        m_position = behind + glm::vec3(0.0f, Constants::CAMERA_THIRD_PERSON_HEIGHT, 0.0f);
        m_lookTarget = entity->getPosition() + Constants::PLAYER_EYE_HEIGHT;
    } else {
        // Premiere personne : camera a hauteur des yeux
        m_position = entity->getPosition() + Constants::PLAYER_EYE_HEIGHT;
    }
}

glm::mat4 Camera::getViewMatrix() const {
    if (m_isThirdPerson) {
        return glm::lookAt(m_position, m_lookTarget, m_upVector);
    }
    return glm::lookAt(m_position, m_position + m_front, m_upVector);
}
