#include "Direction.h"

Direction::Direction(double yaw, double pitch) : m_yaw(yaw), m_pitch(pitch) {
	m_dirty = true;
}

glm::vec3 Direction::getDirectionVector() const {
	// Cache : la trigonometrie (cos/sin/radians) n'est recalculee que si
	// yaw/pitch ont change depuis le dernier appel (flag dirty).
	if (m_dirty) {
		glm::highp_vec3 direction;
		direction.x = static_cast<float>(cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)));
		direction.y = static_cast<float>(sin(glm::radians(m_pitch)));
		direction.z = static_cast<float>(sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)));
		m_cachedDirection = glm::normalize(direction); // Normalisation du vecteur (Norme=1, meme direction et sens)
		m_dirty = false;
	}
	return m_cachedDirection;
}

void Direction::addDelta(double deltaX, double deltaY) {
	m_yaw += deltaX;
	m_pitch += deltaY;
	m_dirty = true;
	++m_version;
	// Limiter l'inclinaison pour �viter le retournement
	if (m_pitch > 89.0)
		m_pitch = 89.0;
	if (m_pitch < -89.0)
		m_pitch = -89.0;
}

void Direction::setYawPitch(double yaw, double pitch) {
	m_yaw = yaw;
	m_pitch = pitch;
	m_dirty = true;
	++m_version;
}