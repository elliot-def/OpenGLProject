#pragma once
#include "Menu.h"

#include <string>
#include <unordered_map>

class Game;
class InputManager;

// Menu de reconfiguration des boutons de manette (sous-menu des Options) :
// une ligne par action avec le bouton courant. Un clic sur une action lance
// la capture du prochain bouton presse (via InputManager::beginControllerCapture),
// puis le nouveau binding est applique et sauvegarde dans res/keys.json.
class ControllerBindingsMenu : public Menu {
public:
    ControllerBindingsMenu(Game* game, SoundManager* soundManager,
                           std::vector<std::unique_ptr<TextRenderer>>* textRenderers,
                           ShaderManager* shaderManager, CursorManager* cursorManager);

    // L'InputManager est cree APRES les menus : injecte ici. Construit les items.
    void setInputManager(InputManager* inputManager);

private:
    InputManager* m_inputManager = nullptr;
    // Action dont on attend le nouveau bouton (vide si aucune capture)
    std::string m_pendingAction;
    // Index de l'item de chaque action (pour afficher l'invite de capture)
    std::unordered_map<std::string, int> m_itemIndexByAction;

    void rebuildItems();
    void startCapture(const std::string& action);
    // Callback InputManager : button = bouton GLFW_GAMEPAD_BUTTON_* capture
    void onControllerButtonCaptured(int button);
};
