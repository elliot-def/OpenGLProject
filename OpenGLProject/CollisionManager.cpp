#define GLM_ENABLE_EXPERIMENTAL
#include "CollisionManager.h"
#include "Mesh.h"

#include "constants/physics.h"
#include "constants/player.h"

#include <algorithm>
#include <iostream>
#include <cmath>
#include <cfloat>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers internes
// ─────────────────────────────────────────────────────────────────────────────

// Calcule l'OBB world-space (centre, rotation, demi-étendues) d'un ensemble de
// meshes. Extraction : centre local de l'AABB transformé par la matrice, axes =
// colonnes normalisées de la matrice (rotation pure), demi-étendues = longueurs
// des colonnes × demi-étendues locales. Valable pour T·R·S classiques ; la
// rotation est portée par la matrice, l'échelle dans les longueurs de colonnes.
OBB CollisionManager::computeWorldOBB(const std::vector<Mesh*>& meshes,
    const glm::mat4& modelMatrix)
{
    OBB result;

    // AABB locale regroupée sur tous les sous-meshes
    glm::vec3 lMin(FLT_MAX);
    glm::vec3 lMax(-FLT_MAX);
    for (const auto* mesh : meshes) {
        lMin = glm::min(lMin, mesh->getLocalAABBMin());
        lMax = glm::max(lMax, mesh->getLocalAABBMax());
    }
    if (lMin.x > lMax.x || lMin.y > lMax.y || lMin.z > lMax.z) {
        return result; // AABB locale vide → OBB invalide (halfExtents = 0)
    }

    const glm::vec3 localCenter = (lMin + lMax) * 0.5f;
    const glm::vec3 localHalf   = (lMax - lMin) * 0.5f;

    // Centre world du box
    result.center = glm::vec3(modelMatrix * glm::vec4(localCenter, 1.0f));

    // Rotation pure + étendues : colonnes de la matrice (vecteurs d'axe)
    const glm::vec3 col0(modelMatrix[0]);
    const glm::vec3 col1(modelMatrix[1]);
    const glm::vec3 col2(modelMatrix[2]);
    const float s0 = glm::length(col0);
    const float s1 = glm::length(col1);
    const float s2 = glm::length(col2);
    if (s0 < 1e-6f || s1 < 1e-6f || s2 < 1e-6f) {
        return result; // matrice dégénérée → OBB invalide
    }

    result.rotation    = glm::mat3(col0 / s0, col1 / s1, col2 / s2);
    result.halfExtents = glm::vec3(s0 * localHalf.x, s1 * localHalf.y, s2 * localHalf.z);
    return result;
}

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

// Test sphère / OBB (boîte orientée : rotation prise en compte)
// On passe la sphère dans le repère local du box (Rᵀ·d), on fait un test
// AABB classique, puis on ramène la normale en world space (R·n). C'est ce
// qui rend la hitbox des objets rotatés (cube qui tourne) fidèle au visuel.
CollisionResult CollisionManager::testSphereOBB(glm::vec3   center,
    float       radius,
    const OBB&  box)
{
    CollisionResult result;

    // Centre de la sphère dans le repère local du box
    const glm::vec3 localCenter = box.toLocal(center - box.center);

    // Test AABB local (même logique que testSphereAABB mais sur ±halfExtents)
    const glm::vec3 closest = glm::clamp(localCenter, -box.halfExtents, box.halfExtents);
    const glm::vec3 diff = localCenter - closest;
    const float     dist2 = glm::length2(diff);
    if (dist2 > radius * radius) return result; // pas de collision

    result.hit = true;

    glm::vec3 localNormal;
    float     penetration = 0.0f;

    const bool insideBox = (closest == localCenter);
    if (insideBox) {
        // Centre à l'intérieur : normale = face locale la plus proche
        const float dx = box.halfExtents.x - std::abs(localCenter.x);
        const float dy = box.halfExtents.y - std::abs(localCenter.y);
        const float dz = box.halfExtents.z - std::abs(localCenter.z);

        if (dx <= dy && dx <= dz) {
            localNormal = glm::vec3(localCenter.x > 0.f ? 1.f : -1.f, 0.f, 0.f);
            penetration = radius + dx;
        }
        else if (dy <= dx && dy <= dz) {
            localNormal = glm::vec3(0.f, localCenter.y > 0.f ? 1.f : -1.f, 0.f);
            penetration = radius + dy;
        }
        else {
            localNormal = glm::vec3(0.f, 0.f, localCenter.z > 0.f ? 1.f : -1.f);
            penetration = radius + dz;
        }
    }
    else {
        // Centre à l'extérieur : normale = direction vers le point le plus proche
        const float dist = std::sqrt(dist2);
        localNormal = diff / dist;
        penetration = radius - dist;
    }

    // Normale en world space : suit l'orientation de la boîte (une face
    // inclinée renvoie une normale inclinée → le joueur peut s'y tenir).
    result.normal = box.rotation * localNormal;
    result.penetration = penetration;
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
    OBB  obb = computeWorldOBB(meshes, modelMatrix);
    if (!box.isValid() || !obb.isValid()) return;
    m_dynamicBoxes[key] = { box, obb };
}

void CollisionManager::updateDynamic(const std::string& key,
    const std::vector<Mesh*>& meshes,
    const glm::mat4& modelMatrix)
{
    AABB box = computeWorldAABB(meshes, modelMatrix);
    OBB  obb = computeWorldOBB(meshes, modelMatrix);
    if (!box.isValid() || !obb.isValid()) {
        removeDynamic(key);
        return;
    }
    m_dynamicBoxes[key] = { box, obb };
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

    // Closest-penetration accumulator partagé entre statiques et dynamiques.
    // Capture par référence : le lambda peut muter best.
    auto considerStatic = [&](const StaticBox& sb) {
        CollisionResult r = testSphereAABB(center, radius, sb.aabb);
        if (r.hit && r.penetration > best.penetration) best = r;
    };

    // 1) Statiques : on passe par le BVH (O(log n)) dès qu'il est construit.
    // Le BVH possède sa propre copie des StaticBox (déplacées par buildBVH) et
    // n'est pas perturbé par des ajouts/suppressions ultérieures dans
    // m_staticBoxes — ce qui est le cas attendu, buildBVH étant appelé une
    // seule fois à l'initialisation du niveau (cf. Game::initialize).
    if (m_useBVH) {
        m_bvh.querySphere(center, radius, considerStatic);
    }
    else {
        // Fallback : aucun niveau chargé encore (ou buildBVH jamais appelé).
        for (const auto& sb : m_staticBoxes) considerStatic(sb);
    }

    // 2) Dynamiques : scan linéaire. Les AABB bougent à chaque frame, donc
    // reconstruire un BVH par frame annulerait le gain — le coût de
    // m_dynamicBoxes reste borné par le nombre d'objets mobiles réellement
    // actifs (souvent < 10 dans ce projet).
    // Test OBB : les objets dynamiques peuvent être rotatés (cube qui tourne),
    // une AABB ne peut pas représenter leur hitbox fidèlement.
    for (const auto& [key, db] : m_dynamicBoxes) {
        CollisionResult r = testSphereOBB(center, radius, db.obb);
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

            // Sauvegarde la longueur maximale autorisée avant la projection
            float maxAllowedLength = glm::length(remainingMove);

            remainingMove = remainingMove - project * col.normal;

            // CORRECTION : On s'assure que le glissement ne fait PAS accélérer le joueur
            float newLength = glm::length(remainingMove);
            if (newLength > maxAllowedLength && newLength > 1e-6f) {
                remainingMove = (remainingMove / newLength) * maxAllowedLength;
            }

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
    
	float radius = Constants::Player::DEFAULT_PLAYER_RADIUS;
	float height = Constants::Player::DEFAULT_PLAYER_HEIGHT;
    
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

    m_isPlayerGrounded = false;

    // ── 0. APPLICATION DE LA GRAVITÉ ─────────────────────────────────────────

    if (gravityEnabled) {
        // Accélération de la vélocité verticale par la gravité
        m_verticalVelocity += Constants::Physics::GRAVITY * deltaTime;
    }
    else {
        m_isPlayerGrounded = false;
        m_verticalVelocity = 0.f; // Pas d'accumulation en vol libre
    }

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
    // Cumul du mouvement voulu (saut/vol) et de la vélocité gravitationnelle
    float verticalMove = m_verticalVelocity * deltaTime;
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

    // ── 3. DÉTECTION SOL / PLAFOND (sonde indépendante du mouvement) ────────
    // ── 3. DÉTECTION SOL / PLAFOND (sonde indépendante du mouvement) ────────
    float actualVertical = posAfterV.y - posAfterH.y;  // <-- à rajouter

    if (gravityEnabled) {
        const float groundProbeDistance = 0.05f;
        glm::vec3 groundProbePos = posAfterV + offsetBottom + glm::vec3(0.f, -groundProbeDistance, 0.f);
        CollisionResult groundCheck = testSphereAll(groundProbePos, radius);

        m_isPlayerGrounded = groundCheck.hit && groundCheck.normal.y > 0.5f;

        if (m_isPlayerGrounded && m_verticalVelocity < 0.f) {
            m_verticalVelocity = 0.f;
        }
        if (totalVertical > 1e-3f && std::abs(actualVertical) < std::abs(totalVertical) * 0.5f) {
            m_verticalVelocity = 0.f; // cognement au plafond
        }
    }
    else {
        m_isPlayerGrounded = false;
        m_verticalVelocity = 0.f;
    }

    return posAfterV;
}

void CollisionManager::buildBVH() {
    if (!m_staticBoxes.empty()) {
        m_bvh.build(m_staticBoxes);
        m_useBVH = true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Saut : impulsion verticale conditionnée à l'état au sol
// ─────────────────────────────────────────────────────────────────────────────
bool CollisionManager::tryJump(float jumpVelocity) {
    if (!m_isPlayerGrounded) return false;
    if (jumpVelocity <= 0.f) return false; // Garde : impulsion strictement positive

    // On coupe toute chute résiduelle puis on applique l'impulsion vers le haut.
    // La gravité (intégrée dans resolvePlayerMovement) décélérera puis
    // redessendra naturellement le joueur, donnant l'arc de saut recherché.
    m_verticalVelocity = jumpVelocity;
    // On marque le joueur comme non-grounded : évite un re-saut intra-frame
    // avant que la prochaine frame repasse dans resolvePlayerMovement.
    m_isPlayerGrounded = false;
    return true;
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
    for (const auto& [key, db] : m_dynamicBoxes) {
        std::cout << "    [" << key << "] "
            << "min(" << db.aabb.min.x << "," << db.aabb.min.y << "," << db.aabb.min.z << ") "
            << "max(" << db.aabb.max.x << "," << db.aabb.max.y << "," << db.aabb.max.z << ")"
            << std::endl;
    }
}