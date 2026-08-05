#include "Spotlight.h"
#include "Player.h"
#include "Shader.h"
#include "Renderer.h"
#include "Entity.h"

Spotlight::Spotlight(Renderer* renderer,
	glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular,
	float constant, float linear, float quadratic,
	float cutOff, float outerCutOff) : Entity(renderer, nullptr) {

    m_ambient = ambient;
    m_diffuse = diffuse;
    m_specular = specular;
    m_constant = constant;
    m_linear = linear;
    m_quadratic = quadratic;
    m_cutOff = cutOff;
    m_outerCutOff = outerCutOff;
}

void Spotlight::update(glm::vec3 position, Direction* direction) {
	m_position = position;
	if (m_ownsDirection) { delete m_direction; m_ownsDirection = false; }
	m_direction = direction;
}

void Spotlight::update(Player* player) {
	m_position = player->getPosition() + Constants::Player::PLAYER_EYE_HEIGHT;
	if (m_ownsDirection) { delete m_direction; m_ownsDirection = false; }
	m_direction = player->getDirection();
}

const Spotlight::SpotLocations& Spotlight::ensureLocations(Shader* shader) {
    const unsigned int programId = shader->getID();
    auto it = m_lightLocations.find(programId);
    if (it != m_lightLocations.end()) {
        return it->second;
    }

    SpotLocations loc;
    loc.position    = shader->getUniformLocation("spotLight.position");
    loc.direction   = shader->getUniformLocation("spotLight.direction");
    loc.ambient     = shader->getUniformLocation("spotLight.ambient");
    loc.diffuse     = shader->getUniformLocation("spotLight.diffuse");
    loc.specular    = shader->getUniformLocation("spotLight.specular");
    loc.constant    = shader->getUniformLocation("spotLight.constant");
    loc.linear      = shader->getUniformLocation("spotLight.linear");
    loc.quadratic   = shader->getUniformLocation("spotLight.quadratic");
    loc.cutOff      = shader->getUniformLocation("spotLight.cutOff");
    loc.outerCutOff = shader->getUniformLocation("spotLight.outerCutOff");

    return m_lightLocations.emplace(programId, loc).first->second;
}

void Spotlight::applyToShader(Shader* shader, bool isEnabled) {
    // Flashlight — hot path : uniquement des GLint pré-calculées.
    const SpotLocations& loc = ensureLocations(shader);

    shader->setVec3(loc.position, m_position);
    shader->setVec3(loc.direction, m_direction->getDirectionVector());
    shader->setFloat(loc.cutOff, m_cutOff);
    shader->setFloat(loc.outerCutOff, m_outerCutOff);

    if (isEnabled) {
        // Mise � jour du timer et g�n�ration d'une nouvelle cible si n�cessaire
        m_flickerTimer += m_renderer->getDeltaTime(); // Assurez-vous d'avoir acc�s au deltaTime
        if (m_flickerTimer >= m_flickerChangeInterval) {
            m_targetFlicker = 0.85f + (rand() % 30) / 100.0f; // Entre 0.85 et 1.15
            m_flickerTimer = 0.0f;
        }

        // Interpolation lin�aire vers la valeur cible
        m_currentFlicker += (m_targetFlicker - m_currentFlicker) * m_smoothingSpeed * m_renderer->getDeltaTime();

        // Application de l'al�atoire liss� sur l'intensit�
        shader->setVec3(loc.ambient, m_ambient * m_currentFlicker);
        shader->setVec3(loc.diffuse, m_diffuse * m_currentFlicker);
        shader->setVec3(loc.specular, m_specular * m_currentFlicker);
    }
    else {
        shader->setVec3(loc.ambient, glm::vec3(0.0f, 0.0f, 0.0f));
        shader->setVec3(loc.diffuse, glm::vec3(0.0f, 0.0f, 0.0f));
        shader->setVec3(loc.specular, glm::vec3(0.0f, 0.0f, 0.0f));
    }
    shader->setFloat(loc.constant, m_constant);
    shader->setFloat(loc.linear, m_linear);
    shader->setFloat(loc.quadratic, m_quadratic);
}