#pragma once
#include <string>
#include "Menu.h"
#include "gamestate.h"

class Game;
class TextRenderer;
class InputManager;
class ShaderManager;
class TextureManager;
class SoundManager;
class MainMenu;
class PauseMenu;
class OptionsMenu;
class Renderer;

class MenuManager{
private:
	Game* m_game; // Pointeur vers le jeu principal
    std::vector<std::unique_ptr<TextRenderer>>* m_textRenderers;
    ShaderManager* m_shaderManager;
    Renderer* m_renderer;
    TextureManager* m_textureManager;
    SoundManager* m_soundManager;
    InputManager* m_inputManager;
    MainMenu* m_mainMenu;
    PauseMenu* m_pauseMenu;
    OptionsMenu* m_optionsMenu;
    GameState m_currentState = STATE_MENU;
    GameState m_previousState;

    void initMenus();

    std::string stateToString(GameState state);
public:
    MenuManager(Game* game, SoundManager* soundManager, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textManagers, TextureManager* textureManager, ShaderManager* shaderManager);
    ~MenuManager();

    Menu* getCurrentMenu();
    void setInputManager(InputManager* inputManager) { m_inputManager = inputManager; };

    void updateHover(double mouseX, double mouseY);
    void handleClick(double mouseX, double mouseY);

    void update();
    void draw();

    void changeState(GameState newState);
    GameState getCurrentState() const;
};