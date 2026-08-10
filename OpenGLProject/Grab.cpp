#include "Grab.h"
#include "FirstPersonArms.h"

Grab::Grab() : Key(nullptr, "Grab", ConfigKeys::KEY_GRAB) {
    setOnPressAction(InputContext::GAME, [this]() {
        if (m_firstPersonArms) {
            m_firstPersonArms->triggerGrab();
        }
    });
}

