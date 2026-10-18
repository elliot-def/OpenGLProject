#include "LightManager.h"

#include <stdexcept>
#include "Shader.h"
#include "LightSource.h"
#include "Spotlight.h"
#include "Player.h"
#include "Renderer.h"

LightManager::LightManager(Renderer* renderer, Player* player) {
    m_flashlight = new Spotlight(renderer);
    m_player = player;

    // DirLight (soleil) : défauts LUMINEUX (cf. demande « luminosité
    // directionnelle du soleil augmentée »). Surchargés par la carte via
    // setDirLight (clé "sun" du JSON, ambiance par map).
    m_dirLight.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    m_dirLight.ambient = glm::vec3(0.55f, 0.55f, 0.48f);
    m_dirLight.diffuse = glm::vec3(1.5f, 1.5f, 1.4f);
    m_dirLight.specular = glm::vec3(0.6f, 0.6f, 0.6f);
}

LightManager::~LightManager() {
    for (auto& light : m_lightSources) {
        delete light;
    }

	delete m_flashlight;
}
size_t LightManager::addPointLight(LightSource* light) {
    if (m_lightSources.size() < MAX_POINT_LIGHTS) {
        m_lightSources.push_back(light);
        m_lightRoom.push_back(-1);   // no affectée : lumière libre
        return m_lightSources.size() - 1;
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
    loc.dirDiffuse   = shader->getUniformLocation("dirLight.diffuse");    loc.dirSpecular = shader->getUniformLocation("dirLight.specular");
    loc.roomCount = shader->getUniformLocation("uRoomCount");
    for (int i = 0; i < Constants::Shader::MAX_ROOMS; ++i) {
        loc.roomMin[i] = shader->getUniformLocation("uRoomMin[" + std::to_string(i) + "]");
        loc.roomMax[i] = shader->getUniformLocation("uRoomMax[" + std::to_string(i) + "]");
    }
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        loc.lightRoom[i] = shader->getUniformLocation("uLightRoom[" + std::to_string(i) + "]");
    }

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

    // Pièces intérieures : uRoomCount = nb de pièces valides ; chaque lumière
    // assignée (uLightRoom[i] >= 0) n'éclaire que les fragments de SA pièce
    // (la lumière ne traverse pas les murs) ; -1 = lumière libre (extérieure).
    shader->setInt(loc.roomCount, static_cast<int>(m_roomMin.size()));
    for (int i = 0; i < Constants::Shader::MAX_ROOMS; ++i) {
        shader->setVec3(loc.roomMin[i],
                        i < static_cast<int>(m_roomMin.size()) ? m_roomMin[i] : glm::vec3(0.0f));
        shader->setVec3(loc.roomMax[i],
                        i < static_cast<int>(m_roomMax.size()) ? m_roomMax[i] : glm::vec3(0.0f));
    }
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        shader->setInt(loc.lightRoom[i],
                       i < static_cast<int>(m_lightRoom.size()) ? m_lightRoom[i] : -1);
    }
}

void LightManager::update() {
    for (auto& light : m_lightSources) {
        light->update();
    }
    m_flashlight->update(m_player);
}

void LightManager::setFlashlightPosition(const glm::vec3& position) {
    // Position seule : la direction reste celle empruntee au joueur
    // (setBorrowedDirection dans Spotlight::update(Player*)).
    if (m_flashlight) {
        m_flashlight->setPosition(position);
    }
}

int LightManager::addRoom(const glm::vec3& roomMin, const glm::vec3& roomMax) {
    if (m_roomMin.size() >= Constants::Shader::MAX_ROOMS) {
        throw std::invalid_argument("Room limit reached, please increase Constants::Shader::MAX_ROOMS.");
    }
    m_roomMin.push_back(roomMin);
    m_roomMax.push_back(roomMax);
    return static_cast<int>(m_roomMin.size()) - 1;
}

void LightManager::setLightRoom(size_t lightIndex, int roomIndex) {
    if (lightIndex < m_lightRoom.size()) {
        m_lightRoom[lightIndex] = roomIndex;
    }
}
