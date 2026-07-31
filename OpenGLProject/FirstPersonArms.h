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

    // Joue une fois l'animation de tir ("finger_gun_fire") puis revient a l'idle
    void triggerFire();

    Model* getModel() { return m_model.get(); }
    const std::vector<Mesh*>& getMeshes();

private:
    Camera* m_camera = nullptr;
    LightManager* m_lightManager = nullptr;
    std::unique_ptr<Model> m_model;
    std::unique_ptr<Animator> m_animator;
    unsigned int m_fallbackTexture = 0;

    float m_animTime = 0.0f;
    int m_idleAnimIndex = -1;   // index de "rest" (retour apres tir)
    int m_fireAnimIndex = -1;   // index de "finger_gun_fire" (clic gauche)
    glm::vec3 m_playerPos{ 0.0f }; // position du joueur (attachement world-space 3P)
    std::vector<std::string> m_boneUniformNames; // pré-calculés "uBoneMatrices[0]".."[51]"
};
