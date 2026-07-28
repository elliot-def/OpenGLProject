#include "ModelEntity.h"
#include "Model.h"
#include "Shader.h"
#include "Camera.h"
#include "LightManager.h"
#include <memory>

#include <glad/glad.h>  // GL_TRUE/GL_FALSE/glDepthMask — requise par l'outline pass

ModelEntity::ModelEntity(Camera* camera, LightManager* lightManager, Renderer* renderer, const std::string& modelPath, TextureManager* textureManager)
	: m_camera(camera), m_lightManager(lightManager), Entity(renderer, nullptr) {
	m_model = std::make_unique<Model>(m_camera, m_lightManager, modelPath, textureManager);
}

ModelEntity::~ModelEntity() {
    // Le destructeur unique_ptr s'occupe de lib�rer la m�moire du mod�le
}

void ModelEntity::draw(Shader* shader) {
    // Reconstruction de la matrice modele (meme logique que getModelMatrix()).
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    glm::vec3 dir = m_direction->getDirectionVector();
    float yaw = atan2(dir.x, dir.z);
    model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));

    // ── OUTLINE PASS ─────────────────────────────────────────────────────────────
    if (m_outlineEnabled && m_outlineShader) {
        m_outlineShader->use();
        glm::mat4 outline_model = glm::scale(model, glm::vec3(1.0f + m_outlineThickness));
        m_outlineShader->setMat4("uModel", outline_model);
        m_outlineShader->setMat4("uView", m_camera->getViewMatrix());
        m_outlineShader->setMat4("uProjection", shader->getProjection());
        m_outlineShader->setVec3("uOutlineColor", m_outlineColor);

        glDepthMask(GL_FALSE);
        m_model->draw(*m_outlineShader);
        glDepthMask(GL_TRUE);
    }

    shader->use();
    shader->setModel(model);      // il faut setter le model avant
    shader->setupMatrices();      // envoie model + view + projection

    shader->setVec3("viewPos", m_camera->getPosition());
    m_lightManager->applyToShader(shader);

    m_model->draw(*shader);
}

void ModelEntity::drawDebug(Shader* shader) {
    shader->use();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);

    glm::vec3 dir = m_direction->getDirectionVector();
    float yaw = atan2(dir.x, dir.z);
    model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));

    shader->setMat4("model", model);

    // Dessiner la bounding box
    m_model->drawBoundingBox(*shader);
}

bool ModelEntity::checkCollision(const ModelEntity& other) const {
    glm::mat4 thisMatrix = getModelMatrix();
    glm::mat4 otherMatrix = other.getModelMatrix();

    return m_model->checkCollision(*other.m_model, thisMatrix, otherMatrix);
}

bool ModelEntity::raycast(const glm::vec3& origin, const glm::vec3& direction,
    float& distance) const {
    glm::mat4 modelMatrix = getModelMatrix();
    return m_model->raycast(origin, direction, modelMatrix, distance);
}

BoundingBox ModelEntity::getWorldBoundingBox() const {
    return m_model->getTransformedBoundingBox(getModelMatrix());
}

glm::mat4 ModelEntity::getModelMatrix() const {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);

    glm::vec3 dir = m_direction->getDirectionVector();
    float yaw = atan2(dir.x, dir.z);
    model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));

    return model;
}