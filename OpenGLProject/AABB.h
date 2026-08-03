#pragma once

#include <limits>
#include <glm/glm.hpp>
#include <string>

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
// OBB (Oriented Bounding Box)
// ─────────────────────────────────────────────────────────────────────────────
// Boîte englobante ORIENTÉE : centre + demi-étendues le long des axes locaux +
// matrice de rotation (colonnes = axes locaux du box, orthonormés).
// Contrairement à l'AABB, elle suit la rotation de l'objet : c'est elle qu'il
// faut utiliser pour les objets dynamiques qui pivotent (ex: cube qui tourne).
struct OBB {
    glm::vec3 center{ 0.0f };        // centre world
    glm::vec3 halfExtents{ 0.0f };   // demi-étendues le long des axes locaux
    glm::mat3 rotation{ 1.0f };      // rotation pure (Rᵀ = R⁻¹)

    bool isValid() const {
        return halfExtents.x > 0.0f && halfExtents.y > 0.0f && halfExtents.z > 0.0f;
    }

    // Passe un vecteur world dans le repère local du box (rotation pure →
    // produit scalaire avec les colonnes = multiplication par Rᵀ).
    glm::vec3 toLocal(const glm::vec3& world) const {
        return glm::vec3(glm::dot(rotation[0], world),
                         glm::dot(rotation[1], world),
                         glm::dot(rotation[2], world));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Entrée de collider statique
// ─────────────────────────────────────────────────────────────────────────────

struct StaticBox {
    std::string name;
    AABB        aabb;
};

// ─────────────────────────────────────────────────────────────────────────────
// Entrée de collider dynamique (objet mobile)
// ─────────────────────────────────────────────────────────────────────────────
// AABB : englobante rapide (debug/affichage). OBB : test de collision précis,
// prend la rotation en compte (un cube incliné a enfin une hitbox inclinée).
struct DynamicBox {
    AABB aabb;
    OBB  obb;
};
