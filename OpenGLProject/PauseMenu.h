#pragma once
#include "Menu.h"

class Game;
class ShaderManager;
class TextRenderer;

class PauseMenu : public Menu {
public:
    PauseMenu(Game* game, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager);
};