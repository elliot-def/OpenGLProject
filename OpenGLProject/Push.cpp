#include "Push.h"
#include "FirstPersonArms.h"

Push::Push() : Key(nullptr, "Push", ConfigKeys::KEY_PUSH) {
    setOnPressAction(InputContext::GAME, [this]() {
        if (m_firstPersonArms) {
            m_firstPersonArms->triggerPush();
        }
    });
}

