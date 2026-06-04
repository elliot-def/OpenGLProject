#include "PauseMenu.h"
#include "Game.h"
#include "constants.h"

PauseMenu::PauseMenu(Game* game, SoundManager* soundManager, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager)
    : Menu(game, soundManager, textRenderers, shaderManager, "Pause", false)
{
    addItem("Reprendre", Constants::WINDOW_WIDTH / 2, 160, 200, 50, [game]() {
        game->changeState(STATE_PLAYING);
        });
    addItem("Menu Principal", Constants::WINDOW_WIDTH / 2, 230, 200, 50, [game]() {
        game->changeState(STATE_MENU);
        });
    addItem("Quitter", Constants::WINDOW_WIDTH / 2, 300, 200, 50, [game]() {
        game->stop();
        });
}

