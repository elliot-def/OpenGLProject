#pragma once

#include "Menu.h"
#include "constants.h"

class Game;
class TextRenderer;
class Renderer;

class MainMenu : public Menu {
public:
	MainMenu(Game* game, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textRenderers = nullptr, ShaderManager* shaderManager = nullptr, const std::string& t = Constants::WINDOW_TITLE, bool bg = true);
	
	void update(bool isAFK);
private:
	glm::vec3 m_colorDVDLogo;
	Renderer* m_renderer;
};