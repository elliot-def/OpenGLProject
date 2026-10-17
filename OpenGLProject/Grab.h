#pragma once

#include <functional>

#include "Key.h"
#include "configKeys.h"

class FirstPersonArms;

// Touche Grab (E) : declenche l'animation grab des bras en 1P + l'interaction
// monde (ouvrir/fermer une porte proche, voir Scene::tryToggleDoor).
class Grab : public Key {
public:
    Grab();

    void setFirstPersonArms(FirstPersonArms* arms) { m_firstPersonArms = arms; }
    void setInteractCallback(std::function<void()> callback) { m_onInteract = std::move(callback); }

private:
    FirstPersonArms* m_firstPersonArms = nullptr;
    std::function<void()> m_onInteract;  // interaction monde (portes...)
};
