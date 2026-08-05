#pragma once
#include <string>
#include <memory>
#include <vector>

#include "Entity.h"
#include "Model.h"


class Shader;
class Model;
class TextureManager;
class Camera;
class LightManager;
class Animator;
class Direction;

struct BoundingBox;

class ModelEntity : public Entity {
public:
    ModelEntity(Camera* camera, LightManager* lightManager, Renderer* renderer, const std::string& modelPath, TextureManager* textureManager);

    ~ModelEntity();

    void draw(Shader* shader) override;

    void drawDebug(Shader* shader);

    // Animation du modèle riggé (à appeler chaque frame)
    void updateAnimation(float deltaTime);

    // Joue une animation par son index (ou -1 pour ne rien changer)
    void playAnimation(int animIndex, bool loop = true);

    // Joue l'animation d'idle (détectée automatiquement)
    void playIdle();

    // Joue l'animation de marche (détectée automatiquement)
    void playWalk();

    // Index des animations détectées
    int getIdleAnimIndex() const { return m_idleAnimIndex; }
    int getWalkAnimIndex() const { return m_walkAnimIndex; }
    int getRunAnimIndex() const { return m_runAnimIndex; }
    int getPunchAnimIndex() const { return m_punchAnimIndex; }
    int getRestAnimIndex() const { return m_restAnimIndex; }

    // Retourne l'Animator pour un contrôle avancé
    Animator* getAnimator() { return m_animator.get(); }
    const Animator* getAnimator() const { return m_animator.get(); }

    // Indique si le modèle a des animations
    bool hasAnimations() const { return m_hasAnimations; }

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

    // ── Cache de la matrice modele ───────────────────────────────────────
    // getModelMatrix() est appele 3-4 fois par frame (draw, drawDebug,
    // checkCollision, getWorldBoundingBox, updateDynamic/BVH). La matrice
    // n'est reconstruite que si position / direction / spin ont change depuis
    // le dernier appel. La comparaison de cles est robuste meme si un
    // mutateur est contourne (m_position/m_spinAngle modifies directement).
    mutable glm::mat4 m_cachedModelMatrix{ 1.0f };
    mutable glm::vec3 m_cachedPos{ 0.0f };
    mutable const Direction* m_cachedDirPtr = nullptr;
    mutable unsigned int m_cachedDirVersion = 0;
    mutable glm::vec3 m_cachedSpinAxis{ 0.0f };
    mutable float m_cachedSpinAngle = 0.0f;
    mutable bool m_modelMatrixValid = false;
    Camera* m_camera;
	LightManager* m_lightManager;
    std::unique_ptr<Model> m_model;

    // ── Animation ──
    std::unique_ptr<Animator> m_animator;
    bool m_hasAnimations = false;
    int m_idleAnimIndex = -1;
    int m_walkAnimIndex = -1;
    int m_runAnimIndex = -1;    // course (sprint) — "Run-M"
    int m_punchAnimIndex = -1;  // jab one-shot (touche R) — "Left-Punch-M"
    int m_restAnimIndex = -1;   // pose d'attente avant l'idle — "Rest"

    void detectAnimations();
};