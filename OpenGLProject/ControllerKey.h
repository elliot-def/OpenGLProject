#pragma once

#include "Key.h"
#include <functional>

class Controller;

// Classe ControllerKey : touche de jeu pilotee par un bouton de manette
// (equivalent manette de PlayerKey pour le clavier). Le bouton (index
// GLFW_GAMEPAD_BUTTON_*) est stocke dans m_key (herite de Key), et l'etat
// est sonde via la classe Controller (edge detection incluse).
class ControllerKey : public Key {
public:
    using Action = std::function<void()>;

    // controller : manette source de l'etat (pointeur non possede)
    // button     : bouton GLFW_GAMEPAD_BUTTON_* a ecouter
    ControllerKey(Player* player, const std::string& name, int button,
                  Controller* controller,
                  Action onPress = nullptr,
                  Action onRelease = nullptr,
                  Action ifPressed = nullptr);

    // Met a jour l'etat de la touche a partir de la manette :
    // a appeler chaque frame dans le contexte actif.
    void update(InputContext context);

    // Change la manette source (utilise par InputManager quand il bascule
    // dynamiquement entre Steam Input et XInput).
    void setController(Controller* controller) { m_controller = controller; }

private:
    Controller* m_controller;
};
