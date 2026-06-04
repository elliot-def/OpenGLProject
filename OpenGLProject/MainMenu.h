#pragma once

#include "Menu.h"
#include "constants.h"
#include "chrono"

class Game;
class TextRenderer;
class SoundManager;
class Sound;
class Renderer;

class MainMenu : public Menu {
public:
	MainMenu(Game* game, SoundManager* soundManager, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textRenderers = nullptr, ShaderManager* shaderManager = nullptr, const std::string& t = Constants::WINDOW_TITLE, bool bg = true);
	~MainMenu();
	
	void update(bool isAFK);
private:
	glm::vec3 m_colorDVDLogo;
	Renderer* m_renderer;

	std::chrono::time_point<std::chrono::system_clock> m_lastWeirdSoundPlayed = std::chrono::system_clock::now();;
	std::vector<Sound*> m_weirdSounds;
	Sound* m_clickSound;
};