#pragma once
#include <string>
#include "Menu.h"
#include "gamestate.h"

class Game;
class TextRenderer;
class InputManager;
class ShaderManager;
class TextureManager;
class CursorManager;
class SoundManager;
class MainMenu;
class PauseMenu;
class OptionsMenu;
class KeyBindingsMenu;
class ControllerBindingsMenu;
class Renderer;

class MenuManager{
private:
	Game* m_game; // Pointeur vers le jeu principal
    std::vector<std::unique_ptr<TextRenderer>>* m_textRenderers;
    ShaderManager* m_shaderManager;
    Renderer* m_renderer;
    TextureManager* m_textureManager;
    CursorManager* m_cursorManager;
    SoundManager* m_soundManager;
    InputManager* m_inputManager;
    MainMenu* m_mainMenu;
    PauseMenu* m_pauseMenu;
    OptionsMenu* m_optionsMenu;
    KeyBindingsMenu* m_keyBindingsMenu;
    ControllerBindingsMenu* m_controllerBindingsMenu;
    GameState m_currentState = STATE_MENU;
    GameState m_previousState;
    // Sous-menus affiches a la place des Options (STATE_OPTIONS)
    bool m_showKeyBindings = false;
    bool m_showControllerBindings = false;

    void initMenus();

    std::string stateToString(GameState state);
public:
    MenuManager(Game* game, SoundManager* soundManager, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textManagers, TextureManager* textureManager, ShaderManager* shaderManager, CursorManager* cursorManager);
    ~MenuManager();

    Menu* getCurrentMenu();
    void setInputManager(InputManager* inputManager);

    // Navigation vers les sous-menus de reconfiguration / retour aux Options
    void showKeyBindings() { m_showKeyBindings = true; m_showControllerBindings = false; resetCurrentMenuFocus(); };
    void showControllerBindings() { m_showControllerBindings = true; m_showKeyBindings = false; resetCurrentMenuFocus(); };
    void showOptions() { m_showKeyBindings = false; m_showControllerBindings = false; resetCurrentMenuFocus(); };

    // ── Navigation manette (discrète) ──
    // Transmet les déplacements/validations du stick gauche au menu courant
    // (voir Menu::navigate / activateSelected / adjustSelected / resetFocus).
    void navigateController(int direction);
    void activateControllerSelection();
    void adjustControllerSelection(int direction);
    void resetCurrentMenuFocus();

    void updateHover(double mouseX, double mouseY);
    void handleClick(double mouseX, double mouseY);

    // A appeler chaque frame (bouton gauche enfonce ou non) pour permettre le drag
    // des sliders (RangeInput) du menu actuellement affiche.
    void updateDrag(double mouseX, double mouseY, bool mousePressed);

    void update();
    void draw();

    // Dessine les shapes de premier plan du menu courant (par-dessus le texte).
    // À appeler APRÈS le flush des TextRenderers (voir Game::run).
    void drawOverlays();

    void changeState(GameState newState);
    GameState getCurrentState() const;
};
