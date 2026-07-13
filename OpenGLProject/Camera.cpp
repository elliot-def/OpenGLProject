#include "Camera.h"

Camera::Camera(glm::vec3 position, Direction* direction) :
    m_position(position),                     // Position de départ de la caméra
    m_direction(direction),                   // Point regardé
    m_upVector(glm::vec3(0.0f, 1.0f, 0.0f)),  // Définit l'axe vertical (Y+)
    m_renderer(nullptr)                       // Pas encore lié à un renderer
{
    // Ici tu peux ajouter d’autres initialisations si nécessaire
}

void Camera::update(Entity* entity) {
    // On place la caméra à la position et la direction de l’entité
    glm::vec3 eyeOffset = glm::vec3(0.f, 1.0f, 0.f); // yeux à 1.5m au-dessus des pieds
    m_position = entity->getPosition() + eyeOffset;

    m_front = entity->getDirectionVector();
}

glm::mat4 Camera::getViewMatrix() {
    // Petit debug dans la console pour voir la position de la caméra
    /*std::cout << "Camera Position: ("
        << m_position.x << ", "
        << m_position.y << ", "
        << m_position.z << ")\n";*/

    // glm::lookAt crée une matrice View en utilisant position, target et upVector
    return glm::lookAt(m_position, m_position+m_front, m_upVector);
}
