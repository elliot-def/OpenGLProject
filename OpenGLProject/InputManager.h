#pragma once

#include <vector>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>
#include <unordered_map>

#include "Key.h"

// Declarations anticipees pour eviter d'inclure tous les fichiers
class Game;
class MenuManager;
class Window;
class Player;
class Mouse;

// Classe InputManager : gere toutes les touches du jeu et leur etat
class InputManager {
public:
    // Constructeur
    // game : pointeur vers le jeu
    // window : pointeur vers la fenetre
    // player : pointeur vers le joueur
    // Appelle loadKeys() pour initialiser toutes les touches
    InputManager(Game* game, MenuManager* menuManager, Window* window, Player* player) : m_game(game), m_menuManager(menuManager), m_window(window), m_player(player) {
        loadKeys();
    }

    // Destructeur
    ~InputManager();

    // Retourne un pointeur vers une touche a partir de son nom
    Key* getKey(const std::string& name);
	std::chrono::system_clock::time_point getLastUpdateTime() { return m_lastUpdateTime; };
    InputContext getContext() { return m_context; };

    void setContext(InputContext context) { m_context = context; };

    // Methode appelee chaque frame pour mettre a jour l'etat de toutes les touches
    void update();

private:
    std::unordered_map<std::string, Key*> m_keys; // Contient toutes les touches accessibles par leur nom
	InputContext m_context = InputContext::MENU;  // Contexte actuel (jeu, menu, inventaire...)
    Mouse* m_mouse;                               // Pointeur vers la souris (pour gerer les actions liees a la souris)

    Player* m_player;               // Pointeur vers le joueur
    Game* m_game;                   // Pointeur vers le jeu
	MenuManager* m_menuManager;     // Pointeur vers le menu manager
    Window* m_window;               // Pointeur vers la fenetre

    std::chrono::system_clock::time_point m_lastUpdateTime;
    void resetLastUpdateTime() { m_lastUpdateTime = std::chrono::system_clock::now(); };

    // Charge et initialise toutes les touches du jeu
    void loadKeys();
};