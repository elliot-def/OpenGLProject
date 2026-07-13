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
        // (une rotation peut intervertir min/max, donc on teste tous les coins)
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
// sweepSphere : déplace une sphère avec sliding itératif
// ─────────────────────────────────────────────────────────────────────────────

glm::vec3 CollisionManager::sweepSphere(glm::vec3 start,
    glm::vec3 movement,
    float     radius,
    int       maxIterations) const
{
    glm::vec3 pos = start;

    for (int iter = 0; iter < maxIterations; ++iter) {
        if (glm::length2(movement) < 1e-8f) break;

        glm::vec3 target = pos + movement;
        if (std::isnan(target.x) || std::isinf(target.x)) {
            std::cerr << "[Collision] target NaN/Inf, pos=" << pos.x << "," << pos.y << "," << pos.z << std::endl;
            break;
        }

        CollisionResult col = testSphereAll(target, radius);
        if (!col.hit) {
            pos = target;
            break;
        }

        // Dépénètre
        target += col.normal * (col.penetration + 1e-3f);
        

        // Sliding : retire la composante normale du mouvement restant
        movement = movement - glm::dot(movement, col.normal) * col.normal;

        pos = target;
    }

    return pos;
}

// ─────────────────────────────────────────────────────────────────────────────
// resolvePlayerMovement : capsule joueur = sphère basse + sphère haute
// ─────────────────────────────────────────────────────────────────────────────

glm::vec3 CollisionManager::resolvePlayerMovement(glm::vec3 currentPos,
    glm::vec3 desiredMovement,
    float     deltaTime,
    float     radius,
    float     height)
{

    if (std::isnan(currentPos.x) || std::isinf(currentPos.x)) {
        std::cerr << "[Collision] Position NaN/Inf détectée !" << std::endl;
        return currentPos;
    }

    // Centres des deux sphères de la capsule
    auto sphereBottom = [&](glm::vec3 pos) { return pos + glm::vec3(0.f, radius, 0.f); };
    auto sphereTop = [&](glm::vec3 pos) { return pos + glm::vec3(0.f, height - radius, 0.f); };

    m_isGrounded = false;

    // Gravité
    //m_verticalVelocity += GRAVITY * deltaTime;
    float verticalMove = m_verticalVelocity * deltaTime;

    // ── Mouvement horizontal ──────────────────────────────────────────────────
    glm::vec3 horizontalMovement = glm::vec3(desiredMovement.x, 0.f, desiredMovement.z);
    glm::vec3 posAfterH = currentPos;

    if (glm::length2(horizontalMovement) > 1e-8f) {
        // Sphère basse
        glm::vec3 newBottom = sweepSphere(sphereBottom(currentPos), horizontalMovement, radius);
        posAfterH = newBottom - glm::vec3(0.f, radius, 0.f);

        // Sphère haute (tête) — prend le résultat le plus conservateur
        glm::vec3 newTop = sweepSphere(sphereTop(posAfterH), horizontalMovement, radius);
        glm::vec3 posFromTop = newTop - glm::vec3(0.f, height - radius, 0.f);

        if (glm::length2(posFromTop - currentPos) < glm::length2(posAfterH - currentPos)) {
            posAfterH.x = posFromTop.x;
            posAfterH.z = posFromTop.z;
        }
    }

    // ── Mouvement vertical ────────────────────────────────────────────────────
    float     totalVertical = desiredMovement.y + verticalMove;
    glm::vec3 vertMove = glm::vec3(0.f, totalVertical, 0.f);
    glm::vec3 posAfterV = posAfterH;

    // Sphère basse
    glm::vec3 newBottom = sweepSphere(sphereBottom(posAfterH), vertMove, radius);
    posAfterV = newBottom - glm::vec3(0.f, radius, 0.f);

    // Sphère haute — prend le résultat le plus haut (le plus restrictif vers le bas)
    glm::vec3 newTop = sweepSphere(sphereTop(posAfterV), vertMove, radius);
    glm::vec3 posFromTop = newTop - glm::vec3(0.f, height - radius, 0.f);
    posAfterV.y = std::min(posAfterV.y, posFromTop.y);

    // Détection sol / plafond
    float actualVertical = posAfterV.y - posAfterH.y;

    if (totalVertical < -1e-3f && std::abs(actualVertical) < std::abs(totalVertical) * 0.5f) {
        m_isGrounded = true;
        m_verticalVelocity = 0.f;
    }
    if (totalVertical > 1e-3f && std::abs(actualVertical) < std::abs(totalVertical) * 0.5f) {
        m_verticalVelocity = 0.f;
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