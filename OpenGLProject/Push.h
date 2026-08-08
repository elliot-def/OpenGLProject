#pragma once

#include "Key.h"
#include "configKeys.h"

// Touche Push (R) : l'animation de coup de poing du personnage 3P/1P est
// declenchee par CharacterAnimationController via getKey("Push") ; rien a
// faire ici cote viewmodel (supprime).
class Push : public Key {
public:
    Push();
};
