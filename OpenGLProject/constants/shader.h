#pragma once

namespace Constants {
	namespace Shader {
        inline constexpr const char* SHADERS_FOLDER_PATH = "./res/shaders/";
		inline constexpr const int MAX_LIGHTS_SOURCES = 10;
		// Pièces intérieures bornées (filtre lumière) : boîtes max déclarées
		// dans les shaders (uRoomMin/uRoomMax[MAX_ROOMS]) + LightManager.
		inline constexpr const int MAX_ROOMS = 4;

	}
}