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
// Entrée de collider statique
// ─────────────────────────────────────────────────────────────────────────────

struct StaticBox {
    std::string name;
    AABB        aabb;
};
