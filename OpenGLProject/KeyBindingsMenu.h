#pragma once
#include "Menu.h"

#include <string>
#include <unordered_map>

class Game;
class InputManager;

// Menu de reconfiguration des touches (sous-menu des Options) : une ligne par
// action avec la touche courante. Un clic sur une action lance la capture de
// la prochaine touche pressee (via InputManager::beginKeyCapture), puis le
// nouveau binding est applique et sauvegarde dans res/keys.json.
class KeyBindingsMenu : public Menu {
public:
    KeyBindingsMenu(Game* game, SoundManager* soundManager,
                    std::vector<std::unique_ptr<TextRenderer>>* textRenderers,
                    ShaderManager* shaderManager, CursorManager* cursorManager);

    // L'InputManager est cree APRES les menus (Game::loadResources) : il est
    // injecte ici une fois pret. Construit egalement les items du menu.
    void setInputManager(InputManager* inputManager);

private:
    InputManager* m_inputManager = nullptr;
    // Action dont on attend la nouvelle touche (vide si aucune capture)
    std::string m_pendingAction;
    // Index de l'item de chaque action (pour afficher l'invite de capture)
    std::unordered_map<std::string, int> m_itemIndexByAction;

    // Reconstruit les items avec les touches courantes (apres un rebinding)
    void rebuildItems();
    // Lance la capture de la touche pour l'action donnee
    void startCapture(const std::string& action);
    // Callback InputManager : keyCode = touche capturee (-1 = annulation)
    void onKeyCaptured(int keyCode);
};
