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

		// ── Mise en page / typographie des menus ─────────────────────────
		inline constexpr float ITEM_W = 380.0f;          // zone cliquable d'une option
		inline constexpr float ITEM_H = 56.0f;
		inline constexpr float ITEM_TEXT_SCALE = 0.6f;    // police des options (Amarna)
		inline constexpr float ITEM_HOVER_SCALE = 0.67f;  // agrandissement au survol/focus
		inline constexpr float HELP_TEXT_SCALE = 0.42f;   // texte d'aide (sans action)
		inline constexpr float TITLE_SCALE = 0.5f;        // titre (Gnocchi)
		inline constexpr float HOVER_BAR_H = 4.0f;        // soulignement de l'option survolee
		inline constexpr float SHADOW_OFFSET = 3.0f;      // ombre portee du texte
		// Colonne d'options : pas VERTICAL IDENTIQUE entre chaque option.
		inline constexpr float MAIN_TOP = 660.0f;
		inline constexpr float MAIN_STEP = 84.0f;
		inline constexpr float OPTIONS_TOP = 560.0f;
		inline constexpr float OPTIONS_STEP = 70.0f;
		inline constexpr float AUDIO_TOP = 400.0f;
		inline constexpr float AUDIO_STEP = 64.0f;
		inline constexpr float PAUSE_TOP = 160.0f;
		inline constexpr float PAUSE_STEP = 84.0f;
	}
}