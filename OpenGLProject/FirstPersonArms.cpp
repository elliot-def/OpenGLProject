#include "FirstPersonArms.h"
#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "LightManager.h"
#include "Animator.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

FirstPersonArms::FirstPersonArms(Camera* camera, LightManager* lightManager,
                                 const std::string& modelPath, TextureManager* textureManager)
    : m_camera(camera), m_lightManager(lightManager),
      m_model(std::make_unique<Model>(camera, lightManager, modelPath, textureManager)),
      m_animator(std::make_unique<Animator>()) {

    m_animator->setup(m_model.get());

    // Pré-calculer les noms d'uniforms des bones
    for (int i = 0; i < MAX_BONES; i++) {
        m_boneUniformNames.push_back("uBoneMatrices[" + std::to_string(i) + "]");
    }

    // Jouer l'idle par défaut (chercher "guard_idle" par nom)
    const aiScene* scene = m_model->getScene();
    if (scene) {
        printf("[FPArms] scene OK, animations=%u, bones=%zu\n",
               scene->mNumAnimations, m_model->getBoneInfoMap().size());
        for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
            std::string name(scene->mAnimations[i]->mName.C_Str());
            if (name.find("guard_idle") != std::string::npos) {
                m_animator->playAnimation(i, true);
                break;
            }
        }
        if (!m_animator->isPlaying() && scene->mNumAnimations > 0) {
            printf("[FPArms] guard_idle pas trouve, fallback anim 0\n");
            m_animator->playAnimation(0, true);
        }
    } else {
        printf("[FPArms] ERREUR: scene NULL!\n");
    }

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

void FirstPersonArms::update(float deltaTime, const glm::vec3& playerPos, bool isSprinting) {
    m_animator->update(deltaTime);
}

const std::vector<Mesh*>& FirstPersonArms::getMeshes() {
    return m_model->getMeshes();
}

void FirstPersonArms::draw(Shader* shader) {
    auto& meshes = m_model->getMeshes();
    glm::mat4 armModel = glm::mat4(1.0f);
    armModel = glm::translate(armModel, glm::vec3(
        Constants::FP_ARMS_OFFSET_X,
        Constants::FP_ARMS_OFFSET_Y,
        Constants::FP_ARMS_OFFSET_Z));
    armModel = glm::rotate(armModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    armModel = glm::scale(armModel, glm::vec3(Constants::FP_ARMS_SCALE));

    shader->use();

    shader->setMat4("model", armModel);
    shader->setMat4("view", glm::mat4(1.0f));
    shader->setMat4("projection", shader->getProjection());
    shader->setVec3("viewPos", glm::vec3(0.0f));

    // Envoyer les matrices des bones au shader (noms pré-calculés)
    const auto& boneMats = m_animator->getFinalBoneMatrices();
    size_t count = boneMats.size() < m_boneUniformNames.size() ? boneMats.size() : m_boneUniformNames.size();
    for (size_t i = 0; i < count; i++) {
        shader->setMat4(m_boneUniformNames[i].c_str(), boneMats[i]);
    }

    // Désactiver les lumières monde
    shader->setVec3("dirLight.ambient",  glm::vec3(0.0f));
    shader->setVec3("dirLight.diffuse",  glm::vec3(0.0f));
    shader->setVec3("dirLight.specular", glm::vec3(0.0f));
    shader->setVec3("spotLight.ambient",  glm::vec3(0.0f));
    shader->setVec3("spotLight.diffuse",  glm::vec3(0.0f));
    shader->setVec3("spotLight.specular", glm::vec3(0.0f));

    // Lumière locale en espace caméra
    shader->setInt("numberLightSources", 1);
    shader->setVec3("lightSources[0].position", glm::vec3(0.2f, 0.8f, -0.3f));
    shader->setVec3("lightSources[0].ambient",  Constants::FP_ARMS_SKIN_COLOR * 0.5f);
    shader->setVec3("lightSources[0].diffuse",  Constants::FP_ARMS_SKIN_COLOR);
    shader->setVec3("lightSources[0].specular", glm::vec3(0.15f));
    shader->setFloat("lightSources[0].constant",  1.0f);
    shader->setFloat("lightSources[0].linear",    0.14f);
    shader->setFloat("lightSources[0].quadratic", 0.07f);

    // Fallback texture blanche
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
