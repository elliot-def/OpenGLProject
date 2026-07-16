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
	m_direction = direction;
}

void Spotlight::update(Player* player) {
	m_position = player->getPosition() + Constants::PLAYER_EYE_HEIGHT;
	m_direction = player->getDirection();
}

void Spotlight::applyToShader(Shader* shader, bool isEnabled) {
    // Flashlight

    shader->setVec3("spotLight.position", m_position);
    shader->setVec3("spotLight.direction", m_direction->getDirectionVector());
    shader->setFloat("spotLight.cutOff", m_cutOff);
    shader->setFloat("spotLight.outerCutOff", m_outerCutOff);

    if (isEnabled) {
        // Mise à jour du timer et génération d'une nouvelle cible si nécessaire
        m_flickerTimer += m_renderer->getDeltaTime(); // Assurez-vous d'avoir accès au deltaTime
        if (m_flickerTimer >= m_flickerChangeInterval) {
            m_targetFlicker = 0.85f + (rand() % 30) / 100.0f; // Entre 0.85 et 1.15
            m_flickerTimer = 0.0f;
        }

        // Interpolation linéaire vers la valeur cible
        m_currentFlicker += (m_targetFlicker - m_currentFlicker) * m_smoothingSpeed * m_renderer->getDeltaTime();

        // Application de l'aléatoire lissé sur l'intensité
        shader->setVec3("spotLight.ambient", m_ambient * m_currentFlicker);
        shader->setVec3("spotLight.diffuse", m_diffuse * m_currentFlicker);
        shader->setVec3("spotLight.specular", m_specular * m_currentFlicker);
    }
    else {
        shader->setVec3("spotLight.ambient", glm::vec3(0.0f, 0.0f, 0.0f));
        shader->setVec3("spotLight.diffuse", glm::vec3(0.0f, 0.0f, 0.0f));
        shader->setVec3("spotLight.specular", glm::vec3(0.0f, 0.0f, 0.0f));
    }
    shader->setFloat("spotLight.constant", m_constant);
    shader->setFloat("spotLight.linear", m_linear);
    shader->setFloat("spotLight.quadratic", m_quadratic);
}