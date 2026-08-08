#pragma once

#include "Key.h"         // Classe de base pour toutes les touches
#include "configKeys.h"  // Contient les touches et configurations par defaut

class MenuManager;
class Player;

class LeftClick : public Key {
public:
    LeftClick(Player* player, MenuManager* MenuManager);

    // Destructeur vide
    virtual ~LeftClick() {}
private:
    MenuManager* m_menuManager;
};
