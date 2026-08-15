#include "GlfwController.h"
#include "Log.h"

#include <cmath>
#include <cstdio>

GlfwController::GlfwController(int joystickId)
    : m_joystickId(joystickId) {
    // Etat initial : on considere le frame precedent vide pour que la
    // premiere frame ne soit pas interpretee comme une "nouvelle entree".
    m_prevState = GLFWgamepadstate{};
}

bool GlfwController::poll() {
    m_prevState = m_state;

    // La manette n'est pas forcement le premier joystick (autres
    // peripheriques, Steam Input, manette rebranchee sur un autre index...) :
    // on cherche le premier joystick reconnu comme gamepad.
    const bool wasConnected = m_connected;

    int foundId = -1;
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickIsGamepad(jid) == GLFW_TRUE) {
            foundId = jid;
            break;
        }
    }

    if (foundId >= 0) {
        // Manette presente : reset du delai de grace
        m_joystickId = foundId;
        m_disconnectGraceFrames = 0;
        m_connected = true;
    } else if (m_connected) {
        // Manette absente : delai de grace pour filtrer le clignotement au
        // branchement (re-enumeration USB, client Steam en arriere-plan qui
        // fait disparaitre la manette 1-2 frames puis la re-fait apparaitre).
        // On la considere encore connectee (etat vide : aucun input fantome)
        // tant que le delai n'est pas expire.
        if (m_disconnectGraceFrames < kDisconnectGraceFrames) {
            ++m_disconnectGraceFrames;
            m_state = GLFWgamepadstate{};
            return false;
        }
        m_connected = false;
    }

    if (m_connected != wasConnected) {
        if (m_connected) {
            logPrintf("[GlfwController] Manette XInput detectee (joystick %d)\n", m_joystickId);
        } else {
            logPrintf("[GlfwController] Manette XInput deconnectee\n");
        }
    }

    if (!m_connected) {
        m_state = GLFWgamepadstate{};
        return false;
    }
    if (glfwGetGamepadState(m_joystickId, &m_state) != GLFW_TRUE) {
        // Le joystick a ete debranche entre les deux appels
        m_state = GLFWgamepadstate{};
        m_connected = false;
        return false;
    }

    // Une manette vient d'etre branchee (hotplug) : considere comme une
    // activite, pour que la bascule de source d'entree + la notification
    // "Manette" apparaissent immediatement, sans attendre le premier appui.
    if (m_connected && !wasConnected) return true;

    // Activite = un bouton vient d'etre presse OU un axe vient de bouger
    for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; ++b) {
        if (isButtonJustPressed(b)) return true;
    }
    for (int a = 0; a <= GLFW_GAMEPAD_AXIS_LAST; ++a) {
        if (std::fabs(m_state.axes[a] - m_prevState.axes[a]) > 0.05f) return true;
    }
    return false;
}

bool GlfwController::isButtonPressed(int button) const {
    if (button < 0 || button > GLFW_GAMEPAD_BUTTON_LAST) return false;
    return m_connected && m_state.buttons[button] == GLFW_PRESS;
}

bool GlfwController::isButtonJustPressed(int button) const {
    if (button < 0 || button > GLFW_GAMEPAD_BUTTON_LAST) return false;
    return m_connected
        && m_state.buttons[button] == GLFW_PRESS
        && m_prevState.buttons[button] != GLFW_PRESS;
}

bool GlfwController::isButtonJustReleased(int button) const {
    if (button < 0 || button > GLFW_GAMEPAD_BUTTON_LAST) return false;
    return m_connected
        && m_state.buttons[button] != GLFW_PRESS
        && m_prevState.buttons[button] == GLFW_PRESS;
}

float GlfwController::getAxisValue(int axis) const {
    if (!m_connected || axis < 0 || axis > GLFW_GAMEPAD_AXIS_LAST) return 0.0f;
    float value = m_state.axes[axis];
    if (std::fabs(value) < STICK_DEADZONE) return 0.0f;
    return value;
}
