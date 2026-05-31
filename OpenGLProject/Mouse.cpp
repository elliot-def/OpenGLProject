// OpenGL
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Mouse.h"
#include "Player.h"
#include "MenuManager.h"


Mouse::Mouse(Player* player, MenuManager* menuManager) : m_player(player), m_menuManager(menuManager), m_xpos(0.0), m_ypos(0.0),
    m_sensitivity(ConfigKeys::DEFAULT_MOUSE_SENSITIVITY), m_context(InputContext::MENU) {
    // Initialization of keys map with mouse buttons
    m_keys["LeftClick"] = new LeftClick(m_player, m_menuManager);

    setHandleMovements(InputContext::GAME, [this](double xpos, double ypos) {
        double xoffset = xpos - m_xpos;
        double yoffset = m_ypos - ypos; // Inverse : l'axe Y va de bas en haut
        m_xpos = xpos;
        m_ypos = ypos;
        xoffset *= m_sensitivity;
        yoffset *= m_sensitivity;

        // The angle of rotation up or down is also referred to as pitch;
        // the angle of rotation left or right is also referred to as yaw.

        m_player->processMouseMovements(xoffset, yoffset);
	});

    setHandleMovements(InputContext::MENU, [this](double xpos, double ypos) {
        m_menuManager->updateHover(xpos, ypos);
    });
}

/*
 * Methode appelee chaque frame pour gerer les mouvements de la souris et les clics
 * context : contexte actuel (jeu, menu...)
 * xpos, ypos : position actuelle de la souris
 * return : bool indiquant si une action a été effectuée
 */
bool Mouse::update(InputContext context, double xpos, double ypos) {
	bool actionPerformed = false;
    auto it = m_handleMovement.find(context);
    if (it != m_handleMovement.end() && it->second) {
        if ((m_xpos != xpos) or (m_ypos != ypos)) {
            it->second(xpos, ypos);
            m_xpos = xpos;
            m_ypos = ypos;
			actionPerformed = true;
        }
    }

    for (const auto& pair : m_keys) {
        Key* key = pair.second;
        int state = glfwGetMouseButton(glfwGetCurrentContext(), key->getKey());
        bool wasPressed = key->getStatus();
        if (state == GLFW_PRESS) {
            key->ifPressed(m_context);
			actionPerformed = true;
        }
        if (state == GLFW_PRESS && !wasPressed) {
            key->onPress(m_context);
        }
        else if (state == GLFW_RELEASE && wasPressed) {
            key->onRelease(m_context);
        }
    }
    return actionPerformed;
}