#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class Model;
class Mesh;
class Shader;
class Camera;
class LightManager;
class TextureManager;
class Animator;

class FirstPersonArms {
public:
    FirstPersonArms(Camera* camera, LightManager* lightManager,
                    const std::string& modelPath, TextureManager* textureManager);
    ~FirstPersonArms();

    void draw(Shader* shader);
    void update(float deltaTime, const glm::vec3& playerPos, bool isSprinting);

    Model* getModel() { return m_model.get(); }
    const std::vector<Mesh*>& getMeshes();

private:
    Camera* m_camera = nullptr;
    LightManager* m_lightManager = nullptr;
    std::unique_ptr<Model> m_model;
    std::unique_ptr<Animator> m_animator;
    unsigned int m_fallbackTexture = 0;

    float m_animTime = 0.0f;
    std::vector<std::string> m_boneUniformNames; // pré-calculés "uBoneMatrices[0]".."[51]"
};
