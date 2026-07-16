#pragma once

#define NOMINMAX
#include <Windows.h>

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <limits>

#include "Constants.h"

class Mesh;

// ─────────────────────────────────────────────────────────────────────────────
// AABB (Axis-Aligned Bounding Box)
// ─────────────────────────────────────────────────────────────────────────────

struct AABB {
    glm::vec3 min{ (std::numeric_limits<float>::max)() };
    glm::vec3 max{ -(std::numeric_limits<float>::max)() };

    void expand(glm::vec3 p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }
    void expand(const AABB& o) {
        min = glm::min(min, o.min);
        max = glm::max(max, o.max);
    }

    glm::vec3 center() const { return (min + max) * 0.5f; }
    bool isValid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }

    // Retourne le point de l'AABB le plus proche de p
    glm::vec3 closestPoint(glm::vec3 p) const {
        return glm::clamp(p, min, max);
    }

    bool intersectsSphere(glm::vec3 center, float radius) const {
        glm::vec3 closest = glm::clamp(center, min, max);
        return glm::length(center - closest) <= radius;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Résultat d'un test de collision sphère/AABB
// ─────────────────────────────────────────────────────────────────────────────

struct CollisionResult {
    bool      hit = false;
    glm::vec3 normal = glm::vec3(0.0f);   // Normale de la surface touchée (world space)
    float     penetration = 0.0f;              // Profondeur de pénétration
};

// ─────────────────────────────────────────────────────────────────────────────
// Entrée de collider statique
// ─────────────────────────────────────────────────────────────────────────────

struct StaticBox {
    std::string name;
    AABB        aabb;
};

// ─────────────────────────────────────────────────────────────────────────────
// CollisionManager
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class CollisionManager
 *
 * Système de collision basé sur des AABB.
 *
 * Deux catégories :
 *  - Statiques  (carte, décor) : AABB construite une fois au chargement
 *  - Dynamiques (ennemis, objets mobiles) : AABB mise à jour via updateDynamic()
 *
 * Usage :
 * @code
 * // --- Chargement ---
 * collisionManager.addStaticMesh(cubeMesh, modelMatrix, "sol");
 * collisionManager.addDynamicMesh("ennemi1", { enemyMesh }, enemyModelMatrix);
 *
 * // --- Chaque frame ---
 * collisionManager.updateDynamic("ennemi1", { enemyMesh }, newModelMatrix);
 *
 * glm::vec3 newPos = collisionManager.resolvePlayerMovement(
 *     playerPos, desiredMovement, deltaTime);
 * @endcode
 */
class CollisionManager {
public:
    CollisionManager() = default;
    ~CollisionManager() = default;

    // ── Enregistrement ───────────────────────────────────────────────────────

    /**
     * Ajoute un mesh statique. Calcule son AABB world-space et la stocke.
     * @param mesh        Pointeur vers le Mesh OpenGL
     * @param modelMatrix Transform world-space du mesh
     * @param name        Identifiant debug (optionnel)
     */
    void addStaticMesh(const Mesh* mesh,
        const glm::mat4& modelMatrix,
        const std::string& name = "");

    /**
     * Ajoute un ou plusieurs meshes dynamiques sous une clé commune.
     * L'AABB englobante de tous les sous-meshes est calculée immédiatement.
     */
    void addDynamicMesh(const std::string& key,
        const std::vector<Mesh*>& meshes,
        const glm::mat4& modelMatrix);

    /**
     * Met à jour l'AABB d'un groupe de meshes dynamiques.
     * Appeler chaque frame si l'objet a bougé.
     */
    void updateDynamic(const std::string& key,
        const std::vector<Mesh*>& meshes,
        const glm::mat4& modelMatrix);

    void removeDynamic(const std::string& key);
    void clear();

    // ── Résolution mouvement joueur ──────────────────────────────────────────

    /**
     * @brief Résout le mouvement du joueur avec sliding et gravité.
     *
     * Le joueur est modélisé comme une capsule (2 sphères) pour gérer
     * correctement les marches et les pentes.
     *
     * @param currentPos      Centre bas de la capsule (pieds du joueur)
     * @param desiredMovement Déplacement voulu ce frame (hors gravité)
     * @param deltaTime       Temps écoulé en secondes
     * @param radius          Rayon de la sphère de la capsule
     * @param height          Hauteur totale de la capsule
     * @return                Nouvelle position du joueur
     */
    glm::vec3 resolvePlayerMovement(glm::vec3 currentPos,
        glm::vec3 desiredMovement,
        float     deltaTime,
        bool      gravityEnabled,
        float     radius = Constants::DEFAULT_PLAYER_RADIUS,
        float     height = Constants::DEFAULT_PLAYER_HEIGHT);

    glm::vec3 pushPlayerAway(glm::vec3 currentPlayerPos);

    void  setVerticalVelocity(float vy) { m_verticalVelocity = vy; }
    float getVerticalVelocity()  const { return m_verticalVelocity; }
    bool  isGrounded()           const { return m_isGrounded; }

    // ── Debug ────────────────────────────────────────────────────────────────

    void printInfo() const;

private:
    std::vector<StaticBox>                     m_staticBoxes;
    std::unordered_map<std::string, AABB>      m_dynamicBoxes;

    float m_verticalVelocity = 0.0f;
    bool  m_isGrounded = false;

    // Construit l'AABB world-space d'un ensemble de meshes
    static AABB computeWorldAABB(const std::vector<Mesh*>& meshes,
        const glm::mat4& modelMatrix);

    // Teste une sphère contre une AABB et retourne le résultat de collision
    static CollisionResult testSphereAABB(glm::vec3 center,
        float     radius,
        const AABB& box);

    // Teste une sphère contre tous les colliders (statiques + dynamiques)
    CollisionResult testSphereAll(glm::vec3 center, float radius) const;

    // Déplace une sphère en résolvant itérativement les contacts (sliding)
    glm::vec3 sweepSphere(glm::vec3 start,
        glm::vec3 movement,
        float     radius,
        int       maxIterations = 5) const;
};