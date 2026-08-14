#include "Controller.h"

#include <cstdio>

// Diagnostic : liste ce que GLFW voit comme joysticks a un instant T.
// Sur Windows, GLFW ne lit que XInput : si Steam Input est actif pour le jeu,
// il peut cacher la manette du XInput et ce listing restera vide.
void Controller::logDevices(const char* tag) {
    printf("[Controller] %s :\n", tag);
    bool any = false;
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickPresent(jid) == GLFW_TRUE) {
            any = true;
            const char* name = glfwGetJoystickName(jid);
            const char* guid = glfwGetJoystickGUID(jid);
            printf("[Controller]   slot %d : \"%s\" guid=%s gamepad=%d\n",
                   jid,
                   name ? name : "(sans nom)",
                   guid ? guid : "(sans guid)",
                   glfwJoystickIsGamepad(jid) == GLFW_TRUE ? 1 : 0);
        }
    }
    if (!any) {
        printf("[Controller]   aucun joystick visible par GLFW\n");
    }
}
