#pragma once
#include "Menu.h"
#include "gamestate.h"

class Game;
class ShaderManager;
class TextRenderer;

class OptionsMenu : public Menu {
public:
    OptionsMenu(Game* game, SoundManager* soundManager, GameState& previousState, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager);
};
