#include "Camera.h"

Camera::Camera(glm::vec3 position, Direction* direction) :
    m_position(position),                     // Position de d�part de la cam�ra
    m_direction(direction),                   // Point regard�
    m_upVector(glm::vec3(0.0f, 1.0f, 0.0f)),  // D�finit l'axe vertical (Y+)
    m_renderer(nullptr)                       // Pas encore li� � un renderer
{
    // Ici tu peux ajouter d�autres initialisations si n�cessaire
}

void Camera::update(Entity* entity) {
    // On place la cam�ra � la position et la direction de l�entit�
    m_position = entity->getPosition() + Constants::PLAYER_EYE_HEIGHT;

    m_front = entity->getDirectionVector();
}

glm::mat4 Camera::getViewMatrix() const {
    // Petit debug dans la console pour voir la position de la cam�ra
    /*std::cout << "Camera Position: ("
        << m_position.x << ", "
        << m_position.y << ", "
        << m_position.z << ")\n";*/

        // glm::lookAt cr�e une matrice View en utilisant position, target et upVector
    return glm::lookAt(m_position, m_position + m_front, m_upVector);
}
