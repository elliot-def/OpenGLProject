#include "KeyBindingsMenu.h"
#include "Game.h"
#include "InputManager.h"
#include "MenuManager.h"

#include "constants/window.h"

#include <GLFW/glfw3.h>
#include <string>

// Actions reconfigurables : nom d'action (InputManager) + libelle francais
static const struct { const char* action; const char* label; } kBindableActions[] = {
    { "Forward",     "Avancer" },
    { "Backward",    "Reculer" },
    { "Left",        "Gauche" },
    { "Right",       "Droite" },
    { "Jump",        "Sauter" },
    { "Crouch",      "S'accroupir" },
    { "Sprint",      "Courir" },
    { "Flashlight",  "Torche" },
    { "ThirdPerson", "Vue 1P/3P" },
    { "Push",        "Pousser" },
    { "Grab",        "Attraper" },
    { "Noclip",      "Noclip" },
};
static constexpr int kActionCount = sizeof(kBindableActions) / sizeof(kBindableActions[0]);

// Nom affichable d'un code de touche GLFW : nom natif pour les touches
// imprimables, nom francise pour les touches speciales.
static std::string keyToName(int key) {
    const char* name = glfwGetKeyName(key, 0);
    if (name && name[0] != '\0') {
        std::string s(name);
        if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') {
            s[0] = static_cast<char>(s[0] - 'a' + 'A');
        }
        return s;
    }
    switch (key) {
    case GLFW_KEY_SPACE: return "Espace";
    case GLFW_KEY_LEFT_SHIFT: return "Maj Gauche";
    case GLFW_KEY_RIGHT_SHIFT: return "Maj Droite";
    case GLFW_KEY_LEFT_CONTROL: return "Ctrl Gauche";
    case GLFW_KEY_RIGHT_CONTROL: return "Ctrl Droite";
    case GLFW_KEY_LEFT_ALT: return "Alt Gauche";
    case GLFW_KEY_RIGHT_ALT: return "Alt Droite";
    case GLFW_KEY_TAB: return "Tab";
    case GLFW_KEY_ENTER: return "Entree";
    case GLFW_KEY_BACKSPACE: return "Retour Arriere";
    case GLFW_KEY_ESCAPE: return "Echap";
    case GLFW_KEY_LEFT: return "Fleche Gauche";
    case GLFW_KEY_RIGHT: return "Fleche Droite";
    case GLFW_KEY_UP: return "Fleche Haut";
    case GLFW_KEY_DOWN: return "Fleche Bas";
    case GLFW_KEY_INSERT: return "Inser";
    case GLFW_KEY_DELETE: return "Suppr";
    case GLFW_KEY_HOME: return "Origine";
    case GLFW_KEY_END: return "Fin";
    case GLFW_KEY_PAGE_UP: return "Page Haut";
    case GLFW_KEY_PAGE_DOWN: return "Page Bas";
    default: break;
    }
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
        return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
    }
    return "Touche " + std::to_string(key);
}

KeyBindingsMenu::KeyBindingsMenu(Game* game, SoundManager* soundManager,
                                 std::vector<std::unique_ptr<TextRenderer>>* textRenderers,
                                 ShaderManager* shaderManager, CursorManager* cursorManager)
    : Menu(game, soundManager, textRenderers, shaderManager, cursorManager, "Clavier & Souris", false) {
    // Les items sont construits dans rebuildItems(), une fois l'InputManager injecte
}

void KeyBindingsMenu::setInputManager(InputManager* inputManager) {
    m_inputManager = inputManager;
    if (m_inputManager) {
        m_inputManager->setOnKeyCaptured([this](int keyCode) { onKeyCaptured(keyCode); });
    }
    rebuildItems();
}

void KeyBindingsMenu::rebuildItems() {
    clear();
    m_itemIndexByAction.clear();

    // Rappel d'utilisation
    addItem("Cliquez sur une action puis appuyez sur la touche",
            Constants::Window::WINDOW_WIDTH / 2, 340, 900, 40, nullptr);

    // Une ligne par action : libelle + touche courante
    const float startY = 400.0f;
    const float step = 44.0f;
    for (int i = 0; i < kActionCount; ++i) {
        const std::string action = kBindableActions[i].action;
        const std::string label = kBindableActions[i].label;

        int code = m_inputManager ? m_inputManager->getKeyBinding(action) : GLFW_KEY_UNKNOWN;
        const std::string text = label + " : " + keyToName(code);

        addItem(text, Constants::Window::WINDOW_WIDTH / 2, startY + i * step, 560, 40,
                [this, action]() { startCapture(action); });

        // L'item vient d'etre ajoute : c'est le dernier de la liste
        m_itemIndexByAction[action] = static_cast<int>(m_items.size()) - 1;
    }

    // Sensibilite de la souris (rotation de la camera 1P)
    float mouseSens = m_inputManager ? m_inputManager->getMouseSensitivity()
                                     : ConfigKeys::DEFAULT_MOUSE_SENSITIVITY;
    addRange("Sensibilite de la souris", Constants::Window::WINDOW_WIDTH / 2, 940, 300, 25,
        0.01f, 0.20f, mouseSens, [this](float value) {
            if (m_inputManager) m_inputManager->setMouseSensitivity(value);
        });

    addItem("Retour", Constants::Window::WINDOW_WIDTH / 2, 1000, 200, 50, [this]() {
        // Une capture en cours est abandonnee en quittant le menu
        if (m_inputManager) m_inputManager->cancelKeyCapture();
        m_pendingAction.clear();
        m_game->getMenuManager()->showOptions();
    });
}

void KeyBindingsMenu::startCapture(const std::string& action) {
    if (!m_inputManager) return;
    m_pendingAction = action;
    m_inputManager->beginKeyCapture();

    // Invite visuel sur l'item concerne
    auto it = m_itemIndexByAction.find(action);
    if (it != m_itemIndexByAction.end()) {
        setItemText(static_cast<size_t>(it->second), "Appuyez sur une touche...");
    }
}

void KeyBindingsMenu::onKeyCaptured(int keyCode) {
    if (m_pendingAction.empty()) return;
    const std::string action = m_pendingAction;
    m_pendingAction.clear();

    if (keyCode == GLFW_KEY_ESCAPE) {
        // Echap annule la capture : aucune touche n'est modifiee
        rebuildItems();
        return;
    }

    // Applique le nouveau binding (met a jour la Key + sauvegarde keys.json)
    m_inputManager->setKeyBinding(action, keyCode);
    rebuildItems();
}
