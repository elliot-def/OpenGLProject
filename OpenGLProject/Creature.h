#pragma once

#include <vector>
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
// IA : A* sur grille (XZ) evitant murs et portes fermees (testSphereAll),
// recalcule toutes les kRepathInterval s ; ligne droite si degagee. Marche vers le joueur à kSpeed u/s (plus rapide que la marche,
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

    // Chemin A* jusqu a goal (waypoints monde, lisses par visibilite).
    // false si aucun chemin (cible enfermee / trop loin) -> repli ligne droite.
    bool findPath(const glm::vec3& goal, const CollisionManager* cm);
    bool isFree(const glm::vec3& p, const CollisionManager* cm) const;
    bool lineClear(const glm::vec3& a, const glm::vec3& b, const CollisionManager* cm) const;

    // ── Réglages ──────────────────────────────────────────────────────────
    static constexpr float kSpeed        = 3.0f;   // u/s (marche 2.5, sprint 4.0)
    static constexpr float kStopDistance = 1.2f;   // distance d'arrêt (« attaque »)
    static constexpr float kBodyRadius   = 0.35f;  // rayon du sweepSphere
    static constexpr float kStepHeight   = 0.10f;  // hauteur franchie sans blocage (tapis)
    static constexpr float kCellSize     = 0.25f;  // pas de la grille A* (< passage d une porte)
    static constexpr float kPathMargin   = 10.0f;  // marge autour du segment creature-joueur
    static constexpr int   kMaxExpand    = 20000;  // garde-fou cout A*
    static constexpr float kRepathInterval = 0.5f; // s (voit les portes ouvertes/fermees)
    static constexpr float kHeadBaseY    = 1.43f;  // hauteur du cou (tete coupee au cou y=-0.72)
    static constexpr float kHeadScale    = 0.45f;  // tête ≈ 0.65 u de haut
    static constexpr float kTurnRate    = 6.0f;   // lissage du cap (1/s)
    static constexpr float kWalkCycleHz  = 2.2f;   // fréquence du balancement
    static constexpr float kSwingDeg     = 28.0f;  // amplitude du balancement

    ModelEntity* m_head = nullptr;

    std::vector<glm::vec3> m_path;    // waypoints (m_pathIdx = courant)
    size_t m_pathIdx = 0;
    float  m_repathTimer = 0.0f;

    glm::vec3 m_position{ 0.0f };     // au sol (y = 0)
    glm::vec3 m_faceDir{ 0.0f, 0.0f, 1.0f };  // cap horizontal (normalisé)
    float     m_walkTime = 0.0f;      // temps de marche (balancement)
    bool      m_moving   = false;
};
