#pragma once

#include <vector>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <memory>

#include "configKeys.h" // Contient la sensibilite par defaut de la souris
#include "LeftClick.h"

class Player;

// Classe Mouse : gere les mouvements de la souris pour le joueur
class Mouse {
public:
    // Constructeur
    // player : pointeur vers le joueur qui recevra les mouvements de souris
    Mouse(Player* player, MenuManager* menuManager);

    // Methode pour gerer le mouvement de la souris
    // xpos, ypos : position actuelle de la souris
    bool update(InputContext context, double xpos, double ypos);

	void setContext(InputContext context) { m_context = context; };
	InputContext getContext() { return m_context; };
private:
	std::map<std::string, Key*> m_keys; // Contient toutes les touches accessibles par leur nom
    std::map<InputContext, std::function<void(double xpos, double ypos)>> m_handleMovement;
	MenuManager* m_menuManager;
	InputContext m_context;
    Player* m_player;                   // Pointeur vers le joueur pour appliquer les rotations de la camera
    double m_xpos, m_ypos;              // Dernieres positions connues de la souris
    const float m_sensitivity;          // Sensibilite de la souris (vitesse de rotation)

    void setHandleMovements(InputContext context, std::function<void(double xpos, double ypos)> action) {
        m_handleMovement[context] = action;
    }
};
