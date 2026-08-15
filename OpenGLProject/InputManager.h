#pragma once

#include <vector>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <array>
#include <functional>
#include <chrono>

// GLFW sans les headers OpenGL (GLAD les fournit) : evite le conflit
// "OpenGL header already included" quand InputManager.h est inclus avant glad.h
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h> // GLFW_KEY_LAST (taille de la table de capture)

#include "Key.h"
#include "InputNotification.h"
#include "configKeys.h"

// Declarations anticipees pour eviter d'inclure tous les fichiers
class Game;
class MenuManager;
class Window;
class Player;
class Mouse;
class FirstPersonArms;
class Controller;
class ControllerKey;
class GlfwController;
class SteamInputController;

// Classe InputManager : gere toutes les touches du jeu et leur etat
class InputManager {
public:
    // Constructeur
    // game : pointeur vers le jeu
    // window : pointeur vers la fenetre
    // player : pointeur vers le joueur
    // Appelle loadKeys() pour initialiser toutes les touches
    InputManager(Game* game, MenuManager* menuManager, Window* window, Player* player);

    ~InputManager();

    // Retourne un pointeur vers une touche a partir de son nom
    Key* getKey(const std::string& name);
	std::chrono::system_clock::time_point getLastUpdateTime() { return m_lastUpdateTime; };
    InputContext getContext() { return m_context; };

    void setContext(InputContext context) { m_context = context; };

    // Source d'entree actuellement activee (clavier/souris ou manette) :
    // mise a jour dynamiquement dans update() a la moindre activite.
    InputSource getActiveSource() const { return m_activeSource; };

    // Notification de bascule clavier/souris <-> manette (dessinee par Game)
    InputNotification* getNotification() { return m_notification.get(); };

    // Transmet les bras au Mouse/LeftClick (tir), Push (R) et Grab (E)
    void setFirstPersonArms(FirstPersonArms* arms);

    // ── Bindings des touches (persistes dans res/keys.json) ──
    // Remplace la touche d'une action (met a jour la Key + sauvegarde JSON)
    void setKeyBinding(const std::string& action, int keyCode);
    // Code de touche courant d'une action (fallback sur la ConfigKeys si absente)
    int getKeyBinding(const std::string& action) const;

    // ── Capture de la prochaine touche (rebinding dans le menu Options) ──
    void beginKeyCapture();
    void cancelKeyCapture() { m_isCapturingKey = false; };
    bool isCapturingKey() const { return m_isCapturingKey; };
    // Callback appele quand une touche est capturee (keyCode, -1 = annule par Echap)
    void setOnKeyCaptured(std::function<void(int keyCode)> callback) { m_onKeyCaptured = std::move(callback); };

    // ── Bindings de la manette (persistes dans res/keys.json) ──
    // Remplace le bouton (GLFW_GAMEPAD_BUTTON_*) d'une action manette.
    void setControllerBinding(const std::string& action, int button);
    int getControllerBinding(const std::string& action) const;

    // ── Capture du prochain bouton de manette (rebinding manette) ──
    void beginControllerCapture();
    void cancelControllerCapture() { m_isCapturingControllerButton = false; };
    bool isCapturingControllerButton() const { return m_isCapturingControllerButton; };
    void setOnControllerButtonCaptured(std::function<void(int button)> callback) { m_onControllerButtonCaptured = std::move(callback); };

    // ── Sensibilites (reglees dans les menus Clavier&Souris / Manette) ──
    // Persistees dans res/keys.json avec les bindings (voir saveBindings).
    void setMouseSensitivity(float sensitivity);
    float getMouseSensitivity() const;  // defini dans le .cpp (Mouse incomplet ici)
    void setControllerLookSensitivity(float sensitivity);
    float getControllerLookSensitivity() const { return m_controllerLookSensitivity; };

    // Methode appelee chaque frame pour mettre a jour l'etat de toutes les touches
    void update();

private:
    std::unordered_map<std::string, std::unique_ptr<Key>> m_keys; // Contient toutes les touches accessibles par leur nom
    // Touches pilotees par la manette (boutons GLFW_GAMEPAD_BUTTON_*) :
    // mises a jour separement car leur etat vient de la manette, pas de glfwGetKey.
    std::unordered_map<std::string, std::unique_ptr<ControllerKey>> m_controllerKeys;
	InputContext m_context = InputContext::MENU;  // Contexte actuel (jeu, menu, inventaire...)
    // Derniere source d'entree utilisee (bascule dynamique + notification)
    InputSource m_activeSource = InputSource::KEYBOARD_MOUSE;
    // Etat de connexion de la manette active au frame precedent : sert a
    // detecter les branchements/debranchements a chaud (hotplug) pour
    // afficher la notification et basculer de source immediatement.
    bool m_lastControllerConnected = false;


    Player* m_player;               // Pointeur vers le joueur
    Game* m_game;                   // Pointeur vers le jeu
	MenuManager* m_menuManager;     // Pointeur vers le menu manager
    Window* m_window;               // Pointeur vers la fenetre
    FirstPersonArms* m_firstPersonArms = nullptr; // Bras 1P (Push/Grab manette)

    std::unique_ptr<Mouse> m_mouse;
    // Deux sources manette possibles : XInput (hors-connexion / repli) et
    // Steam Input (en ligne). m_controller pointe vers la source active.
    std::unique_ptr<GlfwController> m_glfwController;
    std::unique_ptr<SteamInputController> m_steamController;
    Controller* m_controller = nullptr;
    std::unique_ptr<InputNotification> m_notification; // Notification de bascule

    // Etat courant du sprint pilote par la manette (RT) : on ne touche au
    // joueur qu'au changement, pour ne pas ecraser la touche Shift clavier.
    bool m_controllerSprintActive = false;

    // Derniere position connue de la souris (detection de mouvement pour la bascule)
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    // Etat precedent du bouton gauche (edge detection pour la bascule)
    bool m_lastLeftButtonPressed = false;

    // ── Bindings action -> code de touche (persistes dans res/keys.json) ──
    std::unordered_map<std::string, int> m_keyBindings;
    // Bindings action -> bouton de manette (persistes aussi dans res/keys.json)
    std::unordered_map<std::string, int> m_controllerBindings;
    void loadKeyBindings();      // lit res/keys.json (clavier) avec fallback ConfigKeys
    void loadControllerBindings(); // lit res/keys.json (manette) avec fallback ConfigKeys
    void saveBindings();         // ecrit clavier + manette dans res/keys.json

    // ── Capture de touche (rebinding) ──
    bool m_isCapturingKey = false;
    // Etat du clavier au debut de la capture : seules les NOUVELLES pressions
    // comptent (une touche deja enfoncee au clic n'est pas capturee).
    std::array<bool, GLFW_KEY_LAST + 1> m_capturePrevState{};
    std::function<void(int)> m_onKeyCaptured;

    // ── Capture de bouton manette (rebinding) ──
    bool m_isCapturingControllerButton = false;
    std::function<void(int)> m_onControllerButtonCaptured;

    // ── Sensibilites (persistees dans res/keys.json avec les bindings) ──
    float m_mouseSensitivity = ConfigKeys::DEFAULT_MOUSE_SENSITIVITY;
    float m_controllerLookSensitivity = ConfigKeys::CONTROLLER_LOOK_SENSITIVITY;

    // ── Navigation manette dans les menus (stick gauche) ──
    // Transforme l'axe analogique en pas discrets avec front + répétition.
    struct AxisNav {
        int holdDir = 0;      // -1 / 0 / +1 (direction tenue)
        bool repeating = false;
        std::chrono::steady_clock::time_point holdStart{};
        std::chrono::steady_clock::time_point lastRepeat{};
    };
    AxisNav m_navAxisY; // haut/bas : déplacer la sélection
    AxisNav m_navAxisX; // gauche/droite : ajuster (sliders / listes)
    int axisStep(AxisNav& nav, float value);
    // Navigation dans les menus (appelée quand le contexte n'est pas GAME).
    void updateMenuNavigation();

    std::chrono::system_clock::time_point m_lastUpdateTime;
    void resetLastUpdateTime() { m_lastUpdateTime = std::chrono::system_clock::now(); };

    // Charge et initialise toutes les touches du jeu
    void loadKeys();
    // Charge les touches pilotees par la manette (boutons)
    void loadControllerKeys();
    // Met a jour le gameplay manette : sticks (regard/deplacement analogiques),
    // gachettes (sprint) et boutons (actions).
    // skipButton : bouton a ignorer ce frame (bouton fraichement capture pour
    // un rebinding, pour qu'il ne declenche pas aussi son action en jeu).
    void updateController(InputContext context, int skipButton = -1);
};