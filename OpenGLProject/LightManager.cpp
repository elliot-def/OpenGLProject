#include "LightManager.h"

#include "Shader.h"
#include "LightSource.h"
#include "Spotlight.h"
#include "Player.h"
#include "Renderer.h"

LightManager::LightManager(Renderer* renderer, Player* player) {
    m_flashlight = new Spotlight(renderer);
    m_player = player;

    // DirLight
    m_dirLight.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    m_dirLight.ambient = glm::vec3(0.2f, 0.2f, 0.05f);
    m_dirLight.diffuse = glm::vec3(0.2f, 0.2f, 0.2f);
    m_dirLight.specular = glm::vec3(0.5f, 0.5f, 0.5f);
}

LightManager::~LightManager() {
    for (auto& light : m_lightSources) {
        delete light;
    }

	delete m_flashlight;
}
void LightManager::addPointLight(LightSource* light) {
    if (m_lightSources.size() < MAX_POINT_LIGHTS) {
        m_lightSources.push_back(light);
    }
    else {
        throw std::invalid_argument("Light limit reached, please increase it in LightManager.");
    }
}

// Résout les locations une seule fois par shader (cle = id du programme GL).
// Les noms ne sont construits qu'ici (premier usage) : le hot path
// applyToShader() ne manipule plus que des GLint pré-calculées.
const LightManager::LightLocations& LightManager::ensureLightLocations(Shader* shader) {
    const unsigned int programId = shader->getID();
    auto it = m_lightLocations.find(programId);
    if (it != m_lightLocations.end()) {
        return it->second;
    }

    LightLocations loc;
    loc.numberLightSources = shader->getUniformLocation("numberLightSources");
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        const std::string base = "lightSources[" + std::to_string(i) + "].";
        loc.position[i]  = shader->getUniformLocation(base + "position");
        loc.ambient[i]   = shader->getUniformLocation(base + "ambient");
        loc.diffuse[i]   = shader->getUniformLocation(base + "diffuse");
        loc.specular[i]  = shader->getUniformLocation(base + "specular");
        loc.constant[i]  = shader->getUniformLocation(base + "constant");
        loc.linear[i]    = shader->getUniformLocation(base + "linear");
        loc.quadratic[i] = shader->getUniformLocation(base + "quadratic");
    }
    loc.dirDirection = shader->getUniformLocation("dirLight.direction");
    loc.dirAmbient   = shader->getUniformLocation("dirLight.ambient");
    loc.dirDiffuse   = shader->getUniformLocation("dirLight.diffuse");
    loc.dirSpecular  = shader->getUniformLocation("dirLight.specular");

    return m_lightLocations.emplace(programId, loc).first->second;
}

void LightManager::applyToShader(Shader* shader) {
    // Hot path : uniquement des GLint pré-calculées (aucun hash de string).
    const LightLocations& loc = ensureLightLocations(shader);

    // Lightsources
    shader->setInt(loc.numberLightSources, static_cast<int>(m_lightSources.size()));

    for (size_t i = 0; i < m_lightSources.size(); ++i) {
        shader->setVec3(loc.position[i],  m_lightSources[i]->getPosition());
        shader->setVec3(loc.ambient[i],   m_lightSources[i]->getAmbient());
        shader->setVec3(loc.diffuse[i],   m_lightSources[i]->getDiffuse());
        shader->setVec3(loc.specular[i],  m_lightSources[i]->getSpecular());

        shader->setFloat(loc.constant[i],  m_lightSources[i]->getConstant());
        shader->setFloat(loc.linear[i],    m_lightSources[i]->getLinear());
        shader->setFloat(loc.quadratic[i], m_lightSources[i]->getQuadratic());
    }

    // Flashlight
    m_flashlight->applyToShader(shader, m_player->getFlashlightIsEnabled());

    // DirLight
    shader->setVec3(loc.dirDirection, m_dirLight.direction);
    shader->setVec3(loc.dirAmbient,   m_dirLight.ambient);
    shader->setVec3(loc.dirDiffuse,   m_dirLight.diffuse);
    shader->setVec3(loc.dirSpecular,  m_dirLight.specular);
}

void LightManager::update() {
    for (auto& light : m_lightSources) {
        light->update();
    }
    m_flashlight->update(m_player);
}
void LightManager::draw(){
    for (auto& light : m_lightSources) {
        light->draw();
    }
}