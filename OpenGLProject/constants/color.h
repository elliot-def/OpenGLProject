#pragma once

#include <glm/vec3.hpp>

namespace Constants {
	namespace Color {
		// Shininess values for different materials in OpenGL
		// Range: 0.0 (matte) to 128.0 (very shiny)

		// Matte materials
		inline constexpr const glm::vec3 SHADOW_GREY = glm::vec3(35 / 255.0f, 31 / 255.0f, 32 / 255.0f);       // #231F20
		inline constexpr const glm::vec3 TOMATO_JAM = glm::vec3(187 / 255.0f, 68 / 255.0f, 48 / 255.0f);       // #BB4430
		inline constexpr const glm::vec3 TROPICAL_TEAL = glm::vec3(126 / 255.0f, 189 / 255.0f, 194 / 255.0f);  // #7EBDC2
		inline constexpr const glm::vec3 VANILLA_CUSTARD = glm::vec3(243 / 255.0f, 223 / 255.0f, 162 / 255.0f);// #F3DFA2
		inline constexpr const glm::vec3 LINEN = glm::vec3(239 / 255.0f, 230 / 255.0f, 221 / 255.0f);          // #EFE6DD
	}
}