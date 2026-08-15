#include "PauseMenu.h"
#include "Game.h"
#include "constants/window.h"

PauseMenu::PauseMenu(Game* game, SoundManager* soundManager, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager, CursorManager* cursorManager)
    : Menu(game, soundManager, textRenderers, shaderManager, cursorManager, "Pause", false)
{
    addItem("Reprendre", Constants::Window::WINDOW_WIDTH / 2, 160, 200, 50, [game]() {
        game->changeState(STATE_PLAYING);
        });
    addItem("Menu Principal", Constants::Window::WINDOW_WIDTH / 2, 230, 200, 50, [game]() {
        // Retour vers le menu principal : on restaure sa selection manette.
        game->changeState(STATE_MENU, true);
        });
    addItem("Quitter", Constants::Window::WINDOW_WIDTH / 2, 300, 200, 50, [game]() {
        game->stop();
        });
}

