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

		// Chute : seuil de vélocité verticale (negatif) pour déclencher
		// l'animation de chute (Falling Idle). Doit être suffisamment
		// éloigné de 0 pour ne pas se déclencher lors de petits sauts.
		inline constexpr float FALL_VELOCITY_THRESHOLD = -1.5f;

		// Multiplicateur de vitesse post-chute / post-saut.
		// 1.0 = pas de réduction. 0.5 = demi-vitesse pendant
		// POST_LAND_SLOWDOWN_DURATION secondes après l'atterrissage.
		inline constexpr float POST_LAND_SPEED_FACTOR = 0.4f;

		// Durée (secondes) du ralenti après atterrissage.
		inline constexpr float POST_LAND_SLOWDOWN_DURATION = 0.6f;

		// Durée minimum (secondes) de l'animation "Falling To Landing".
		// Empêche l'animation d'être interrompue trop tôt.
		inline constexpr float LANDING_ANIM_MIN_DURATION = 0.5f;

		// Hauteur minimum (unités) de chute pour jouer l'animation "Falling To Landing".
		// Si la chute est inférieure à cette hauteur, on skip directement l'idle/walk.
		// 0.0 = toujours jouer l'anim de landing, 2.0 = fall-height >= 2 unités.
		inline constexpr float FALL_HEIGHT_LANDING_THRESHOLD = 1.5f;

		// Multiplicateur de vitesse post-chute (apres une chute suffisamment haute).
		// Plus la valeur est basse, plus le joueur est ralenti.
		// 0.33 = divise par 3, 0.5 = divise par 2.
		inline constexpr float FALL_SLOWDOWN_FACTOR = 0.35f;

		// Durée (secondes) du ralenti post-chute.
		inline constexpr float FALL_SLOWDOWN_DURATION = 0.8f;
	}
}