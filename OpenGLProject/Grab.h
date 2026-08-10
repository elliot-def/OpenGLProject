#pragma once

#include "Key.h"
#include "configKeys.h"

class FirstPersonArms;

// Touche Grab (E) : declenche l'animation grab des bras en 1P.
class Grab : public Key {
public:
    Grab();

    void setFirstPersonArms(FirstPersonArms* arms) { m_firstPersonArms = arms; }

private:
    FirstPersonArms* m_firstPersonArms = nullptr;
};
