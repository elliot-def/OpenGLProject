#pragma once

#include "Key.h"         // Classe de base pour toutes les touches
#include "configKeys.h"  // Contient les touches et configurations par defaut

class MenuManager;
class Player;
class FirstPersonArms;

class LeftClick : public Key {
public:
    LeftClick(Player* player, MenuManager* MenuManager);

    // Destructeur vide
    virtual ~LeftClick() {}

    // Donne acces aux bras pour declencher l'animation de tir au clic gauche
    void setFirstPersonArms(FirstPersonArms* arms) { m_firstPersonArms = arms; }
private:
    MenuManager* m_menuManager;
    FirstPersonArms* m_firstPersonArms = nullptr;
};
