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

    // --- PRÉCALCUL DES NOMS D'UNIFORMS ---
    // On réserve la mémoire pour éviter les réallocations du vecteur
    m_lightUniformNames.reserve(MAX_POINT_LIGHTS);
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        std::string base = "lightSources[" + std::to_string(i) + "].";
        LightUniformStrings names;
        names.position = base + "position";
        names.ambient = base + "ambient";
        names.diffuse = base + "diffuse";
        names.specular = base + "specular";
        names.constant = base + "constant";
        names.linear = base + "linear";
        names.quadratic = base + "quadratic";
        m_lightUniformNames.push_back(names);
    }
}

LightManager::~LightManager() {
    for (auto& light : m_lightSources) {
        delete light;
    }

	delete m_flashlight;
}
void LightManager::addPointLight(LightSource* light) {
    if (m_lightSources.size() < MAX_POINT_LIGHTS) {
        // 1. On récupère l'index de la nouvelle lumière
        size_t newIndex = m_lightSources.size();

        // 2. On génère ses chaînes d'uniforms associées
        std::string base = "lightSources[" + std::to_string(newIndex) + "].";
        LightUniformStrings names;
        names.position = base + "position";
        names.ambient = base + "ambient";
        names.diffuse = base + "diffuse";
        names.specular = base + "specular";
        names.constant = base + "constant";
        names.linear = base + "linear";
        names.quadratic = base + "quadratic";

        // 3. On l'ajoute au cache de noms et on stocke la lumière
        m_lightUniformNames.push_back(names);
        m_lightSources.push_back(light);
    }
    else {
        throw std::invalid_argument("Light limit reached, please increase it in LightManager.");
    }
}

void LightManager::applyToShader(Shader* shader) {
    // Lightsources
    shader->setInt("numberLightSources", static_cast<int>(m_lightSources.size()));

    for (size_t i = 0; i < m_lightSources.size(); ++i) {
        // Utilisation directe des chaînes précalculées (zéro allocation de mémoire ici !)
        const auto& names = m_lightUniformNames[i];

        shader->setVec3(names.position, m_lightSources[i]->getPosition());
        shader->setVec3(names.ambient, m_lightSources[i]->getAmbient());
        shader->setVec3(names.diffuse, m_lightSources[i]->getDiffuse());
        shader->setVec3(names.specular, m_lightSources[i]->getSpecular());

        shader->setFloat(names.constant, m_lightSources[i]->getConstant());
        shader->setFloat(names.linear, m_lightSources[i]->getLinear());
        shader->setFloat(names.quadratic, m_lightSources[i]->getQuadratic());
    }

    // Flashlight
    m_flashlight->applyToShader(shader, m_player->getFlashlightIsEnabled());

    // DirLight
    shader->setVec3("dirLight.direction", m_dirLight.direction);
    shader->setVec3("dirLight.ambient", m_dirLight.ambient);
    shader->setVec3("dirLight.diffuse", m_dirLight.diffuse);
    shader->setVec3("dirLight.specular", m_dirLight.specular);
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