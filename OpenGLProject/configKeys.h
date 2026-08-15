#pragma once

// GLFW sans les headers OpenGL (GLAD les fournit) : evite le conflit
// "OpenGL header already included" quand configKeys.h est inclus avant glad.h
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h> // Librairie pour la gestion des fenetres et des touches

// Namespace ConfigKeys : contient toutes les touches et configurations par defaut du jeu
namespace ConfigKeys {

    // Touches de deplacement
    inline const int KEY_FORWARD = GLFW_KEY_W;       // Avancer
    inline const int KEY_BACKWARD = GLFW_KEY_S;      // Reculer
    inline const int KEY_LEFT = GLFW_KEY_A;          // Aller a gauche
    inline const int KEY_RIGHT = GLFW_KEY_D;         // Aller a droite

    // Touches d'action
    inline const int KEY_CROUCH = GLFW_KEY_LEFT_CONTROL; // S'accroupir
    inline const int KEY_JUMP = GLFW_KEY_SPACE;          // Sauter
    inline const int KEY_SPRINT = GLFW_KEY_LEFT_SHIFT;   // Courir
    inline const int KEY_ESCAPE = GLFW_KEY_ESCAPE;       // Ouvrir le menu ou quitter
    inline const int KEY_FLASHLIGHT = GLFW_KEY_T;
    inline const int KEY_THIRD_PERSON = GLFW_KEY_C;
    inline const int KEY_PUSH = GLFW_KEY_R;
    inline const int KEY_GRAB = GLFW_KEY_E;
    inline const int KEY_NOCLIP = GLFW_KEY_N;
    inline const int KEY_DEBUG_HUD = GLFW_KEY_F3;      // HUD debug des animations (3P)

    inline const int MOUSE_LEFT_CLICK = GLFW_MOUSE_BUTTON_LEFT;         // Aller a droite
    inline const int MOUSE_RIGHT_CLICK = GLFW_MOUSE_BUTTON_RIGHT;       // Aller a droite

    // Sensibilite par defaut de la souris (pour rotation de la camera)
    inline const float DEFAULT_MOUSE_SENSITIVITY = 0.05f;

    // ── Manette Xbox (mapping GLFW standard gamepad) ───────────────────────
    // Boutons (index GLFW_GAMEPAD_BUTTON_*)
    inline const int CONTROLLER_JUMP = GLFW_GAMEPAD_BUTTON_A;            // A : saut
    inline const int CONTROLLER_CROUCH = GLFW_GAMEPAD_BUTTON_B;          // B : s'accroupir
    inline const int CONTROLLER_PUSH = GLFW_GAMEPAD_BUTTON_X;            // X : pousser (R au clavier)
    inline const int CONTROLLER_THIRD_PERSON = GLFW_GAMEPAD_BUTTON_Y;    // Y : 1P/3P (C au clavier)
    inline const int CONTROLLER_FLASHLIGHT = GLFW_GAMEPAD_BUTTON_LEFT_BUMPER;  // LB : torche (T au clavier)
    inline const int CONTROLLER_GRAB = GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER;       // RB : attraper (E au clavier)
    inline const int CONTROLLER_ESCAPE = GLFW_GAMEPAD_BUTTON_START;      // Start : menu / reprendre (Echap)
    inline const int CONTROLLER_NOCLIP = GLFW_GAMEPAD_BUTTON_BACK;       // Back : noclip (N au clavier)

    // Axes (index GLFW_GAMEPAD_AXIS_*)
    inline const int CONTROLLER_MOVE_X_AXIS = GLFW_GAMEPAD_AXIS_LEFT_X;  // Stick gauche : strafe
    inline const int CONTROLLER_MOVE_Y_AXIS = GLFW_GAMEPAD_AXIS_LEFT_Y;  // Stick gauche : avant/arriere
    inline const int CONTROLLER_LOOK_X_AXIS = GLFW_GAMEPAD_AXIS_RIGHT_X; // Stick droit : regard horizontal
    inline const int CONTROLLER_LOOK_Y_AXIS = GLFW_GAMEPAD_AXIS_RIGHT_Y; // Stick droit : regard vertical
    inline const int CONTROLLER_SPRINT_AXIS = GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER; // RT : sprinter

    // Sensibilite du stick droit (regard analogique, en degres/frame) :
    // plus c'est eleve, plus le regard tourne vite quand le stick est pousse.
    inline const float CONTROLLER_LOOK_SENSITIVITY = 3.0f;

    // Seuil (0..1) au-dessus duquel la gachette RT active le sprint
    inline const float CONTROLLER_TRIGGER_THRESHOLD = 0.5f;
}
