#include "ControllerBindingsMenu.h"
#include "Game.h"
#include "InputManager.h"
#include "MenuManager.h"

#include "constants/window.h"

#include <GLFW/glfw3.h>
#include <string>

// Actions reconfigurables a la manette : nom d'action (InputManager) + libelle
// (le deplacement au stick gauche et le sprint a la gachette RT restent fixes).
static const struct { const char* action; const char* label; } kControllerActions[] = {
    { "Jump",        "Sauter" },
    { "Crouch",      "S'accroupir" },
    { "Push",        "Pousser" },
    { "ThirdPerson", "Vue 1P/3P" },
    { "Flashlight",  "Torche" },
    { "Grab",        "Attraper" },
    { "Noclip",      "Noclip" },
    { "Escape",      "Menu / Reprendre" },
};
static constexpr int kControllerActionCount =
    sizeof(kControllerActions) / sizeof(kControllerActions[0]);

// Nom affichable d'un bouton de manette GLFW_GAMEPAD_BUTTON_*.
static std::string buttonToName(int button) {
    switch (button) {
    case GLFW_GAMEPAD_BUTTON_A: return "A";
    case GLFW_GAMEPAD_BUTTON_B: return "B";
    case GLFW_GAMEPAD_BUTTON_X: return "X";
    case GLFW_GAMEPAD_BUTTON_Y: return "Y";
    case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return "LB";
    case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return "RB";
    case GLFW_GAMEPAD_BUTTON_BACK: return "Back";
    case GLFW_GAMEPAD_BUTTON_START: return "Start";
    case GLFW_GAMEPAD_BUTTON_GUIDE: return "Guide";
    case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return "Stick Gauche";
    case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return "Stick Droit";
    case GLFW_GAMEPAD_BUTTON_DPAD_UP: return "Croix Haut";
    case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return "Croix Droite";
    case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return "Croix Bas";
    case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return "Croix Gauche";
    default: return "Bouton " + std::to_string(button);
    }
}

ControllerBindingsMenu::ControllerBindingsMenu(Game* game, SoundManager* soundManager,
                                               std::vector<std::unique_ptr<TextRenderer>>* textRenderers,
                                               ShaderManager* shaderManager, CursorManager* cursorManager)
    : Menu(game, soundManager, textRenderers, shaderManager, cursorManager, "Manette", false) {
    // Les items sont construits dans rebuildItems(), une fois l'InputManager injecte
}

void ControllerBindingsMenu::setInputManager(InputManager* inputManager) {
    m_inputManager = inputManager;
    if (m_inputManager) {
        m_inputManager->setOnControllerButtonCaptured([this](int button) { onControllerButtonCaptured(button); });
    }
    rebuildItems();
}

void ControllerBindingsMenu::rebuildItems() {
    clear();
    m_itemIndexByAction.clear();

    addItem("Cliquez sur une action puis appuyez sur le bouton de la manette",
            Constants::Window::WINDOW_WIDTH / 2, 355, 900, 40, nullptr);

    const float startY = 415.0f;
    const float step = 46.0f;
    for (int i = 0; i < kControllerActionCount; ++i) {
        const std::string action = kControllerActions[i].action;
        const std::string label = kControllerActions[i].label;

        int button = m_inputManager ? m_inputManager->getControllerBinding(action) : -1;
        const std::string text = label + " : " + buttonToName(button);

        addItem(text, Constants::Window::WINDOW_WIDTH / 2, startY + i * step, 560, 40,
                [this, action]() { startCapture(action); });

        m_itemIndexByAction[action] = static_cast<int>(m_items.size()) - 1;
    }

    // Sensibilite du stick droit (camera) de la manette
    float lookSens = m_inputManager ? m_inputManager->getControllerLookSensitivity()
                                    : ConfigKeys::CONTROLLER_LOOK_SENSITIVITY;
    addRange("Sensibilite du joystick", Constants::Window::WINDOW_WIDTH / 2, 800, 300, 25,
        0.0f, 3.0f, lookSens, [this](float value) {
            if (m_inputManager) m_inputManager->setControllerLookSensitivity(value);
        });

    addItem("Retour", Constants::Window::WINDOW_WIDTH / 2, 860, 200, 50, [this]() {
        if (m_inputManager) m_inputManager->cancelControllerCapture();
        m_pendingAction.clear();
        m_game->getMenuManager()->showOptions();
    });
}

void ControllerBindingsMenu::startCapture(const std::string& action) {
    if (!m_inputManager) return;
    m_pendingAction = action;
    m_inputManager->beginControllerCapture();

    auto it = m_itemIndexByAction.find(action);
    if (it != m_itemIndexByAction.end()) {
        setItemText(static_cast<size_t>(it->second), "Appuyez sur un bouton...");
    }
}

void ControllerBindingsMenu::onControllerButtonCaptured(int button) {
    if (m_pendingAction.empty()) return;
    const std::string action = m_pendingAction;
    m_pendingAction.clear();

    m_inputManager->setControllerBinding(action, button);
    rebuildItems();
}
