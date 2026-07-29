#include "ArmsRenderer.h"

#include "Camera.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "LightManager.h"
#include "Model.h"
#include "Animator.h"
#include "Mesh.h"        // pour mesh->draw() sur Model::getMeshes()
#include "constants.h"

#include <glad/glad.h>  // GL_DEPTH_TEST, GL_DEPTH_BUFFER_BIT, glDisable/glClear
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>

void ArmsRenderer::drawFP(Camera* camera,
    ShaderManager* shaderManager,
    LightManager* lightManager,
    Model* armsModel,
    Animator* armsAnimator) {
    if (!camera || !shaderManager || !armsModel || !armsAnimator) return;

    Shader* skinned = nullptr;
    try { skinned = shaderManager->getShader("skinned"); }
    catch (const std::exception& e) { return; }
    if (!skinned) return;

    // 1. Activer le shader EN PREMIER
    skinned->use();

    // 2. Transmettre les matrices Caméra / Projection
    glm::vec3 eyePos = camera->getPosition();
    glm::vec3 front = camera->getFront();
    glm::vec3 up = camera->getUp();

    float forwardOffset = Constants::FP_ARMS_FORWARD_OFFSET;
    float downOffset = Constants::FP_ARMS_DOWN_OFFSET;

    glm::mat4 armsModelMat(1.0f);
    armsModelMat = glm::translate(armsModelMat, eyePos - up * downOffset + front * forwardOffset);

    glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    if (glm::length(flatFront) > 1e-4f) {
        float yaw = std::atan2(flatFront.x, flatFront.z) + Constants::FP_ARMS_YAW_OFFSET;
        armsModelMat = glm::rotate(armsModelMat, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    armsModelMat = glm::scale(armsModelMat, glm::vec3(Constants::FP_ARMS_SCALE));

    constexpr float kNearPlane = 0.05f;
    const glm::mat4 savedProj = skinned->getProjection();
    const glm::mat4 armsProj = glm::perspective(
        glm::radians(60.0f),
        static_cast<float>(Constants::WINDOW_WIDTH) / static_cast<float>(Constants::WINDOW_HEIGHT),
        kNearPlane,
        50.0f
    );

    skinned->setProjection(armsProj);
    skinned->setModel(armsModelMat);
    skinned->setupMatrices(); // Configure Model, View, Projection

    // 3. Envoyer uBoneMatrices APRES setupMatrices()
    const std::vector<glm::mat4>& bones = armsAnimator->getFinalBoneMatrices();
    if (!bones.empty()) {
        glm::mat4 padded[100];
        int count = std::min(static_cast<int>(bones.size()), 100);
        for (int i = 0; i < count; ++i) padded[i] = bones[i];
        for (int i = count; i < 100; ++i) padded[i] = glm::mat4(1.0f);

        // Envoi direct du tableau
        skinned->setMat4Array("uBoneMatrices", padded, 100);
    }

    // 4. Textures & Lumières
    skinned->setInt("texture_diffuse", 0);
    skinned->setInt("texture_specular", 1);
    skinned->setVec3("viewPos", eyePos);
    if (lightManager) {
        lightManager->applyToShader(skinned);
    }
    else {
        skinned->setInt("numLights", 0);
    }

    // 5. Rendu OpenGL
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE); // Désactive le culling pour éviter les faces inversées par le rig

    for (auto* mesh : armsModel->getMeshes()) {
        mesh->draw();
    }

    glEnable(GL_CULL_FACE);
    skinned->setProjection(savedProj);
}