#pragma once

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

    inline const int MOUSE_LEFT_CLICK = GLFW_MOUSE_BUTTON_LEFT;         // Aller a droite
    inline const int MOUSE_RIGHT_CLICK = GLFW_MOUSE_BUTTON_RIGHT;       // Aller a droite

    // Sensibilite par defaut de la souris (pour rotation de la camera)
    inline const float DEFAULT_MOUSE_SENSITIVITY = 0.05f;
}
