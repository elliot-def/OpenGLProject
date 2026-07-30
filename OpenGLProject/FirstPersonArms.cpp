#include "FirstPersonArms.h"
#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "LightManager.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

FirstPersonArms::FirstPersonArms(Camera* camera, LightManager* lightManager,
                                 const std::string& modelPath, TextureManager* textureManager)
    : m_camera(camera), m_lightManager(lightManager),
      m_model(std::make_unique<Model>(camera, lightManager, modelPath, textureManager)) {

    // Texture blanche de fallback (1x1) si le modele n'a pas de textures
    unsigned char white[] = { 255, 255, 255, 255 };
    glGenTextures(1, &m_fallbackTexture);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

FirstPersonArms::~FirstPersonArms() {
    if (m_fallbackTexture)
        glDeleteTextures(1, &m_fallbackTexture);
}

const std::vector<Mesh*>& FirstPersonArms::getMeshes() {
    return m_model->getMeshes();
}

void FirstPersonArms::draw(Shader* shader) {
    auto& meshes = m_model->getMeshes();

    // Modele en +Z (FBX Mixamo), camera regarde -Z => rotation 180 Y
    glm::mat4 armModel = glm::mat4(1.0f);
    armModel = glm::translate(armModel, glm::vec3(Constants::FP_ARMS_OFFSET_X, Constants::FP_ARMS_OFFSET_Y, Constants::FP_ARMS_OFFSET_Z));
    armModel = glm::rotate(armModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    armModel = glm::scale(armModel, glm::vec3(Constants::FP_ARMS_SCALE));

    shader->use();

    shader->setMat4("model", armModel);
    shader->setMat4("view", glm::mat4(1.0f));
    shader->setMat4("projection", shader->getProjection());
    shader->setVec3("viewPos", glm::vec3(0.0f));

    // Desactiver les lumieres monde (dir + spot)
    shader->setVec3("dirLight.ambient",  glm::vec3(0.0f));
    shader->setVec3("dirLight.diffuse",  glm::vec3(0.0f));
    shader->setVec3("dirLight.specular", glm::vec3(0.0f));
    shader->setVec3("spotLight.ambient",  glm::vec3(0.0f));
    shader->setVec3("spotLight.diffuse",  glm::vec3(0.0f));
    shader->setVec3("spotLight.specular", glm::vec3(0.0f));

    // Lumiere locale en espace camera
    shader->setInt("numberLightSources", 1);
    shader->setVec3("lightSources[0].position", glm::vec3(0.2f, 0.8f, -0.3f));
    shader->setVec3("lightSources[0].ambient",  Constants::FP_ARMS_SKIN_COLOR * 0.5f);
    shader->setVec3("lightSources[0].diffuse",  Constants::FP_ARMS_SKIN_COLOR);
    shader->setVec3("lightSources[0].specular", glm::vec3(0.15f));
    shader->setFloat("lightSources[0].constant",  1.0f);
    shader->setFloat("lightSources[0].linear",    0.14f);
    shader->setFloat("lightSources[0].quadratic", 0.07f);

    // Fallback texture blanche si le mesh n'a pas de textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);

    shader->setInt("texture_diffuse", 0);
    shader->setInt("texture_specular", 1);

    for (auto* mesh : meshes) {
        mesh->draw();
    }
}
