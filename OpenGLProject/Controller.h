#pragma once

#include <GLFW/glfw3.h>

// ---------------------------------------------------------------------------
// Interface commune aux deux modes de lecture de la manette :
//   - GlfwController  : XInput via GLFW (mode hors-connexion)
//   - SteamInputController : Steam Input API (mode en ligne)
// Expose un etat normalise "boutons/axes" (indexes GLFW_GAMEPAD_BUTTON_* /
// GLFW_GAMEPAD_AXIS_*), pour que ControllerKey et InputManager restent
// independants de la source reelle.
// ---------------------------------------------------------------------------
class Controller {
public:
    virtual ~Controller() = default;

    // Sondage de l'etat courant (une fois par frame).
    // Retourne true si une nouvelle entree a eu lieu ce frame
    // (bouton juste presse ou axe venant de bouger).
    virtual bool poll() = 0;

    virtual bool isConnected() const = 0;

    virtual bool isButtonPressed(int button) const = 0;
    virtual bool isButtonJustPressed(int button) const = 0;
    virtual bool isButtonJustReleased(int button) const = 0;

    // Valeur d'un axe (GLFW_GAMEPAD_AXIS_*) apres zone morte :
    // 0.0f si |valeur| < deadzone, sinon la valeur brute.
    virtual float getAxisValue(int axis) const = 0;

    // True si l'axe depasse la zone morte.
    bool isAxisActive(int axis) const { return getAxisValue(axis) != 0.0f; }

    // Zone morte par defaut des sticks (la valeur d'un stick relache est
    // rarement exactement 0.0 : il faut la filtrer).
    static constexpr float STICK_DEADZONE = 0.15f;

    // Diagnostic : liste les joysticks vus par GLFW/XInput (voir Controller.cpp).
    static void logDevices(const char* tag);
};
