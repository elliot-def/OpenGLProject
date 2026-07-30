#pragma once

#include "Key.h"
#include "configKeys.h"

class ThirdPerson : public Key {
public:
    ThirdPerson(Player* player);

    virtual ~ThirdPerson() {}
};
