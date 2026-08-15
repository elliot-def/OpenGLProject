#pragma once

#include "Controller.h"

#include <array>
#include <unordered_map>

// Steam Input (mode en ligne). Types de l'API Steam Input.
#include "dependencies/steam/isteaminput.h"

// Mode en ligne : manette lue via l'API Steam Input (ISteamInput).
// Les actions semantiques du manifest (res/steam_input_manifest.vdf) sont
// traduites vers les memes indexes boutons/axes GLFW_GAMEPAD_* que le mode
// XInput, pour que ControllerKey et InputManager soient identiques quel que
// soit le mode.
class SteamInputController : public Controller {
public:
    SteamInputController();
    ~SteamInputController() override;

    bool poll() override;
    bool isConnected() const override { return m_connected; }
    bool isButtonPressed(int button) const override;
    bool isButtonJustPressed(int button) const override;
    bool isButtonJustReleased(int button) const override;
    float getAxisValue(int axis) const override;

    // true si Steam Input a pu s'initialiser (action set + actions resolues).
    // Si false, InputManager se replie sur GlfwController (XInput).
    bool isAvailable() const { return m_available; }

private:
    bool m_initialized = false; // SteamInput()->Init() a reussi
    bool m_available = false;   // action set + actions resolues
    bool m_connected = false;   // au moins une manette Steam Input connectee
    InputHandle_t m_handle = 0; // premiere manette (suffisant en solo)

    InputActionSetHandle_t m_actionSet = 0;
    // Boutons : index GLFW_GAMEPAD_BUTTON_* -> handle d'action digitale Steam
    std::unordered_map<int, InputDigitalActionHandle_t> m_digital;
    // Axes : une action analogique par groupe
    InputAnalogActionHandle_t m_move = 0;   // stick gauche (x/y)
    InputAnalogActionHandle_t m_look = 0;   // stick droit (x/y)
    InputAnalogActionHandle_t m_sprint = 0; // gachette droite (x)

    std::array<bool, GLFW_GAMEPAD_BUTTON_LAST + 1> m_buttons{};
    std::array<bool, GLFW_GAMEPAD_BUTTON_LAST + 1> m_prevButtons{};
    std::array<float, GLFW_GAMEPAD_AXIS_LAST + 1> m_axes{};
    std::array<float, GLFW_GAMEPAD_AXIS_LAST + 1> m_prevAxes{};

    // Delai de grace au debranchement : nombre de sondages consecutifs sans
    // manette avant de la considerer vraiment debranchee. Filtre le
    // clignotement au branchement (Steam re-applique la config a chaud).
    static constexpr int kDisconnectGraceFrames = 30; // ~0.5s a 60 FPS
    int m_disconnectGraceFrames = 0;

    bool init();
};
