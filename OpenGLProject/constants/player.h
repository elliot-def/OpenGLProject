#pragma once

#include <glm/vec3.hpp>

namespace Constants {
	namespace Player {
		inline constexpr float PLAYER_WALKING_SPEED = 2.5f; // unit : pixels per second
		inline constexpr float PLAYER_SPRINTING_SPEED = 4.0f; // unit : pixels per second
		inline constexpr float DEFAULT_PLAYER_RADIUS = 0.2f;
		inline constexpr float DEFAULT_PLAYER_HEIGHT = 1.8f;

		inline constexpr glm::vec3 PLAYER_EYE_HEIGHT = glm::vec3(0.0f, 1.5f, 0.0f); // Hauteur des yeux du joueur par rapport à sa position (en unités de jeu)

		// Saut : Vélocité verticale initiale appliquée au joueur quand il saute
		// Avec GRAVITY = -9.81, V=12 m/s donne ~2.45 s en l'air (hauteur max ~7.3 m)
		inline constexpr float PLAYER_JUMP_VELOCITY = 6.0f;
	}
}