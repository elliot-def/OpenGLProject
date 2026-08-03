#pragma once

#include "Key.h"
#include "configKeys.h"

class FirstPersonArms;

class Grab : public Key {
public:
    Grab();

    void setFirstPersonArms(FirstPersonArms* arms) { m_firstPersonArms = arms; }

private:
    FirstPersonArms* m_firstPersonArms = nullptr;
};
