#pragma once

#include <string>
#include <iostream>
#include <map>
#include <functional>

#include "Player.h" // Classe Player represente le joueur dans le jeu

enum class InputContext {
    GAME,
    MENU,
    INVENTORY,
    PAUSED
};

// Classe Key : classe de base abstraite pour toutes les touches du jeu
// Chaque touche derivee (Forward, Backward, Jump, Crouch...) herite de cette classe
class Key {
public:
    // Constructeur
    // player : pointeur vers le joueur associe a cette touche (peut etre nullptr pour des touches globales comme Escape)
    // name : nom de la touche (ex : "Forward", "Jump")
    // key : code de la touche physique (ex : GLFW_KEY_W)
    Key(Player* player, const std::string& name, int key) : m_name(name), m_key(key), m_player(player), m_isPressed(false) {}

    // Destructeur virtuel : necessaire pour l'heritage
    virtual ~Key() {}

    // Accesseurs
    const std::string& getKeyName() const { return m_name; } // Retourne le nom de la touche
    int getKey() const { return m_key; }                     // Retourne le code de la touche physique
    bool getStatus() const { return m_isPressed; }          // Retourne si la touche est actuellement appuyee

    void onPress(InputContext context) {
        m_isPressed = true; // Change l'etat pour indiquer que la touche est appuyee
        auto it = m_contextOnPress.find(context);
        if (it != m_contextOnPress.end() && it->second) {
            it->second(); // Exécuter l'action
        }
    }

    // Methode appelee lorsque la touche est relachee
    void onRelease(InputContext context) {
        m_isPressed = false; // Change l'etat pour indiquer que la touche est relachee
        auto it = m_contextOnRelease.find(context);
        if (it != m_contextOnRelease.end() && it->second) {
            it->second();
        }
    }

    // Methode appelee chaque frame si la touche est maintenue
    void ifPressed(InputContext context) {
        if (m_isPressed) {
            auto it = m_contextIfPress.find(context);
            if (it != m_contextIfPress.end() && it->second) {
                it->second();
            }
        }
    }

    // Méthodes pour configurer les actions par contexte
    void setOnPressAction(InputContext context, std::function<void()> action) {
        m_contextOnPress[context] = action;
    }

    void setOnReleaseAction(InputContext context, std::function<void()> action) {
        m_contextOnRelease[context] = action;
    }

    void setIfPressedAction(InputContext context, std::function<void()> action) {
        m_contextIfPress[context] = action;
    }
protected:
    Player* m_player;         // Pointeur vers le joueur associe a la touche
    const std::string m_name; // Nom de la touche
    const int m_key;          // Code de la touche physique
    bool m_isPressed;         // Etat de la touche (true si appuyee, false sinon)
	std::map<InputContext, std::function<void()>> m_contextOnRelease; // Actions specifiques par contexte
	std::map<InputContext, std::function<void()>> m_contextOnPress; // Actions specifiques par contexte
	std::map<InputContext, std::function<void()>> m_contextIfPress; // Actions specifiques par contexte
};
