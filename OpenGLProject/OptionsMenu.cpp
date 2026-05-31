#include "OptionsMenu.h"
#include "Game.h"
#include "constants.h"
#include <iostream>

OptionsMenu::OptionsMenu(Game* game, GameState& previousState, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager)
    : Menu(game, textRenderers, shaderManager, "Options", false)
{
    addItem("Son: ON", Constants::WINDOW_WIDTH / 2, 160, 200, 50, []() {
        std::cout << "Toggle son" << std::endl;
        });
    addItem("Retour", Constants::WINDOW_WIDTH / 2, 230, 200, 50, [game, &previousState]() {
        game->changeState(previousState == STATE_PLAYING ? STATE_PAUSED : STATE_MENU);
        });
}