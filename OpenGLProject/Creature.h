#pragma once

#include <glm/glm.hpp>

class ModelEntity;
class CollisionManager;
class CubeRenderer;
class Shader;

// ─────────────────────────────────────────────────────────────────────────────
// Creature : ennemi qui POURSUIT le joueur.
//
// Aspect : corps sombre en cubes (torse, jambes, bras qui balancent — rendu
// instancié via CubeRenderer, couleur émissive quasi noire) coiffé de la TÊTE
// DE FROPY (res/models/fropy/fropy_head.obj, extrait de fropy_low_poly.obj).
//
// IA : marche droit vers le joueur à kSpeed u/s (plus rapide que la marche,
// rattrapable en sprint), s'arrête à kStopDistance. Le déplacement passe par
// CollisionManager::sweepSphere → la créature glisse le long des murs au lieu
// de les traverser. La tête fait face au joueur (cap lissé).
// ─────────────────────────────────────────────────────────────────────────────
class Creature {
public:
    // head : entité modèle de la tête (possédée par la Scene, déjà chargée).
    Creature(ModelEntity* head, const glm::vec3& spawnPosition);

    // Poursuite + cap + synchronisation de la tête. collisionManager peut
    // être null (déplacement sans collision, utile pour les tests).
    void update(float deltaTime, const glm::vec3& playerPos,
                CollisionManager* collisionManager);

    // Soumet les cubes du corps au rendu instancié (1 instance par partie).
    void submitBody(CubeRenderer* cubeRenderer, Shader* bodyShader) const;

    // Téléporte la créature (spawn / respawn après auto-test).
    void setPosition(const glm::vec3& position);

    const glm::vec3& getPosition() const { return m_position; }
    float getSpeed() const { return kSpeed; }

private:
    void syncHead();

    // ── Réglages ──────────────────────────────────────────────────────────
    static constexpr float kSpeed        = 3.0f;   // u/s (marche 2.5, sprint 4.0)
    static constexpr float kStopDistance = 1.2f;   // distance d'arrêt (« attaque »)
    static constexpr float kBodyRadius   = 0.35f;  // rayon du sweepSphere
    static constexpr float kStepHeight   = 0.10f;  // hauteur franchie sans blocage (tapis)
    static constexpr float kHeadBaseY    = 1.52f;  // hauteur du cou
    static constexpr float kHeadScale    = 0.45f;  // tête ≈ 0.65 u de haut
    static constexpr float kTurnRate    = 6.0f;   // lissage du cap (1/s)
    static constexpr float kWalkCycleHz  = 2.2f;   // fréquence du balancement
    static constexpr float kSwingDeg     = 28.0f;  // amplitude du balancement

    ModelEntity* m_head = nullptr;

    glm::vec3 m_position{ 0.0f };     // au sol (y = 0)
    glm::vec3 m_faceDir{ 0.0f, 0.0f, 1.0f };  // cap horizontal (normalisé)
    float     m_walkTime = 0.0f;      // temps de marche (balancement)
    bool      m_moving   = false;
};
