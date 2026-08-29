#pragma once
#include <string>
#include <unordered_map>
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
class AudioSettingsMenu;
class VoiceChat;
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
    AudioSettingsMenu* m_audioSettingsMenu = nullptr;
    GameState m_currentState = STATE_MENU;
    GameState m_previousState;
    // Sous-menus affiches a la place des Options (STATE_OPTIONS)
    bool m_showKeyBindings = false;
    bool m_showControllerBindings = false;
    bool m_showAudioSettings = false;
    VoiceChat* m_voiceChat = nullptr;
    // Position manette sauvegardee par menu (clef : pointeur de menu, stable) :
    // restauree au retour dans un menu (sinon premier element).
    std::unordered_map<Menu*, int> m_savedFocus;

    // Sauvegarde la position manette du menu courant (a appeler AVANT de
    // changer de menu, pour pouvoir la restaurer au retour).
    void saveCurrentMenuFocus();
    // Restaure la position sauvegardee d'un menu, ou le premier element si le
    // menu n'a jamais ete visite (ou si sa liste a change).
    void restoreMenuFocus(Menu* menu);

    void initMenus();

    std::string stateToString(GameState state);
public:
    MenuManager(Game* game, SoundManager* soundManager, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textManagers, TextureManager* textureManager, ShaderManager* shaderManager, CursorManager* cursorManager, VoiceChat* voiceChat);
    ~MenuManager();

    Menu* getCurrentMenu();
    void setInputManager(InputManager* inputManager);

    // Navigation vers les sous-menus de reconfiguration / retour aux Options.
    // On sauvegarde toujours la position manette du menu quitte ; l'entree en
    // avant (sous-menu) repart du premier element, le retour aux Options
    // restaure la position sauvegardee.
    void showKeyBindings() { saveCurrentMenuFocus(); m_showKeyBindings = true; m_showControllerBindings = false; m_showAudioSettings = false; resetCurrentMenuFocus(); };
    void showControllerBindings() { saveCurrentMenuFocus(); m_showControllerBindings = true; m_showKeyBindings = false; m_showAudioSettings = false; resetCurrentMenuFocus(); };
    void showAudioSettings() { saveCurrentMenuFocus(); m_showAudioSettings = true; m_showKeyBindings = false; m_showControllerBindings = false; resetCurrentMenuFocus(); };
    void showOptions() { saveCurrentMenuFocus(); m_showKeyBindings = false; m_showControllerBindings = false; m_showAudioSettings = false; restoreMenuFocus(getCurrentMenu()); };

    // ── Navigation manette (discrète) ──
    // Transmet les déplacements/validations du stick gauche au menu courant
    // (voir Menu::navigate / activateSelected / adjustSelected / resetFocus).
    void navigateController(int direction);
    void activateControllerSelection();
    void adjustControllerSelection(int direction);
    void resetCurrentMenuFocus();

    // Retour manette (bouton B) : équivalent du bouton "Retour" du menu
    // courant (sous-menu -> Options, Options -> menu précédent). Sans effet
    // dans les menus sans menu précédent (principal, pause, jeu).
    void goBack();

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

    // changeState : restoreFocus=false (defaut) = entree en avant, la
    // selection manette repart du premier element ; true = retour vers un
    // menu deja visite, la position sauvegardee est restauree.
    void changeState(GameState newState, bool restoreFocus = false);
    GameState getCurrentState() const;
};
