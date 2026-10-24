#pragma once

// Reglages de la creature (fropy) : IA, deplacement, rondes, audition, rendu.
// References par Creature.h (k... = Constants::Creature::...).

namespace Constants {
	namespace Creature {
		inline constexpr float SPEED = 3.0f; // u/s (marche 2.5, sprint 5.5)
		inline constexpr float STOP_DISTANCE = 1.2f; // distance d'arrêt (« attaque »)
		inline constexpr float DEAGGRO_DISTANCE = 45.0f; // plus loin que ca du joueur le plus proche : agro perdue
		inline constexpr float LOSE_SIGHT_TIME = 6.0f; // s hors de vue avant de perdre l'agro
		inline constexpr float VISION_INTERVAL = 0.2f; // s entre deux tests de vision (cout)
		inline constexpr float SIGHT_STEP = 0.5f; // pas d'echantillonnage de la ligne de vue (m)
		inline constexpr float EYE_HEIGHT = 1.5f; // hauteur des yeux pour la ligne de vue
		inline constexpr float INVESTIGATE_TIME = 5.0f; // s passees sur place apres le dernier bruit
		inline constexpr float INVESTIGATE_STOP_DISTANCE = 1.5f;
		inline constexpr float PATROL_SPEED = 1.8f; // u/s pendant les rondes
		inline constexpr float PATROL_STOP_DISTANCE = 0.8f; // arrivee au point de ronde
		inline constexpr float PATROL_WAIT_MIN = 1.0f; // pause a chaque point (s)
		inline constexpr float PATROL_WAIT_MAX = 3.5f;
		inline constexpr float PATROL_STUCK_TIME = 3.0f; // s sans avancer -> nouveau point
		inline constexpr float PATROL_MIN_X = -27.0f;
		inline constexpr float PATROL_MAX_X = 27.0f;
		inline constexpr float PATROL_MIN_Z = -37.0f;
		inline constexpr float PATROL_MAX_Z = 17.0f;
		inline constexpr int PATROL_CELLS_X = 6;
		inline constexpr int PATROL_CELLS_Z = 6;
		inline constexpr float PATROL_DIST_WEIGHT = 0.5f; // s de "fraicheur" echangees contre 1 m de trajet
		inline constexpr float HEAR_OCCLUDED_FACTOR = 0.55f; // portee du bruit sans ligne de vue (murs, arbres)
		inline constexpr float NOISE_LOS_CACHE_TIME = 0.3f; // s de validite du test de ligne de vue d'un bruit
		inline constexpr float BODY_RADIUS = 0.35f; // rayon du sweepSphere
		inline constexpr float STEP_HEIGHT = 0.45f; // marche montee sans blocage (marches d'escalier ~0.28 m)
		inline constexpr float STEP_DOWN = 0.45f; // marche descendue en suivant le sol (au-dela : chute)
		inline constexpr float FOOT_RADIUS = 0.10f; // sonde de sol (sphere etroite sous le centre)
		inline constexpr float CELL_SIZE = 0.25f; // pas de la grille A* (< passage d une porte)
		inline constexpr float PATH_MARGIN = 10.0f; // marge autour du segment creature-joueur
		inline constexpr int MAX_EXPAND = 20000; // garde-fou cout A*
		inline constexpr float PATH_MARGIN_OTHER_FLOOR = 30.0f; // cible a un autre etage : marge elargie pour trouver l'escalier
		inline constexpr int MAX_EXPAND_OTHER_FLOOR = 60000;
		inline constexpr float SAME_FLOOR_HEIGHT = 1.0f; // denivele (m) au-dela duquel la cible est a un autre etage
		inline constexpr float CLIMB_RUN = 4.0f; // heuristique A* : m de marche par m de denivele (escaliers ~3.5)
		// Voice lines (CreatureVoice) : spatialisees a la tete de fropy.
		inline constexpr float VOICE_GAP_MIN = 4.0f;              // s entre deux lignes (tirage uniforme)
		inline constexpr float VOICE_GAP_MAX = 10.0f;
		inline constexpr float VOICE_REFERENCE_DISTANCE = 2.0f;   // m : plein volume en dessous
		inline constexpr float VOICE_MAX_DISTANCE = 35.0f;        // m : portee (= voix d'un joueur, PLAYER_VOICE_NOISE_RADIUS)
		inline constexpr float VOICE_OCCLUDED_HF = 0.2f;          // gain des aigus derriere un mur (passe-bas)
		inline constexpr float VOICE_OCCLUSION_INTERVAL = 0.25f;  // s entre deux tests d'occlusion
		inline constexpr float REPATH_INTERVAL = 0.5f; // s (voit les portes ouvertes/fermees)
		inline constexpr float HEAD_BASE_Y = 1.90f; // hauteur de la bouche (voice lines, cf. CreatureVoice)
		inline constexpr float TURN_RATE = 6.0f; // lissage du cap (1/s)
		// Modele articule (res/models/fropy/fropy_player.glb) : taille et animations.
		inline constexpr float MODEL_HEIGHT = 2.2f;      // m, tete comprise (plus grand que le joueur : 1.8)
		inline constexpr float WALK_ANIM_SPEED = 1.7f;   // m/s parcourus par le clip de marche a cette taille (cadence 1)
		inline constexpr float RUN_ANIM_SPEED = 4.0f;    // m/s parcourus par le clip de course
		inline constexpr float RUN_THRESHOLD = 2.4f;     // m/s : au-dessus, course (poursuite) ; en dessous, marche (ronde)
		// Portes : fropy les ouvre comme un joueur (portee + regard, Door::canInteract) quand elles
		// barrent sa trajectoire. L'A* les traverse avec un surcout (temps d'ouverture).
		inline constexpr float DOOR_PATH_COST = 6.0f;    // cases A* (0.25 m) ajoutees au passage d'une porte
		inline constexpr float DOOR_LOOKAHEAD = 2.5f;    // m de trajectoire testes devant fropy
	}
}
