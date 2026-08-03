#pragma once

#include "Key.h"
#include "configKeys.h"
#include <functional>

// Classe générique pour les touches joueur (remplace Forward, Backward, Left,
// Right, Crouch, Jump, Sprint, Flashlight, ThirdPerson).
// Les actions sont passées sous forme de lambdas au constructeur.
class PlayerKey : public Key {
public:
    using Action = std::function<void()>;

    PlayerKey(Player* player, const std::string& name, int key,
              Action onPress = nullptr,
              Action onRelease = nullptr,
              Action ifPressed = nullptr);
};
