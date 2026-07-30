#pragma once

#include <memory>
#include <string>
#include <vector>

class Model;
class Mesh;
class Shader;
class Camera;
class LightManager;
class TextureManager;

class FirstPersonArms {
public:
    FirstPersonArms(Camera* camera, LightManager* lightManager,
                    const std::string& modelPath, TextureManager* textureManager);
    ~FirstPersonArms();

    void draw(Shader* shader);

    Model* getModel() { return m_model.get(); }
    const std::vector<Mesh*>& getMeshes();

private:
    Camera* m_camera = nullptr;
    LightManager* m_lightManager = nullptr;
    std::unique_ptr<Model> m_model;
    unsigned int m_fallbackTexture = 0;
};
