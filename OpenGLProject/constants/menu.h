#pragma once

#include "window.h"

namespace Constants {
	namespace Menu {
		inline constexpr float MENU_TITLE_X = Constants::Window::WINDOW_WIDTH / 2.0f;
		inline constexpr float MENU_TITLE_Y = 300.0f;
		inline constexpr float MENU_TITLE_W = 600.0f;
		inline constexpr float MENU_TITLE_H = 150.0f;
		inline constexpr float MAINMENU_AFK_THRESHOLD = 6.0f; // Time in seconds before considering the player as AFK in the main menu
		inline constexpr float WEIRD_SOUND_INTERVAL = 12.00f;
	}
}