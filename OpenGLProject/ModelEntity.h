#pragma once
#include <string>
#include <memory>

#include "Entity.h"
#include "Model.h"


class Shader;
class Model;
class TextureManager;
class Camera;
class LightManager;

struct BoundingBox;

class ModelEntity : public Entity {
public:
    ModelEntity(Camera* camera, LightManager* lightManager, Renderer* renderer, const std::string& modelPath, TextureManager* textureManager);

    ~ModelEntity();

    void draw(Shader* shader) override;

    void drawDebug(Shader* shader);

    // Vérifier collision avec une autre entité
    bool checkCollision(const ModelEntity& other) const;

    // Raycast pour détecter si on clique sur l'entité
    bool raycast(const glm::vec3& origin, const glm::vec3& direction, float& distance) const;

    // Obtenir la bounding box transformée
    BoundingBox getWorldBoundingBox() const;

    Model* getModel() { return m_model.get(); }
    const std::vector<Mesh*>& getMeshes() { return m_model.get()->getMeshes(); }

    glm::mat4 getModelMatrix() const;


private:
    Camera* m_camera;
	LightManager* m_lightManager;
    std::unique_ptr<Model> m_model;
};