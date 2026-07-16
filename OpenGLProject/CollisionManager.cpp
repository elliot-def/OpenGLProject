#define NOMINMAX
#define GLM_ENABLE_EXPERIMENTAL
#include "CollisionManager.h"
#include "Mesh.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers internes
// ─────────────────────────────────────────────────────────────────────────────

// Calcule l'AABB world-space englobant tous les sous-meshes
AABB CollisionManager::computeWorldAABB(const std::vector<Mesh*>& meshes,
    const glm::mat4& modelMatrix)
{
    AABB result;
    for (const auto* mesh : meshes) {
        glm::vec3 lMin = mesh->getLocalAABBMin();
        glm::vec3 lMax = mesh->getLocalAABBMax();

        // Transforme les 8 coins de l'AABB locale pour obtenir une AABB world correcte
        glm::vec3 corners[8] = {
            { lMin.x, lMin.y, lMin.z },
            { lMax.x, lMin.y, lMin.z },
            { lMin.x, lMax.y, lMin.z },
            { lMax.x, lMax.y, lMin.z },
            { lMin.x, lMin.y, lMax.z },
            { lMax.x, lMin.y, lMax.z },
            { lMin.x, lMax.y, lMax.z },
            { lMax.x, lMax.y, lMax.z },
        };
        for (const auto& c : corners)
            result.expand(glm::vec3(modelMatrix * glm::vec4(c, 1.0f)));
    }
    return result;
}

// Test sphère / AABB
// Retourne la normale et la pénétration si collision détectée
CollisionResult CollisionManager::testSphereAABB(glm::vec3   center,
    float       radius,
    const AABB& box)
{
    CollisionResult result;
    if (!box.intersectsSphere(center, radius)) return result;

    glm::vec3 closest = box.closestPoint(center);
    glm::vec3 diff = center - closest;
    float     dist2 = glm::length2(diff);

    result.hit = true;

    bool insideBox = (closest == center);
    if (insideBox) {
        // Centre à l'intérieur de la boîte : cherche la face la plus proche
        // pour avoir une normale cohérente
        glm::vec3 halfSize = (box.max - box.min) * 0.5f;
        glm::vec3 boxCenter = box.center();
        glm::vec3 local = center - boxCenter;

        // Distance à chaque face (en valeur absolue par rapport au demi-côté)
        float dx = halfSize.x - std::abs(local.x);
        float dy = halfSize.y - std::abs(local.y);
        float dz = halfSize.z - std::abs(local.z);

        if (dx <= dy && dx <= dz) {
            result.normal = glm::vec3(local.x > 0.f ? 1.f : -1.f, 0.f, 0.f);
            result.penetration = radius + dx;
        }
        else if (dy <= dx && dy <= dz) {
            result.normal = glm::vec3(0.f, local.y > 0.f ? 1.f : -1.f, 0.f);
            result.penetration = radius + dy;
        }
        else {
            result.normal = glm::vec3(0.f, 0.f, local.z > 0.f ? 1.f : -1.f);
            result.penetration = radius + dz;
        }
    }
    else {
        float dist = std::sqrt(dist2);
        result.normal = diff / dist;
        result.penetration = radius - dist;
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// CollisionManager : enregistrement
// ─────────────────────────────────────────────────────────────────────────────

void CollisionManager::addStaticMesh(const Mesh* mesh,
    const glm::mat4& modelMatrix,
    const std::string& name)
{
    AABB box = computeWorldAABB({ const_cast<Mesh*>(mesh) }, modelMatrix);
    if (!box.isValid()) return;
    m_staticBoxes.push_back({ name, box });
}

void CollisionManager::addDynamicMesh(const std::string& key,
    const std::vector<Mesh*>& meshes,
    const glm::mat4& modelMatrix)
{
    AABB box = computeWorldAABB(meshes, modelMatrix);
    if (!box.isValid()) return;
    m_dynamicBoxes[key] = box;
}

void CollisionManager::updateDynamic(const std::string& key,
    const std::vector<Mesh*>& meshes,
    const glm::mat4& modelMatrix)
{
    AABB box = computeWorldAABB(meshes, modelMatrix);
    if (!box.isValid()) {
        removeDynamic(key);
        return;
    }
    m_dynamicBoxes[key] = box;
}

void CollisionManager::removeDynamic(const std::string& key) {
    m_dynamicBoxes.erase(key);
}

void CollisionManager::clear() {
    m_staticBoxes.clear();
    m_dynamicBoxes.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// CollisionManager : test global
// ─────────────────────────────────────────────────────────────────────────────

CollisionResult CollisionManager::testSphereAll(glm::vec3 center, float radius) const {
    CollisionResult best;

    for (const auto& sb : m_staticBoxes) {
        CollisionResult r = testSphereAABB(center, radius, sb.aabb);
        if (r.hit && r.penetration > best.penetration) best = r;
    }

    for (const auto& [key, box] : m_dynamicBoxes) {
        CollisionResult r = testSphereAABB(center, radius, box);
        if (r.hit && r.penetration > best.penetration) best = r;
    }

    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
// sweepSphere : déplace une sphère avec sliding itératif et substepping anti-tunneling
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// sweepSphere : Déplacement avec glissement et gestion stricte des blocages
// ─────────────────────────────────────────────────────────────────────────────
glm::vec3 CollisionManager::sweepSphere(glm::vec3 start,
    glm::vec3 movement,
    float     radius,
    int       maxIterations) const
{
    glm::vec3 pos = start;
    float moveLength = glm::length(movement);

    if (moveLength < 1e-8f) return pos;

    // Substepping adaptatif anti-tunneling
    float maxStepLength = radius * 0.4f;
    int substeps = 1;
    if (moveLength > maxStepLength) {
        substeps = static_cast<int>(std::ceil(moveLength / maxStepLength));
    }

    glm::vec3 subMovement = movement / static_cast<float>(substeps);

    for (int step = 0; step < substeps; ++step) {
        glm::vec3 remainingMove = subMovement;

        for (int iter = 0; iter < maxIterations; ++iter) {
            if (glm::length2(remainingMove) < 1e-8f) break;

            glm::vec3 target = pos + remainingMove;
            CollisionResult col = testSphereAll(target, radius);

            if (!col.hit) {
                pos = target;
                break;
            }

            // 1. On recule immédiatement la sphère hors du cube (Dépénétration stricte)
            // L'ajout d'un tout petit EPSILON évite que la virgule flottante ne la laisse "coller" au plan
            pos = target + col.normal * (col.penetration + 1e-3f);

            // 2. Glissement : On retire la partie du mouvement qui fonce dans le mur
            float project = glm::dot(remainingMove, col.normal);
            remainingMove = remainingMove - project * col.normal;

            // Sécurité anti-rebond infini dans les coins étroits
            if (project < 0.0f && glm::length2(remainingMove) < 1e-6f) {
                break;
            }
        }
    }

    return pos;
}

// ─────────────────────────────────────────────────────────────────────────────
// pushPlayerAway : Repousse activement le joueur s'il est chevauché par un cube
// ─────────────────────────────────────────────────────────────────────────────
glm::vec3 CollisionManager::pushPlayerAway(glm::vec3 currentPlayerPos) {
    
	float radius = Constants::DEFAULT_PLAYER_RADIUS;
	float height = Constants::DEFAULT_PLAYER_HEIGHT;
    
    glm::vec3 pos = currentPlayerPos;
    
    glm::vec3 offsetBottom = glm::vec3(0.f, radius, 0.f);
    glm::vec3 offsetMiddle = glm::vec3(0.f, height * 0.5f, 0.f);
    glm::vec3 offsetTop = glm::vec3(0.f, height - radius, 0.f);

    // On fait 3 passes rapides pour résoudre les chevauchements avec TOUS les objets (statiques & dynamiques)
    for (int pass = 0; pass < 3; ++pass) {
        // 1. Test aux pieds
        CollisionResult colBot = testSphereAll(pos + offsetBottom, radius);
        if (colBot.hit) {
            // On applique une force de dépénétration immédiate sur la position globale
            pos += colBot.normal * (colBot.penetration + 1e-3f);
        }

        // 2. Test au milieu
        CollisionResult colMid = testSphereAll(pos + offsetMiddle, radius);
        if (colMid.hit) {
            pos += colMid.normal * (colMid.penetration + 1e-3f);
        }

        // 3. Test à la tête
        CollisionResult colTop = testSphereAll(pos + offsetTop, radius);
        if (colTop.hit) {
            pos += colTop.normal * (colTop.penetration + 1e-3f);
        }
    }

    return pos;
}

// ─────────────────────────────────────────────────────────────────────────────
// resolvePlayerMovement : Résolution unifiée pour éviter le clipping dans les coins
// ─────────────────────────────────────────────────────────────────────────────
glm::vec3 CollisionManager::resolvePlayerMovement(glm::vec3 currentPos,
    glm::vec3 desiredMovement,
    float     deltaTime,
    bool      gravityEnabled,
    float     radius,
    float     height)
{
    if (std::isnan(currentPos.x) || std::isinf(currentPos.x)) {
        std::cerr << "[Collision] Position NaN/Inf détectée !" << std::endl;
        return currentPos;
    }

    glm::vec3 offsetBottom = glm::vec3(0.f, radius, 0.f);
    glm::vec3 offsetMiddle = glm::vec3(0.f, height * 0.5f, 0.f);
    glm::vec3 offsetTop = glm::vec3(0.f, height - radius, 0.f);

    m_isGrounded = false;

    // ── 1. MOUVEMENT HORIZONTAL ──────────────────────────────────────────────
    glm::vec3 horizontalMovement = glm::vec3(desiredMovement.x, 0.f, desiredMovement.z);
    glm::vec3 posAfterH = currentPos;

    if (glm::length2(horizontalMovement) > 1e-8f) {
        glm::vec3 newBottom = sweepSphere(posAfterH + offsetBottom, horizontalMovement, radius);
        posAfterH = newBottom - offsetBottom;

        for (int pass = 0; pass < 3; ++pass) {
            CollisionResult colMid = testSphereAll(posAfterH + offsetMiddle, radius);
            if (colMid.hit) posAfterH += colMid.normal * (colMid.penetration + 1e-3f);

            CollisionResult colTop = testSphereAll(posAfterH + offsetTop, radius);
            if (colTop.hit) posAfterH += colTop.normal * (colTop.penetration + 1e-3f);

            CollisionResult colBot = testSphereAll(posAfterH + offsetBottom, radius);
            if (colBot.hit) posAfterH += colBot.normal * (colBot.penetration + 1e-3f);
        }
    }

    // ── 2. MOUVEMENT VERTICAL ────────────────────────────────────────────────
    // On n'applique la chute automatique que si la gravité est activée pour le joueur
    float verticalMove = gravityEnabled ? (m_verticalVelocity * deltaTime) : 0.0f;
    float totalVertical = desiredMovement.y + verticalMove;
    glm::vec3 vertMove = glm::vec3(0.f, totalVertical, 0.f);
    glm::vec3 posAfterV = posAfterH;

    if (std::abs(totalVertical) > 1e-8f) {
        if (totalVertical < 0.f) {
            glm::vec3 newBottom = sweepSphere(posAfterH + offsetBottom, vertMove, radius);
            posAfterV = newBottom - offsetBottom;
        }
        else {
            glm::vec3 newTop = sweepSphere(posAfterH + offsetTop, vertMove, radius);
            posAfterV = newTop - offsetTop;
        }

        CollisionResult colMid = testSphereAll(posAfterV + offsetMiddle, radius);
        if (colMid.hit) {
            posAfterV += colMid.normal * (colMid.penetration + 1e-3f);
        }
    }

    // ── 3. DÉTECTION SOL / PLAFOND ───────────────────────────────────────────
    float actualVertical = posAfterV.y - posAfterH.y;

    if (gravityEnabled) {
        if (totalVertical < -1e-3f && std::abs(actualVertical) < std::abs(totalVertical) * 0.5f) {
            m_isGrounded = true;
            m_verticalVelocity = 0.f;
        }
        if (totalVertical > 1e-3f && std::abs(actualVertical) < std::abs(totalVertical) * 0.5f) {
            m_verticalVelocity = 0.f;
        }
    }
    else {
        m_isGrounded = false;
        m_verticalVelocity = 0.f; // On s'assure qu'aucune vélocité ne s'accumule en volant
    }

    return posAfterV;
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug
// ─────────────────────────────────────────────────────────────────────────────
void CollisionManager::printInfo() const {
    std::cout << "=== CollisionManager (AABB) ===" << std::endl;
    std::cout << "  Statiques  : " << m_staticBoxes.size() << " boîte(s)" << std::endl;
    for (const auto& sb : m_staticBoxes) {
        std::cout << "    [" << sb.name << "] "
            << "min(" << sb.aabb.min.x << "," << sb.aabb.min.y << "," << sb.aabb.min.z << ") "
            << "max(" << sb.aabb.max.x << "," << sb.aabb.max.y << "," << sb.aabb.max.z << ")"
            << std::endl;
    }
    std::cout << "  Dynamiques : " << m_dynamicBoxes.size() << " boîte(s)" << std::endl;
    for (const auto& [key, box] : m_dynamicBoxes) {
        std::cout << "    [" << key << "] "
            << "min(" << box.min.x << "," << box.min.y << "," << box.min.z << ") "
            << "max(" << box.max.x << "," << box.max.y << "," << box.max.z << ")"
            << std::endl;
    }
}