#pragma once
#include <cstdint>
#include <glm/vec3.hpp>

namespace Constants {
    // Window
    inline constexpr int WINDOW_WIDTH = 1920;
    inline constexpr int WINDOW_HEIGHT = 1080;
    inline constexpr const char* WINDOW_TITLE = "OpenGLProject";
    inline constexpr bool IS_WINDOW_FULLSCREEN = false;

    // Renderer
    inline constexpr int DEFAULT_FPS_CAPPING = 240;
    inline constexpr bool DEFAULT_IS_FPS_CAPPING = false;

    // Player

	inline constexpr float PLAYER_WALKING_SPEED = 2.5f; // unit : pixels per second
	inline constexpr float PLAYER_SPRINTING_SPEED = 4.0f; // unit : pixels per second

    // Texture

	inline constexpr const char* TEXTURES_FOLDER_PATHS[] = { "./res/textures/", "./res/models/" };
	inline constexpr const unsigned int FIRST_TEXTURE_ID = 10;     // Starting ID for user textures
	inline constexpr const unsigned int BLACK_TEXTURE_ID = 0;      // ID for default black texture

    // Shader

    inline constexpr const char* SHADERS_FOLDER_PATH = "./res/shaders/";
    
    // Files
    
    inline constexpr const char PREFERED_SEPARATOR_PATH = '/';

    // Lights Shadering

    inline constexpr const int MAX_LIGHTS_SOURCES = 10;

    // Networking

    inline constexpr const int MAX_PACKET_SIZE = 1024;
    inline constexpr const char* SERVER_IP = "127.0.0.1";
    inline constexpr const int SERVER_PORT = 3333;

	inline constexpr uint16_t PACKET_MAGIC = 0xABCD; // Magic number for packet validation

    // Menu

    inline constexpr float MENU_TITLE_X = WINDOW_WIDTH/2.0f;
    inline constexpr float MENU_TITLE_Y = 300.0f;
    inline constexpr float MENU_TITLE_W = 600.0f;
	inline constexpr float MENU_TITLE_H = 150.0f;
	inline constexpr float MAINMENU_AFK_THRESHOLD = 60.0f; // Time in seconds before considering the player as AFK in the main menu
}

namespace Materials {
    // Shininess values for different materials in OpenGL
    // Range: 0.0 (matte) to 128.0 (very shiny)

    // Matte materials
    inline constexpr const float RUBBER = 10.0f;
    inline constexpr const float CLAY = 8.0f;
    inline constexpr const float CONCRETE = 5.0f;

    // Semi-matte materials
    inline constexpr const float WOOD = 15.0f;
    inline constexpr const float PLASTIC_MATTE = 20.0f;
    inline constexpr const float STONE = 12.0f;

    // Semi-glossy materials
    inline constexpr const float PLASTIC_GLOSSY = 32.0f;
    inline constexpr const float CERAMIC = 40.0f;
    inline constexpr const float MARBLE = 45.0f;

    // Glossy materials
    inline constexpr const float GLASS = 64.0f;
    inline constexpr const float POLISHED_WOOD = 50.0f;
    inline constexpr const float PAINTED_METAL = 55.0f;

    // Very shiny materials
    inline constexpr const float BRONZE = 76.8f;
    inline constexpr const float COPPER = 76.8f;
    inline constexpr const float BRASS = 83.2f;
    inline constexpr const float SILVER = 89.6f;
    inline constexpr const float GOLD = 83.2f;
    inline constexpr const float CHROME = 128.0f;
    inline constexpr const float POLISHED_METAL = 128.0f;
    inline constexpr const float MIRROR = 128.0f;

    // Special materials
    inline constexpr const float PEARL = 11.264f;
    inline constexpr const float JADE = 12.8f;
    inline constexpr const float OBSIDIAN = 38.4f;
    inline constexpr const float EMERALD = 76.8f;
    inline constexpr const float RUBY = 76.8f;
}

namespace Colors {
    // Shininess values for different materials in OpenGL
    // Range: 0.0 (matte) to 128.0 (very shiny)

    // Matte materials
    inline constexpr const glm::vec3 SHADOW_GREY = glm::vec3(35/255.0f, 31/255.0f, 32/255.0f);       // #231F20
	inline constexpr const glm::vec3 TOMATO_JAM = glm::vec3(187/255.0f, 68/255.0f, 48/255.0f);       // #BB4430
	inline constexpr const glm::vec3 TROPICAL_TEAL = glm::vec3(126/255.0f, 189/255.0f, 194/255.0f);  // #7EBDC2
	inline constexpr const glm::vec3 VANILLA_CUSTARD = glm::vec3(243/255.0f, 223/255.0f, 162/255.0f);// #F3DFA2
	inline constexpr const glm::vec3 LINEN = glm::vec3(239/255.0f, 230/255.0f, 221/255.0f);          // #EFE6DD

}