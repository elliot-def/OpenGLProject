#include "MenuManager.h"
#include "MainMenu.h"
#include "PauseMenu.h"
#include "OptionsMenu.h"
#include "TextRenderer.h"
#include "ShaderManager.h"
#include "Game.h"
#include "TextureManager.h"
#include "InputManager.h"
#include "Renderer.h"
#include "constants.h"
#include <chrono>

MenuManager::MenuManager(Game* game, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, TextureManager* textureManager, ShaderManager* shaderManager) :
    m_game(game), m_renderer(renderer), m_inputManager(nullptr), m_textRenderers(textRenderers), m_textureManager(textureManager), m_shaderManager(shaderManager), m_currentState(STATE_MENU), m_previousState(STATE_MENU) {
    initMenus();
}

MenuManager::~MenuManager() {
    delete m_mainMenu;
    delete m_pauseMenu;
    delete m_optionsMenu;
}

void MenuManager::initMenus() {
    m_mainMenu = new MainMenu(m_game, m_renderer, m_textRenderers, m_shaderManager);
    m_pauseMenu = new PauseMenu(m_game, m_textRenderers, m_shaderManager);
    m_optionsMenu = new OptionsMenu(m_game, m_previousState, m_textRenderers, m_shaderManager);
}

void MenuManager::changeState(GameState newState) {
    m_previousState = m_currentState;
    m_currentState = newState;
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
        return m_optionsMenu;
    default:
        printf("Aucun menu a afficher\n");
		return m_mainMenu;
        //throw std::runtime_error("Aucun menu a afficher");
    }
}

void MenuManager::updateHover(double mouseX, double mouseY) {
    getCurrentMenu()->updateHover(mouseX, mouseY);
}

void MenuManager::handleClick(double mouseX, double mouseY) {
    getCurrentMenu()->handleClick(mouseX, mouseY);
}

void MenuManager::update() {
    if (m_currentState == STATE_MENU) {
        auto lastInput = m_inputManager->getLastUpdateTime();
        auto now = std::chrono::system_clock::now();
        auto sec = std::chrono::duration<float>(now - lastInput).count();

        m_mainMenu->update(sec > Constants::MAINMENU_AFK_THRESHOLD);
    }
    else if (m_currentState == STATE_PAUSED) {
        m_pauseMenu->update();
    }
    else if (m_currentState == STATE_OPTIONS) {
        m_optionsMenu->update();
    }
    
    
}

void MenuManager::draw() {
    if (m_currentState != STATE_PLAYING) {
		// Désactive le depth test pour le rendu 2D du menu
        glDisable(GL_DEPTH_TEST);
        getCurrentMenu()->draw();
        glEnable(GL_DEPTH_TEST);
    }
}

GameState MenuManager::getCurrentState() const {
    return m_currentState;
}