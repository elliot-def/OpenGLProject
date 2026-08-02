#pragma once

namespace Constants {
    namespace Material {
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
}