#include "ControllerKey.h"
#include "Controller.h"

ControllerKey::ControllerKey(Player* player, const std::string& name, int button,
                             Controller* controller,
                             Action onPress, Action onRelease, Action ifPressed)
    : Key(player, name, button), m_controller(controller) {
    if (onPress)
        setOnPressAction(InputContext::GAME, std::move(onPress));
    if (onRelease)
        setOnReleaseAction(InputContext::GAME, std::move(onRelease));
    if (ifPressed)
        setIfPressedAction(InputContext::GAME, std::move(ifPressed));
}

void ControllerKey::update(InputContext context) {
    if (!m_controller || !m_controller->isConnected()) return;

    // Meme chronologie que le clavier (InputManager) : sur le frame de
    // pression on appelle ifPressed puis onPress ; en maintien ifPressed ;
    // au relache onRelease. Les frontieres viennent de l'edge detection de
    // la manette (etat precedent/courant), pas de m_isPressed : un bouton
    // reste tenu a travers un changement de mode ne rejoue pas l'action.
    if (m_controller->isButtonPressed(m_key)) {
        ifPressed(context);
        if (m_controller->isButtonJustPressed(m_key)) {
            onPress(context);
        }
    }
    else if (m_controller->isButtonJustReleased(m_key)) {
        onRelease(context);
    }
}
