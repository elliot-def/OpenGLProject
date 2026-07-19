// OpenGL
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "LeftClick.h"
#include "MenuManager.h"

LeftClick::LeftClick(Player* player, MenuManager* menuManager) : Key(player, "LeftClick", ConfigKeys::MOUSE_LEFT_CLICK), m_menuManager(menuManager) {
	// Tant que le bouton est maintenu dans le contexte MENU : permet le drag des sliders (RangeInput)
	setIfPressedAction(InputContext::MENU, [this]() {
		double mouseX, mouseY;
		glfwGetCursorPos(glfwGetCurrentContext(), &mouseX, &mouseY);
		m_menuManager->updateDrag(mouseX, mouseY, true);
	});

	// Action a effectuer lorsque la touche est relachee dans le contexte MENU
	setOnReleaseAction(InputContext::MENU, [this]() {
		double mouseX, mouseY;
		glfwGetCursorPos(glfwGetCurrentContext(), &mouseX, &mouseY);
		m_menuManager->handleClick(mouseX, mouseY);
		// Stoppe tout drag en cours (isDragging = false) une fois le bouton relache
		m_menuManager->updateDrag(mouseX, mouseY, false);
	});
	setOnReleaseAction(InputContext::GAME, [this]() {
	});
}
