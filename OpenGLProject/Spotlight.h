#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include "Entity.h"
class Player;
class Shader;
class Renderer;

class Spotlight : public Entity {
public:
	Spotlight(Renderer* renderer,
		glm::vec3 ambient = glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3 diffuse = glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f),
		float constant = 1.0f, float linear = 0.09f, float quadratic = 0.032f,
		float cutOff = glm::cos(glm::radians(12.5f)), float outerCutOff = glm::cos(glm::radians(17.5f)));
	void update(glm::vec3 position, Direction* direction);
	void update(Player* player);
	void applyToShader(Shader* shader, bool isEnabled);
private:

	// Locations GL des uniforms spotLight.*, resolues une seule fois par
	// shader (cle = id du programme GL) au premier applyToShader().
	// Valide car les shaders (ShaderManager) sont crees une fois au demarrage
	// et ne sont jamais detruits/recrees en cours de jeu.
	struct SpotLocations {
		int position    = -1;
		int direction   = -1;
		int ambient     = -1;
		int diffuse     = -1;
		int specular    = -1;
		int constant    = -1;
		int linear      = -1;
		int quadratic   = -1;
		int cutOff      = -1;
		int outerCutOff = -1;
	};
	const SpotLocations& ensureLocations(Shader* shader);
	std::unordered_map<unsigned int, SpotLocations> m_lightLocations;
	/* Dans Entity :
	glm::vec3 m_position;
	Direction m_direction;
	Renderer* renderer;
	*/
	
	glm::vec3 m_ambient;
	glm::vec3 m_diffuse;
	glm::vec3 m_specular;

	float m_constant;
	float m_linear;
	float m_quadratic;

	// Constantes

	float m_cutOff;
	float m_outerCutOff;

	// Variable statique pour le lissage temporel du scintillement de la flashlight
	float m_targetFlicker = 1.0f;
	float m_currentFlicker = 1.0f;
	float m_flickerTimer = 0.0f;
	float m_flickerChangeInterval = 0.2f; // Changement toutes les X secondes
	float m_smoothingSpeed = 5.0f; // Vitesse d'interpolation
};