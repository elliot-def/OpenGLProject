#pragma once
#include "InputManager.h"
#include "Key.h"
#include "Forward.h"
#include "Backward.h"
#include "Left.h"
#include "Right.h"
#include "Crouch.h"
#include "Jump.h"
#include "Sprint.h"
#include "Escape.h"
#include "Mouse.h"
#include "Flashlight.h"
#include "LeftClick.h"
#include "Window.h"

InputManager::~InputManager() {
	for (auto& pair : m_keys) {
		delete pair.second;
	}
	delete m_mouse;
}

Key* InputManager::getKey(const std::string& name) {
	auto it = m_keys.find(name);
	if (it != m_keys.end()) {
		return it->second;
	}
	throw std::out_of_range("Key not found: " + name);
}

void InputManager::loadKeys() {
	m_keys["Forward"] = new Forward(m_player);
	m_keys["Backward"] = new Backward(m_player);
	m_keys["Left"] = new Left(m_player);
	m_keys["Right"] = new Right(m_player);
	m_keys["Crouch"] = new Crouch(m_player);
	m_keys["Jump"] = new Jump(m_player);
	m_keys["Sprint"] = new Sprint(m_player);
	m_keys["Flashlight"] = new Flashlight(m_player);
	m_keys["Escape"] = new Escape(m_game);
	
	m_mouse = new Mouse(m_player, m_menuManager);
}

void InputManager::update() {
	bool actionPerformed = false;
	// Mouse movement & keys
	double xpos, ypos;
	glfwGetCursorPos(m_window->getGLFWwindow(), &xpos, &ypos);
	actionPerformed |= m_mouse->update(m_context, xpos, ypos);
	// Key states
	for (const auto& pair : m_keys) {
		Key* key = pair.second;
		int state = glfwGetKey(glfwGetCurrentContext(), key->getKey());
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

	if(actionPerformed) {
		resetLastUpdateTime();
	}
}