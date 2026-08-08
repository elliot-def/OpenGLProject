#include "InputManager.h"
#include "Game.h"
#include "Entity.h"
#include "PlayerKey.h"
#include "Escape.h"
#include "Mouse.h"
#include "Push.h"
#include "Grab.h"
#include "Window.h"

InputManager::InputManager(Game* game, MenuManager* menuManager, Window* window, Player* player)
    : m_game(game), m_menuManager(menuManager), m_window(window), m_player(player) {
    loadKeys();
}

InputManager::~InputManager() = default;

Key* InputManager::getKey(const std::string& name) {
	auto it = m_keys.find(name);
	if (it != m_keys.end()) {
		return it->second.get();
	}
	throw std::out_of_range("Key not found: " + name);
}

void InputManager::loadKeys() {
	m_keys["Forward"]   = std::make_unique<PlayerKey>(m_player, "Forward",   ConfigKeys::KEY_FORWARD,  nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::FORWARD); });
	m_keys["Backward"]  = std::make_unique<PlayerKey>(m_player, "Backward",  ConfigKeys::KEY_BACKWARD, nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::BACKWARD); });
	m_keys["Left"]      = std::make_unique<PlayerKey>(m_player, "Left",      ConfigKeys::KEY_LEFT,     nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::LEFT); });
	m_keys["Right"]     = std::make_unique<PlayerKey>(m_player, "Right",     ConfigKeys::KEY_RIGHT,    nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::RIGHT); });
	m_keys["Crouch"]    = std::make_unique<PlayerKey>(m_player, "Crouch",    ConfigKeys::KEY_CROUCH,   nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::DOWN); });

	m_keys["Jump"]      = std::make_unique<PlayerKey>(m_player, "Jump",      ConfigKeys::KEY_JUMP,
	    [this]() { m_player->processJump(); },                                         // onPress : saut normal
	    nullptr,
	    [this]() { m_player->processDirectionKey(EntityRelativeDirection::UP); });     // ifPressed : vol noclip
	m_keys["Sprint"]    = std::make_unique<PlayerKey>(m_player, "Sprint",    ConfigKeys::KEY_SPRINT,   [this]() { m_player->setIsSprinting(true); },
	                                                                                               [this]() { m_player->setIsSprinting(false); });
	m_keys["Flashlight"]  = std::make_unique<PlayerKey>(m_player, "Flashlight",  ConfigKeys::KEY_FLASHLIGHT,  nullptr, [this]() { m_player->processFlashLightKey(); });
	m_keys["ThirdPerson"] = std::make_unique<PlayerKey>(m_player, "ThirdPerson", ConfigKeys::KEY_THIRD_PERSON, nullptr, [this]() { m_player->processThirdPersonKey(); });

	m_keys["Escape"] = std::make_unique<Escape>(m_game);
	m_keys["Push"]   = std::make_unique<Push>();
	m_keys["Grab"]   = std::make_unique<Grab>();

	// Touche de debug : HUD des animations du personnage 3P (off par defaut)
	auto debugHudKey = std::make_unique<Key>(nullptr, "DebugHUD", ConfigKeys::KEY_DEBUG_HUD);
	debugHudKey->setOnPressAction(InputContext::GAME, [this]() { m_game->toggleDebugHUD(); });
	m_keys["DebugHUD"] = std::move(debugHudKey);

	m_keys["Noclip"] = std::make_unique<PlayerKey>(m_player, "Noclip", ConfigKeys::KEY_NOCLIP,
	    [this]() { m_player->setUseGravity(!m_player->isGravityEnabled()); });
	
	m_mouse = std::make_unique<Mouse>(m_player, m_menuManager);
}

void InputManager::update() {
	bool actionPerformed = false;
	double xpos, ypos;
	glfwGetCursorPos(m_window->getGLFWwindow(), &xpos, &ypos);
	actionPerformed |= m_mouse->update(m_context, xpos, ypos);
	for (const auto& pair : m_keys) {
		Key* key = pair.second.get();
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
