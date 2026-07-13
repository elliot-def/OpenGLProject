#include "OptionsMenu.h"
#include "Game.h"
#include "constants.h"
#include <iostream>

OptionsMenu::OptionsMenu(Game* game, SoundManager* soundManager, GameState& previousState, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager)
    : Menu(game, soundManager, textRenderers, shaderManager, "Options", false)
{
    addItem("Son: ON", Constants::WINDOW_WIDTH / 2, 700, 200, 50, [this, soundManager]() {
		soundManager->toggleMute();
		m_items[0].text = soundManager->isMuted() ? "Son: OFF" : "Son: ON";
        });
    addItem("Retour", Constants::WINDOW_WIDTH / 2, 800, 200, 50, [game, &previousState]() {
        game->changeState(previousState == STATE_PLAYING ? STATE_PAUSED : STATE_MENU);
        });
}