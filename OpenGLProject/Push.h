#pragma once

#include "Key.h"
#include "configKeys.h"

class FirstPersonArms;

// Touche Push (R) : declenche l'animation push des bras en 1P.
class Push : public Key {
public:
    Push();

    void setFirstPersonArms(FirstPersonArms* arms) { m_firstPersonArms = arms; }

private:
    FirstPersonArms* m_firstPersonArms = nullptr;
};
