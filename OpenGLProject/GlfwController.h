#pragma once

#include "Controller.h"

// Mode hors-connexion : manette lue via GLFW/XInput (glfwGetGamepadState).
// C'est l'ancienne classe Controller, renommee apres extraction de
// l'interface commune. Le Sondage cherche le premier joystick reconnu
// comme gamepad (pas forcement GLFW_JOYSTICK_1).
class GlfwController : public Controller {
public:
    explicit GlfwController(int joystickId = GLFW_JOYSTICK_1);

    bool poll() override;
    bool isConnected() const override { return m_connected; }
    bool isButtonPressed(int button) const override;
    bool isButtonJustPressed(int button) const override;
    bool isButtonJustReleased(int button) const override;
    float getAxisValue(int axis) const override;

private:
    int m_joystickId;
    bool m_connected = false;
    GLFWgamepadstate m_state{};     // Etat courant (apres poll)
    GLFWgamepadstate m_prevState{}; // Etat du frame precedent (edge detection)

    // Delai de grace au debranchement : nombre de sondages consecutifs sans
    // manette avant de la considerer vraiment debranchee. Filtre le
    // clignotement (re-enumeration USB / client Steam) au branchement.
    static constexpr int kDisconnectGraceFrames = 30; // ~0.5s a 60 FPS
    int m_disconnectGraceFrames = 0;
};
