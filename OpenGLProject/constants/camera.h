#pragma once

namespace Constants {
	namespace Camera {
		// Camera third-person
		inline constexpr float CAMERA_THIRD_PERSON_DISTANCE = 2.5f;  // Distance orbitale en 3e personne

		// Camera first-person : offset additionnel par rapport a la position des yeux
		// (s'ajoute a Constants::Player::PLAYER_EYE_HEIGHT). Reglable pour ajuster
		// la hauteur / profondeur de la vue 1P independamment de la hitbox.
		// Hauteur effective = 1.5 + CAMERA_FP_OFFSET_Y. -0.05 place la camera
		// juste au-dessus des epaules (~1.45m) pour eviter de voir le buste
		// quand l'animation de course penche le torse vers l'avant.
		inline constexpr float CAMERA_FP_OFFSET_X = 0.0f;
		inline constexpr float CAMERA_FP_OFFSET_Y = -0.05f;
		inline constexpr float CAMERA_FP_OFFSET_Z = 0.0f;

		// Decalage de la camera 1P dans la direction de deplacement du joueur.
		// Valeur positive = camera avance vers l'avant quand le joueur marche.
		inline constexpr float CAMERA_FP_MOVEMENT_OFFSET = 0.15f;

		// Vitesse de lissage (lerp) du decalage de mouvement en 1P.
		// Plus la valeur est elevee, plus le decalage reagit vite au changement
		// de direction (demarrage/arret). 10.0 = transition douce sans latence.
		inline constexpr float CAMERA_FP_MOVEMENT_LERP_SPEED = 10.0f;
	}
}