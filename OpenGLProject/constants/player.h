#pragma once

#include <glm/vec3.hpp>

namespace Constants {
	namespace Player {
		inline constexpr float PLAYER_WALKING_SPEED = 2.5f; // unit : pixels per second
		inline constexpr float CREATURE_ATTACK_RANGE = 1.6f;   // m : fropy touche le joueur en dessous
		inline constexpr float PLAYER_HIT_COOLDOWN = 2.5f;     // s d'invulnerabilite apres un coup
		inline constexpr float CREATURE_STUN_TIME = 2.5f;      // s pendant lesquelles fropy reste fige apres avoir touche
		inline constexpr float PLAYER_VOICE_NOISE_RADIUS = 35.0f; // m : portee a laquelle fropy entend un joueur parler
		inline constexpr float PLAYER_SPRINTING_SPEED = 7.5f; // unit : pixels per second
		inline constexpr float PLAYER_NOCLIP_SPRINT_SPEED = 20.0f; // sprint en no-clip (vol libre) : traverser la carte vite
		inline constexpr float DEFAULT_PLAYER_RADIUS = 0.2f;
		inline constexpr float DEFAULT_PLAYER_HEIGHT = 1.8f;
		inline constexpr float PLAYER_STEP_HEIGHT = 0.8f; // m : hauteur max d'une marche montee automatiquement (marches ~0.28 m)

		inline constexpr glm::vec3 PLAYER_EYE_HEIGHT = glm::vec3(0.0f, 1.5f, 0.0f); // Hauteur des yeux du joueur par rapport à sa position (en unités de jeu)

		// Saut : Vélocité verticale initiale appliquée au joueur quand il saute
		// Avec GRAVITY = -9.81, V=12 m/s donne ~2.45 s en l'air (hauteur max ~7.3 m)
		inline constexpr float PLAYER_JUMP_VELOCITY = 6.0f;

		// Chute : seuil de vélocité verticale (negatif) pour déclencher
		// l'animation de chute (Falling Idle). Suffisamment bas (negatif) pour
		// NE PAS se declencher lors des micro-decollages en courant vite sur
		// une pente (descentes mesurees ~ -2 a -4 m/s) ni des petits sauts :
		// seules les vraies chutes (> ~1.3 m de hauteur libre) l'affichent.
		inline constexpr float FALL_VELOCITY_THRESHOLD = -5.0f;

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