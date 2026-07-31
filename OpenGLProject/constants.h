#pragma once
// NOTE: this file MUST stay lowercase (constants.h, NOT Constants.h).
// MSVC on Windows is case-insensitive (so any TU can include it either way),
// but Linux/macOS builds are CASE-SENSITIVE and silently fail compilation
// on #include "Constants.h" mismatches. Keep this filename lowercase.
#include <cstdint>
#include <glm/vec3.hpp>

namespace Constants {
    // Window
    inline constexpr int WINDOW_WIDTH = 1920;
    inline constexpr int WINDOW_HEIGHT = 1080;
    inline constexpr const char* WINDOW_TITLE = "OpenGLProject";
    inline constexpr bool IS_WINDOW_FULLSCREEN = false;

    // Renderer
    inline constexpr int DEFAULT_FPS_CAPPING = 240;
    inline constexpr bool DEFAULT_IS_FPS_CAPPING = false;

    // Player

	inline constexpr float PLAYER_WALKING_SPEED = 2.5f; // unit : pixels per second
	inline constexpr float PLAYER_SPRINTING_SPEED = 4.0f; // unit : pixels per second
	inline constexpr float DEFAULT_PLAYER_RADIUS = 0.2f;
	inline constexpr float DEFAULT_PLAYER_HEIGHT = 1.6f;
    inline constexpr glm::vec3 PLAYER_EYE_HEIGHT = glm::vec3(0.0f, 1.0f, 0.0f); // Hauteur des yeux du joueur par rapport à sa position (en unités de jeu)

    // First-person arms
    // Rotation Y 180 deg dans FirstPersonArms::draw() :
    //  - +X rig (main GAUCHE du personnage) -> ecran GAUCHE (-X) : cotes
    //    gauche/droite corrects (pas d'effet miroir).
    //  - +Z rig (avant/paumes) pointe dans l'ecran : on voit le dos des mains,
    //    comme en vraie premiere personne.
    // PAS de rotation autour de X : le rig reste a l'endroit (Y vers le haut),
    // donc en pose "rest" (= bind pose, bras le long du corps, mains a x=+-0.75)
    // les bras pendent des epaules vers les COINS INFERIEURS de l'ecran.
    //
    // Echelle non-uniforme (X=0.65, Y=Z=0.50) place avec pose "rest" :
    //  - mains  : +-0.75*0.65 = +-0.49 -> COINS inferieurs (demi-largeur ~0.55)
    //  - coudes : +-0.56*0.65 = +-0.36 -> s'ecartent depuis les epaules
    //  - epaules: +-0.13*0.65 = +-0.08 -> bas-centre de l'ecran
    // OFFSET_Y=-0.70 : epaules (rig y=1.53*0.5=0.77) a y~+0.07, mains (rig
    // y=1.12*0.5=0.56) a y~-0.14 -> bras dans la moitie basse de l'ecran.
    inline constexpr float FP_ARMS_OFFSET_X =  0.00f;
    inline constexpr float FP_ARMS_OFFSET_Y = -0.70f;
    inline constexpr float FP_ARMS_OFFSET_Z = -0.55f;
    inline constexpr float FP_ARMS_SCALE_X  =  0.65f;
    inline constexpr float FP_ARMS_SCALE_Y  =  0.50f;
    inline constexpr float FP_ARMS_SCALE_Z  =  0.50f;

    // Attachement des bras au corps du joueur en 3e personne (world-space) :
    // le rig est en metres (epaules a y≈1.6, root a y≈1.47). OFFSET_Y positionne
    // le rig pour aligner les epaules sur le torse du joueur (~1.4 m).
    // SCALE = 1.0 : le rig est deja a l'echelle humaine (contrairement au 1P
    // ou on reduit pour le viewmodel).
    inline constexpr float FP_ARMS_3P_OFFSET_Y = -0.2f;
    inline constexpr float FP_ARMS_3P_SCALE    =  1.0f;

    // Couleur de peau pour l'éclairage des bras (ambient ≈ diffuse * 0.5)
    inline constexpr glm::vec3 FP_ARMS_SKIN_COLOR = glm::vec3(0.7f, 0.65f, 0.55f);

    // Bobbing (oscillation) des bras en première personne pendant la marche
    inline constexpr float FP_ARMS_BOB_SPEED     = 5.0f;   // Fréquence d'oscillation (rad/s)
    inline constexpr float FP_ARMS_BOB_AMPLITUDE = 0.04f;  // Amplitude verticale (mètres)

    // Animation de course : oscillation plus rapide et plus ample
    inline constexpr float FP_ARMS_RUN_SPEED       = 10.0f;  // Fréquence course (rad/s)
    inline constexpr float FP_ARMS_RUN_AMPLITUDE   = 0.07f;  // Amplitude verticale course (m)
    inline constexpr float FP_ARMS_RUN_Z_SWING     = 0.06f;  // Balancement avant/arrière (m)
    inline constexpr float FP_ARMS_RUN_PITCH       = 8.0f;   // Rotation avant/arrière (degrés)

    // Camera third-person
    inline constexpr float CAMERA_THIRD_PERSON_DISTANCE = 2.5f;  // Distance orbitale en 3e personne


    // Collision Manager

    static constexpr float GRAVITY = -9.81f;

    // Saut : Vélocité verticale initiale appliquée au joueur quand il saute
    // Avec GRAVITY = -9.81, V=12 m/s donne ~2.45 s en l'air (hauteur max ~7.3 m)
    inline constexpr float PLAYER_JUMP_VELOCITY = 12.0f;

    // Texture

	inline constexpr const char* TEXTURES_FOLDER_PATHS[] = { "./res/textures/", "./res/models/" };
	inline constexpr const unsigned int FIRST_TEXTURE_ID = 10;     // Starting ID for user textures
	inline constexpr const unsigned int BLACK_TEXTURE_ID = 0;      // ID for default black texture

    // Shader

    inline constexpr const char* SHADERS_FOLDER_PATH = "./res/shaders/";
    
    // Files
    
    inline constexpr const char PREFERED_SEPARATOR_PATH = '/';

    // Lights Shadering

    inline constexpr const int MAX_LIGHTS_SOURCES = 10;

    // Networking

    inline constexpr const int MAX_PACKET_SIZE = 1024;
    inline constexpr const char* SERVER_IP = "127.0.0.1";
    inline constexpr const int SERVER_PORT = 3333;

	inline constexpr uint16_t PACKET_MAGIC = 0xABCD; // Magic number for packet validation

    // Menu

    inline constexpr float MENU_TITLE_X = WINDOW_WIDTH/2.0f;
    inline constexpr float MENU_TITLE_Y = 300.0f;
    inline constexpr float MENU_TITLE_W = 600.0f;
	inline constexpr float MENU_TITLE_H = 150.0f;
	inline constexpr float MAINMENU_AFK_THRESHOLD = 6.0f; // Time in seconds before considering the player as AFK in the main menu
	inline constexpr float WEIRD_SOUND_INTERVAL = 12.00f;
	
    inline constexpr const char* JSON_OPTION_PATH = "./res/options.json";

	// Sound
	inline constexpr float DEFAULT_MASTER_VOLUME = 0.2f; // Volume maître par défaut (0.0 → ∞)
}

namespace Materials {
    // Shininess values for different materials in OpenGL
    // Range: 0.0 (matte) to 128.0 (very shiny)

    // Matte materials
    inline constexpr const float RUBBER = 10.0f;
    inline constexpr const float CLAY = 8.0f;
    inline constexpr const float CONCRETE = 5.0f;

    // Semi-matte materials
    inline constexpr const float WOOD = 15.0f;
    inline constexpr const float PLASTIC_MATTE = 20.0f;
    inline constexpr const float STONE = 12.0f;

    // Semi-glossy materials
    inline constexpr const float PLASTIC_GLOSSY = 32.0f;
    inline constexpr const float CERAMIC = 40.0f;
    inline constexpr const float MARBLE = 45.0f;

    // Glossy materials
    inline constexpr const float GLASS = 64.0f;
    inline constexpr const float POLISHED_WOOD = 50.0f;
    inline constexpr const float PAINTED_METAL = 55.0f;

    // Very shiny materials
    inline constexpr const float BRONZE = 76.8f;
    inline constexpr const float COPPER = 76.8f;
    inline constexpr const float BRASS = 83.2f;
    inline constexpr const float SILVER = 89.6f;
    inline constexpr const float GOLD = 83.2f;
    inline constexpr const float CHROME = 128.0f;
    inline constexpr const float POLISHED_METAL = 128.0f;
    inline constexpr const float MIRROR = 128.0f;

    // Special materials
    inline constexpr const float PEARL = 11.264f;
    inline constexpr const float JADE = 12.8f;
    inline constexpr const float OBSIDIAN = 38.4f;
    inline constexpr const float EMERALD = 76.8f;
    inline constexpr const float RUBY = 76.8f;
}

namespace Colors {
    // Shininess values for different materials in OpenGL
    // Range: 0.0 (matte) to 128.0 (very shiny)

    // Matte materials
    inline constexpr const glm::vec3 SHADOW_GREY = glm::vec3(35/255.0f, 31/255.0f, 32/255.0f);       // #231F20
	inline constexpr const glm::vec3 TOMATO_JAM = glm::vec3(187/255.0f, 68/255.0f, 48/255.0f);       // #BB4430
	inline constexpr const glm::vec3 TROPICAL_TEAL = glm::vec3(126/255.0f, 189/255.0f, 194/255.0f);  // #7EBDC2
	inline constexpr const glm::vec3 VANILLA_CUSTARD = glm::vec3(243/255.0f, 223/255.0f, 162/255.0f);// #F3DFA2
	inline constexpr const glm::vec3 LINEN = glm::vec3(239/255.0f, 230/255.0f, 221/255.0f);          // #EFE6DD
}