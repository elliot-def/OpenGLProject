#pragma once

#include <glm/vec3.hpp>

namespace Constants {
	namespace Door {
		// ── Porte interactive (rectangle pivotant de 90 degres) ─────────────────

		// Dimensions par defaut d'une porte (en metres).
		inline constexpr float DEFAULT_WIDTH = 1.2f;
		inline constexpr float DEFAULT_HEIGHT = 2.2f;
		inline constexpr float DEFAULT_THICKNESS = 0.08f;

		// Ouverture : angle cible (degres, autour de la charniere) et vitesse de
		// rotation (deg/s). 90 deg/s -> ouverture/fermeture en 1 seconde.
		inline constexpr float OPEN_ANGLE = 90.0f;
		inline constexpr float OPEN_SPEED = 90.0f;

		// Interaction : distance maximale (m) entre les yeux du joueur et la
		// porte, + dot minimal entre le regard et la direction de la porte pour
		// considerer qu'on "regarde" dans sa direction (0.6 = cone de ~53 degres
		// autour de l'axe du regard).
		inline constexpr float INTERACT_RANGE = 3.0f;
		inline constexpr float INTERACT_LOOK_DOT = 0.6f;

		// Outline d'interaction (indique qu'on peut appuyer sur E) : contour
		// net autour de la porte (coque elargie dessinee AVANT la porte),
		// visible uniquement quand la porte est interactive.
		// Couleur du contour, opacite maximale (1 = pleine intensite), vitesse
		// du pulse (rad/s), vitesse du fondu d'apparition/disparition (1/s) et
		// epaisseur du contour (fraction de la taille de la porte).
		inline constexpr glm::vec3 OUTLINE_COLOR = glm::vec3(1.0f, 0.85f, 0.35f);
		inline constexpr float OUTLINE_OPACITY = 1.0f;
		inline constexpr float OUTLINE_PULSE_SPEED = 5.0f;
		inline constexpr float OUTLINE_FADE_SPEED = 8.0f;
		inline constexpr float OUTLINE_THICKNESS = 0.045f;

		// Couleur de base de la porte (matiere unie, sans texture).
		inline constexpr glm::vec3 DOOR_COLOR = glm::vec3(0.55f, 0.38f, 0.22f);
	}
}
