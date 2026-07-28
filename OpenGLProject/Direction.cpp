#include "Direction.h"

Direction::Direction(double yaw, double pitch) : m_yaw(yaw), m_pitch(pitch) {}

glm::vec3 Direction::getDirectionVector() {
	glm::highp_vec3 direction;
	direction.x = static_cast<float>(cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)));
	direction.y = static_cast<float>(sin(glm::radians(m_pitch)));
	direction.z = static_cast<float>(sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)));
	glm::highp_vec3 normalized_direction = glm::normalize(direction); // Normalisation du vecteur (Norme=1, meme direction et sens)
	return normalized_direction;
}

void Direction::addDelta(double deltaX, double deltaY) {
	m_yaw += deltaX;
	m_pitch += deltaY;
	// Limiter l'inclinaison pour �viter le retournement
	if (m_pitch > 89.0)
		m_pitch = 89.0;
	if (m_pitch < -89.0)
		m_pitch = -89.0;
}