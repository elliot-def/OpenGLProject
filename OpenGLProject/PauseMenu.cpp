#include "PauseMenu.h"
#include "Game.h"
#include "constants/window.h"

PauseMenu::PauseMenu(Game* game, SoundManager* soundManager, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager, CursorManager* cursorManager)
    : Menu(game, soundManager, textRenderers, shaderManager, cursorManager, "Pause", false)
{
    addItem("Reprendre", Constants::Window::WINDOW_WIDTH / 2, Constants::Menu::PAUSE_TOP + 0 * Constants::Menu::PAUSE_STEP, Constants::Menu::ITEM_W, Constants::Menu::ITEM_H, [game]() {
        game->changeState(STATE_PLAYING);
        });
    addItem("Menu Principal", Constants::Window::WINDOW_WIDTH / 2, Constants::Menu::PAUSE_TOP + 1 * Constants::Menu::PAUSE_STEP, Constants::Menu::ITEM_W, Constants::Menu::ITEM_H, [game]() {
        // Retour vers le menu principal : on restaure sa selection manette.
        game->changeState(STATE_MENU, true);
        });
    addItem("Quitter", Constants::Window::WINDOW_WIDTH / 2, Constants::Menu::PAUSE_TOP + 2 * Constants::Menu::PAUSE_STEP, Constants::Menu::ITEM_W, Constants::Menu::ITEM_H, [game]() {
        game->stop();
        });
}

