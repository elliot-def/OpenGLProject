#pragma once

#include "Key.h"
#include "configKeys.h"

class FirstPersonArms;

class Push : public Key {
public:
    Push();

    void setFirstPersonArms(FirstPersonArms* arms) { m_firstPersonArms = arms; }

private:
    FirstPersonArms* m_firstPersonArms = nullptr;
};
