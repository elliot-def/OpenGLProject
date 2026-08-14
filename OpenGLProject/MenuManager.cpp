#include "MenuManager.h"
#include "MainMenu.h"
#include "PauseMenu.h"
#include "OptionsMenu.h"
#include "KeyBindingsMenu.h"
#include "ControllerBindingsMenu.h"
#include "TextRenderer.h"
#include "ShaderManager.h"
#include "Game.h"
#include "TextureManager.h"
#include "SoundManager.h"
#include "Sound.h"
#include "InputManager.h"
#include "Renderer.h"
#include "constants/menu.h"

#include <glad/glad.h>  // dessin 2D des menus: glEnable / glDisable / glBlendFunc
#include <chrono>

MenuManager::MenuManager(Game* game, SoundManager* soundManager, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, TextureManager* textureManager, ShaderManager* shaderManager, CursorManager* cursorManager) :
    m_game(game), m_soundManager(soundManager), m_renderer(renderer), m_inputManager(nullptr), m_textRenderers(textRenderers), m_textureManager(textureManager), m_shaderManager(shaderManager), m_cursorManager(cursorManager), m_currentState(STATE_MENU), m_previousState(STATE_MENU) {
    initMenus();
	changeState(STATE_MENU);
}

MenuManager::~MenuManager() {
    delete m_mainMenu;
    delete m_pauseMenu;
    delete m_optionsMenu;
    delete m_keyBindingsMenu;
    delete m_controllerBindingsMenu;
}

void MenuManager::initMenus() {
    m_mainMenu = new MainMenu(m_game, m_soundManager, m_renderer, m_textRenderers, m_shaderManager, m_cursorManager);
    m_pauseMenu = new PauseMenu(m_game, m_soundManager, m_textRenderers, m_shaderManager, m_cursorManager);
    m_optionsMenu = new OptionsMenu(m_game, m_soundManager, m_previousState, m_textRenderers, m_shaderManager, m_cursorManager);
    m_keyBindingsMenu = new KeyBindingsMenu(m_game, m_soundManager, m_textRenderers, m_shaderManager, m_cursorManager);
    m_controllerBindingsMenu = new ControllerBindingsMenu(m_game, m_soundManager, m_textRenderers, m_shaderManager, m_cursorManager);
}

void MenuManager::setInputManager(InputManager* inputManager) {
    m_inputManager = inputManager;
    // Les sous-menus Clavier & Souris / Manette ont besoin de l'InputManager
    // (bindings + capture + sensibilites deja chargees depuis res/keys.json).
    if (m_keyBindingsMenu) m_keyBindingsMenu->setInputManager(inputManager);
    if (m_controllerBindingsMenu) m_controllerBindingsMenu->setInputManager(inputManager);
}

void MenuManager::changeState(GameState newState) {
    m_previousState = m_currentState;
    m_currentState = newState;
    // En entrant dans les Options, on affiche la page principale (pas un
    // sous-menu laisse ouvert lors d'une session precedente)
    if (newState == STATE_OPTIONS) {
        m_showKeyBindings = false;
        m_showControllerBindings = false;
    }
    if (newState == STATE_MENU) {
		m_mainMenu->resetWeirdSoundPlayedTime();
		Sound* menuMusic = m_soundManager->get("menu_music");
        if (menuMusic) {
            menuMusic->play();
        }
    }
	else if (newState == STATE_PLAYING) {
        m_soundManager->stopAll();
	}
    // A l'entree d'un menu, la selection manette repart du premier element
    // (sans effet en jeu, getCurrentMenu() retourne nullptr).
    resetCurrentMenuFocus();
}

std::string MenuManager::stateToString(GameState state) {
    switch (state) {
    case STATE_MENU: return "Menu";
    case STATE_PLAYING: return "Jeu";
    case STATE_PAUSED: return "Pause";
    case STATE_OPTIONS: return "Options";
    default: return "Inconnu";
    }
}



Menu* MenuManager::getCurrentMenu() {
    switch (m_currentState) {
    case STATE_MENU:
        return m_mainMenu;
    case STATE_PAUSED:
        return m_pauseMenu;
    case STATE_OPTIONS:
        // Sous-menus affiches a la place des Options
        if (m_showKeyBindings) return m_keyBindingsMenu;
        if (m_showControllerBindings) return m_controllerBindingsMenu;
        return m_optionsMenu;
	case STATE_PLAYING:
		return nullptr; // Aucun menu à afficher pendant le jeu
    default:
        printf("Aucun menu a afficher\n");
		return m_mainMenu;
        //throw std::runtime_error("Aucun menu a afficher");
    }
}

void MenuManager::updateHover(double mouseX, double mouseY) {
    Menu* menu = getCurrentMenu();
    if (menu) menu->updateHover(mouseX, mouseY);
}

void MenuManager::handleClick(double mouseX, double mouseY) {
    Menu* menu = getCurrentMenu();
    if (menu) menu->handleClick(mouseX, mouseY);
}

void MenuManager::navigateController(int direction) {
    Menu* menu = getCurrentMenu();
    if (menu) menu->navigate(direction);
}

void MenuManager::activateControllerSelection() {
    Menu* menu = getCurrentMenu();
    if (menu) menu->activateSelected();
}

void MenuManager::adjustControllerSelection(int direction) {
    Menu* menu = getCurrentMenu();
    if (menu) menu->adjustSelected(direction);
}

void MenuManager::resetCurrentMenuFocus() {
    Menu* menu = getCurrentMenu();
    if (menu) menu->resetFocus();
}

void MenuManager::updateDrag(double mouseX, double mouseY, bool mousePressed) {
    Menu* menu = getCurrentMenu();
    if (menu) menu->updateDrag(mouseX, mouseY, mousePressed);
}

void MenuManager::update() {
    if (m_currentState == STATE_MENU) {
        auto lastInput = m_inputManager->getLastUpdateTime();

        auto now = std::chrono::system_clock::now();
        auto sec = std::chrono::duration<float>(now - lastInput).count();

        m_mainMenu->update(sec > Constants::Menu::MAINMENU_AFK_THRESHOLD);
    }
    else if (m_currentState == STATE_PAUSED) {
        m_pauseMenu->update();
    }
    else if (m_currentState == STATE_OPTIONS) {
        if (m_showKeyBindings) {
            m_keyBindingsMenu->update();
        }
        else if (m_showControllerBindings) {
            m_controllerBindingsMenu->update();
        }
        else {
            m_optionsMenu->update();
        }
    }
    
    
}

void MenuManager::draw() {
    if (m_currentState != STATE_PLAYING) {
		// Désactive le depth test pour le rendu 2D du menu
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        getCurrentMenu()->draw();
        glEnable(GL_DEPTH_TEST);
    }
}

void MenuManager::drawOverlays() {
    if (m_currentState == STATE_PLAYING) return;

    // Même état GL que MenuManager::draw() : blend actif, depth test désactivé
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    Menu* menu = getCurrentMenu();
    if (menu) menu->drawOverlays();

    glEnable(GL_DEPTH_TEST);
}

GameState MenuManager::getCurrentState() const {
    return m_currentState;
}
