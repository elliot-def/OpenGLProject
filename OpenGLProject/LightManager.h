#pragma once

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "constants/shader.h"

class Shader;
class LightSource;
class Spotlight;
class Renderer;
class Player;

struct DirLight {
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

class LightManager {
public:
    LightManager(Renderer* renderer, Player* player);
    ~LightManager();

    void addPointLight(LightSource* light);
    void applyToShader(Shader* shader);
    void update();
    void draw();

private:

    // Locations GL des uniforms lumiere, resolues UNE SEULE FOIS par shader
    // (cle = id du programme GL) au premier applyToShader(). Le hot path
    // n'utilise alors que des GLint : plus aucun hash/compare de string.
    // Valide car les shaders (ShaderManager) sont crees une fois au demarrage
    // et ne sont jamais detruits/recrees en cours de jeu.
    struct LightLocations {
        int numberLightSources = -1;
        int position[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int ambient[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int diffuse[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int specular[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int constant[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int linear[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int quadratic[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int dirDirection = -1;
        int dirAmbient = -1;
        int dirDiffuse = -1;
        int dirSpecular = -1;
    };
    const LightLocations& ensureLightLocations(Shader* shader);

    static constexpr int MAX_POINT_LIGHTS = Constants::Shader::MAX_LIGHTS_SOURCES;
    std::vector<LightSource*> m_lightSources;
    std::unordered_map<unsigned int, LightLocations> m_lightLocations; // par id de programme GL
    Spotlight* m_flashlight;
    Player* m_player;
    struct DirLight m_dirLight;
};