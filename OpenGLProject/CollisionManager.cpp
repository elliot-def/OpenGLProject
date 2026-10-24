#define GLM_ENABLE_EXPERIMENTAL
#include "Log.h"
#include "CollisionManager.h"

#include <map>
#include <tuple>
#include <cmath>
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
// Retourne la normale et la pénétration si collision detectee
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
        // Centre à l'intérieur de la boite : cherche la face la plus proche
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

// Test sphère / OBB (boite orientée : rotation prise en compte)
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

    // Normale en world space : suit l'orientation de la boite (une face
    // inclinée renvoie une normale inclinée → le joueur peut s'y tenir).
    result.normal = box.rotation * localNormal;
    result.penetration = penetration;
    return result;
}

// Point d'un triangle le plus proche d'un point P (Real-Time Collision
// Detection, Christer Ericson) — routine de base du test sphère/triangle.
static glm::vec3 closestPointOnTriangle(const glm::vec3& p, const glm::vec3& a,
                                        const glm::vec3& b, const glm::vec3& c)
{
    // Région du sommet A
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 ap = p - a;
    const float d1 = glm::dot(ab, ap);
    const float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    // Région du sommet B
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp);
    const float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    // Région de l'arête AB
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    // Région du sommet C
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp);
    const float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    // Région de l'arête AC
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    // Région de l'arête BC
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    // Région intérieure du triangle
    const float denom = 1.0f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    return a + ab * v + ac * w;
}

// Test sphère / triangle (collision précise "mesh")
// Normale de résultat = direction de dépénétration (vers l'extérieur du
// solide). Côté interne (tunneling) : on pousse toujours par la normale de
// face, pour ressortir du solide au lieu de s'y enfoncer.
CollisionResult CollisionManager::testSphereTriangle(glm::vec3 center, float radius,
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
    const glm::vec3& n)
{
    CollisionResult result;

    const glm::vec3 closest = closestPointOnTriangle(center, a, b, c);
    const glm::vec3 diff = center - closest;
    const float dist2 = glm::length2(diff);

    if (dist2 > radius * radius) return result;

    result.hit = true;

    if (dist2 > 1e-8f) {
        const float dist = std::sqrt(dist2);
        glm::vec3 normal = diff / dist;
        if (glm::dot(normal, n) < 0.0f) normal = n;   // côté interne
        result.normal = normal;
        result.penetration = radius - dist;
    }
    else {
        // Centre pile sur la face : normale de face.
        result.normal = n;
        result.penetration = radius;
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

void CollisionManager::addStaticTriangleMesh(const Mesh* mesh,
    const glm::mat4& modelMatrix,
    const std::string& name)
{
    StaticTriangleGroup group;
    group.name = name;

    const std::vector<Vertex> vertices = mesh->getVertices();
    const std::vector<unsigned int> indices = mesh->getIndices();
    group.triangles.reserve(indices.size() / 3);

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const glm::vec3 p0 = glm::vec3(modelMatrix * glm::vec4(vertices[indices[i]].getPositions(), 1.0f));
        const glm::vec3 p1 = glm::vec3(modelMatrix * glm::vec4(vertices[indices[i + 1]].getPositions(), 1.0f));
        const glm::vec3 p2 = glm::vec3(modelMatrix * glm::vec4(vertices[indices[i + 2]].getPositions(), 1.0f));

        glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
        const float len = glm::length(normal);
        if (len < 1e-8f) continue;   // triangle dégénéré
        normal /= len;

        group.triangles.push_back({ p0, p1, p2, normal });
        group.aabb.expand(p0);
        group.aabb.expand(p1);
        group.aabb.expand(p2);
    }

    if (group.triangles.empty() || !group.aabb.isValid()) return;
    m_staticTriangleGroups.push_back(std::move(group));
}

void CollisionManager::addStaticTriangleSoup(const std::vector<glm::vec3>& tris,
    const std::string& name, float cell)
{
    std::map<std::tuple<int, int, int>, StaticTriangleGroup> groups;
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        const glm::vec3& p0 = tris[i];
        const glm::vec3& p1 = tris[i + 1];
        const glm::vec3& p2 = tris[i + 2];
        glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
        const float len = glm::length(normal);
        if (len < 1e-8f) continue;   // triangle degenere
        normal /= len;

        const glm::vec3 c = (p0 + p1 + p2) / 3.0f;
        const auto key = std::make_tuple(static_cast<int>(std::floor(c.x / cell)),
                                         static_cast<int>(std::floor(c.y / cell)),
                                         static_cast<int>(std::floor(c.z / cell)));
        StaticTriangleGroup& g = groups[key];
        g.triangles.push_back({ p0, p1, p2, normal });
        g.aabb.expand(p0);
        g.aabb.expand(p1);
        g.aabb.expand(p2);
    }
    int n = 0;
    for (auto& kv : groups) {
        if (kv.second.triangles.empty() || !kv.second.aabb.isValid()) continue;
        kv.second.name = name + "#" + std::to_string(n++);
        m_staticTriangleGroups.push_back(std::move(kv.second));
    }
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
    m_staticTriangleGroups.clear();
    m_bvh.build({});
    m_useBVH = false;
    m_triGrid.clear();
    m_triGridBuilt = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// CollisionManager : test global
// ─────────────────────────────────────────────────────────────────────────────

// Segment contre AABB (methode des dalles).
static bool segmentHitsAABB(const glm::vec3& a, const glm::vec3& d, const AABB& box) {
    float t0 = 0.0f, t1 = 1.0f;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(d[i]) < 1e-9f) {
            if (a[i] < box.min[i] || a[i] > box.max[i]) return false;
        } else {
            float ta = (box.min[i] - a[i]) / d[i];
            float tb = (box.max[i] - a[i]) / d[i];
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) return false;
        }
    }
    return true;
}

// Segment contre triangle (Moller-Trumbore, double face) : t dans [0,1].
static bool segmentHitsTriangle(const glm::vec3& a, const glm::vec3& d, const StaticTriangle& tri) {
    const glm::vec3 e1 = tri.b - tri.a, e2 = tri.c - tri.a;
    const glm::vec3 p = glm::cross(d, e2);
    const float det = glm::dot(e1, p);
    if (std::abs(det) < 1e-9f) return false;
    const float inv = 1.0f / det;
    const glm::vec3 tv = a - tri.a;
    const float u = glm::dot(tv, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const glm::vec3 q = glm::cross(tv, e1);
    const float v = glm::dot(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float t = glm::dot(e2, q) * inv;
    return t >= 0.0f && t <= 1.0f;
}

bool CollisionManager::segmentBlocked(const glm::vec3& from, const glm::vec3& to) const {
    const glm::vec3 d = to - from;
    for (const auto& box : m_staticBoxes) {
        if (segmentHitsAABB(from, d, box.aabb)) return true;
    }
    bool hit = false;
    forEachTriangleGroup(glm::min(from, to), glm::max(from, to), [&](const StaticTriangleGroup& group) {
        if (hit || !segmentHitsAABB(from, d, group.aabb)) return;   // rejet rapide : AABB du groupe
        for (const auto& tri : group.triangles) {
            if (segmentHitsTriangle(from, d, tri)) { hit = true; return; }
        }
    });
    if (hit) return true;
    for (const auto& kv : m_dynamicBoxes) {
        if (segmentHitsAABB(from, d, kv.second.aabb)) return true;
    }
    return false;
}

// Parcourt les groupes de triangles dont la cellule de grille recoupe [mn, mx] (XZ),
// chacun une seule fois. Sans grille (pas encore de buildBVH) : tous les groupes.
template <typename Func>
void CollisionManager::forEachTriangleGroup(const glm::vec3& mn, const glm::vec3& mx, Func&& fn) const {
    if (!m_triGridBuilt) {
        for (const auto& g : m_staticTriangleGroups) fn(g);
        return;
    }
    if (++m_queryStamp == 0) {   // debordement : on remet les marques a zero
        std::fill(m_groupStamp.begin(), m_groupStamp.end(), 0u);
        m_queryStamp = 1;
    }
    const int x0 = static_cast<int>(std::floor(mn.x / kTriGridCell)), x1 = static_cast<int>(std::floor(mx.x / kTriGridCell));
    const int z0 = static_cast<int>(std::floor(mn.z / kTriGridCell)), z1 = static_cast<int>(std::floor(mx.z / kTriGridCell));
    for (int cx = x0; cx <= x1; ++cx) {
        for (int cz = z0; cz <= z1; ++cz) {
            const auto it = m_triGrid.find(triGridKey(cx, cz));
            if (it == m_triGrid.end()) continue;
            for (int gi : it->second) {
                if (m_groupStamp[gi] == m_queryStamp) continue;
                m_groupStamp[gi] = m_queryStamp;
                fn(m_staticTriangleGroups[gi]);
            }
        }
    }
}

CollisionResult CollisionManager::testSphereStatic(glm::vec3 center, float radius) const {
    CollisionResult best;
    auto considerStatic = [&](const StaticBox& sb) {
        CollisionResult r = testSphereAABB(center, radius, sb.aabb);
        if (r.hit && r.penetration > best.penetration) best = r;
    };
    // Boites : BVH (O(log n)) des qu'il est construit.
    if (m_useBVH) m_bvh.querySphere(center, radius, considerStatic);
    else for (const auto& sb : m_staticBoxes) considerStatic(sb);

    // Triangles precis : grille 2D puis rejet AABB par groupe.
    const glm::vec3 r3(radius);
    forEachTriangleGroup(center - r3, center + r3, [&](const StaticTriangleGroup& group) {
        if (!group.aabb.intersectsSphere(center, radius)) return;
        for (const auto& tri : group.triangles) {
            CollisionResult r = testSphereTriangle(center, radius, tri.a, tri.b, tri.c, tri.normal);
            if (r.hit && r.penetration > best.penetration) best = r;
        }
    });
    return best;
}

CollisionResult CollisionManager::testSphereDynamic(glm::vec3 center, float radius) const {
    // Scan lineaire : peu d'objets mobiles. Test OBB (objets tournes : portes).
    CollisionResult best;
    for (const auto& [key, db] : m_dynamicBoxes) {
        CollisionResult r = testSphereOBB(center, radius, db.obb);
        if (r.hit && r.penetration > best.penetration) best = r;
    }
    return best;
}

CollisionResult CollisionManager::testSphereAll(glm::vec3 center, float radius) const {
    CollisionResult best = testSphereStatic(center, radius);
    const CollisionResult dyn = testSphereDynamic(center, radius);
    if (dyn.hit && dyn.penetration > best.penetration) best = dyn;
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

            // Dépénétration stricte : on recule la sphère à la surface du cube
            // + un petit epsilon anti-collage. `target` = pos + remainingMove
            // contient déjà TOUT le mouvement de ce substep : la composante
            // normale (qui rentre dans le mur) est annulée par la dépénétration,
            // et la composante tangentielle (le glissement le long du mur) est
            // donc déjà appliquée ici.
            //
            // FIX (bug d'accélération) : l'ancien code recalculait la composante
            // tangentielle dans `remainingMove` puis la ré-appliquait à
            // l'itération suivante → le glissement était compté DEUX FOIS et le
            // joueur accélérait en longeant un mur. On consomme donc tout le
            // mouvement restant (break) : les coins sont de toute façon résolus
            // par les passes de dépénétration de resolvePlayerMovement juste
            // après cet appel.
            pos = target + col.normal * (col.penetration + 1e-3f);
            break;
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
        logErr() << "[Collision] Position NaN/Inf detectee !" << std::endl;
        return currentPos;
    }

    glm::vec3 offsetBottom = glm::vec3(0.f, radius, 0.f);
    glm::vec3 offsetMiddle = glm::vec3(0.f, height * 0.5f, 0.f);
    glm::vec3 offsetTop = glm::vec3(0.f, height - radius, 0.f);

    const bool wasGrounded = m_isPlayerGrounded;
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

    const float stepH = Constants::Player::PLAYER_STEP_HEIGHT;

    // Deplacement horizontal depuis `from` : balayage + depenetration.
    auto moveHorizontal = [&](glm::vec3 from) {
        glm::vec3 p = sweepSphere(from + offsetBottom, horizontalMovement, radius) - offsetBottom;
        for (int pass = 0; pass < 3; ++pass) {
            CollisionResult colMid = testSphereAll(p + offsetMiddle, radius);
            if (colMid.hit) p += colMid.normal * (colMid.penetration + 1e-3f);

            CollisionResult colTop = testSphereAll(p + offsetTop, radius);
            if (colTop.hit) p += colTop.normal * (colTop.penetration + 1e-3f);

            CollisionResult colBot = testSphereAll(p + offsetBottom, radius);
            if (colBot.hit) p += colBot.normal * (colBot.penetration + 1e-3f);
        }
        return p;
    };
    // Sol marchable sous `p` (a moins de `dist`) ? testSphereAll ne renvoie que le
    // contact le plus profond : collee a une contremarche, la sphere pleine voit la
    // contremarche (normale horizontale) et masque le sol -> sonde etroite en plus.
    // La sphere pleine reste necessaire pour un pied pose sur le bord d'une marche.
    // Sphere pleine abaissee de 1 cm seulement : plus bas, la normale sur une arete
    // de marche s'incline sous le seuil et le pied perche n'est plus "au sol".
    auto groundBelow = [&](glm::vec3 p, float dist) {
        CollisionResult c = testSphereAll(p + offsetBottom - glm::vec3(0.f, std::min(dist, 0.01f), 0.f), radius);
        if (c.hit && c.normal.y > 0.5f) return true;
        const float r2 = radius * 0.5f;
        c = testSphereAll(p + glm::vec3(0.f, r2 - dist, 0.f), r2);
        return c.hit && c.normal.y > 0.5f;
    };

    // Descend `p` verticalement jusqu'au premier contact (au plus `maxDrop`).
    // Contrairement a sweepSphere, ne glisse pas le long de l'arete arrondie
    // d'une marche : sinon le joueur retombait au pied de la marche.
    auto dropToContact = [&](glm::vec3 p, float maxDrop) {
        if (!testSphereAll(p + offsetBottom - glm::vec3(0.f, maxDrop, 0.f), radius).hit) return p - glm::vec3(0.f, maxDrop, 0.f);
        float lo = 0.f, hi = maxDrop;   // lo : libre, hi : contact
        for (int i = 0; i < 10; ++i) {
            const float mid = 0.5f * (lo + hi);
            if (testSphereAll(p + offsetBottom - glm::vec3(0.f, mid, 0.f), radius).hit) hi = mid; else lo = mid;
        }
        return p - glm::vec3(0.f, lo, 0.f);
    };

    const bool walking = wasGrounded && m_verticalVelocity <= 0.f && desiredMovement.y <= 1e-6f;
    bool stepped = false;

    if (glm::length2(horizontalMovement) > 1e-8f) {
        posAfterH = moveHorizontal(currentPos);

        // Montee automatique des marches : on tente aussi le deplacement depuis
        // une position surelevee de stepH, puis on repose le joueur au sol. On
        // garde cette version seulement si elle finit plus haut (sur plat ou
        // contre un mur elle revient au meme niveau et est ignoree).
        if (walking) {
            const glm::vec3 raised = currentPos + glm::vec3(0.f, stepH, 0.f);
            if (!testSphereAll(raised + offsetTop, radius).hit && !testSphereAll(raised + offsetMiddle, radius).hit) {
                const glm::vec3 up = moveHorizontal(raised);
                const float maxDrop = stepH + 0.05f;
                const glm::vec3 p = dropToContact(up, maxDrop);
                // Appui trouve (pas forcement "au sol" : a haut FPS on avance peu par
                // frame et le pied n'est d'abord que perche sur l'arete de la marche,
                // avec une normale trop inclinee pour groundBelow -> montee qui calait).
                const bool support = up.y - p.y < maxDrop - 1e-4f;
                if (support && p.y > posAfterH.y + 1e-3f
                    && !testSphereAll(p + offsetMiddle, radius).hit && !testSphereAll(p + offsetTop, radius).hit) {
                    posAfterH = p;
                    m_verticalVelocity = 0.f;
                    stepped = true;
                }
            }
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

    // Descente de marches : on reste colle au sol plutot que de tomber marche par marche.
    if (walking && !stepped && !groundBelow(posAfterV, 0.05f)) {
        glm::vec3 p = sweepSphere(posAfterV + offsetBottom, glm::vec3(0.f, -stepH, 0.f), radius) - offsetBottom;
        if (groundBelow(p, 0.05f)) {
            posAfterV = p;
            m_verticalVelocity = 0.f;
        }
    }

    // ── 3. DÉTECTION SOL / PLAFOND (sonde indépendante du mouvement) ────────
    // ── 3. DÉTECTION SOL / PLAFOND (sonde indépendante du mouvement) ────────
    float actualVertical = posAfterV.y - posAfterH.y;  // <-- à rajouter

    if (gravityEnabled) {
        // Perche sur une marche qu'on vient de monter : compte comme au sol pour
        // que la montee continue a la frame suivante.
        m_isPlayerGrounded = stepped || groundBelow(posAfterV, 0.05f);

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
    // Grille 2D des groupes de triangles (chaque groupe dans toutes les cellules
    // que recouvre son AABB).
    m_triGrid.clear();
    for (int gi = 0; gi < static_cast<int>(m_staticTriangleGroups.size()); ++gi) {
        const AABB& b = m_staticTriangleGroups[gi].aabb;
        const int x0 = static_cast<int>(std::floor(b.min.x / kTriGridCell)), x1 = static_cast<int>(std::floor(b.max.x / kTriGridCell));
        const int z0 = static_cast<int>(std::floor(b.min.z / kTriGridCell)), z1 = static_cast<int>(std::floor(b.max.z / kTriGridCell));
        for (int cx = x0; cx <= x1; ++cx)
            for (int cz = z0; cz <= z1; ++cz)
                m_triGrid[triGridKey(cx, cz)].push_back(gi);
    }
    m_groupStamp.assign(m_staticTriangleGroups.size(), 0u);
    m_queryStamp = 0;
    m_triGridBuilt = true;
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
    logOut() << "=== CollisionManager (AABB) ===" << std::endl;
    logOut() << "  Statiques  : " << m_staticBoxes.size() << " boite(s), "
             << m_staticTriangleGroups.size() << " groupe(s) de triangles" << std::endl;
    for (const auto& sb : m_staticBoxes) {
        logOut() << "    [" << sb.name << "] "
            << "min(" << sb.aabb.min.x << "," << sb.aabb.min.y << "," << sb.aabb.min.z << ") "
            << "max(" << sb.aabb.max.x << "," << sb.aabb.max.y << "," << sb.aabb.max.z << ")"
            << std::endl;
    }
    logOut() << "  Dynamiques : " << m_dynamicBoxes.size() << " boite(s)" << std::endl;
    for (const auto& [key, db] : m_dynamicBoxes) {
        logOut() << "    [" << key << "] "
            << "min(" << db.aabb.min.x << "," << db.aabb.min.y << "," << db.aabb.min.z << ") "
            << "max(" << db.aabb.max.x << "," << db.aabb.max.y << "," << db.aabb.max.z << ")"
            << std::endl;
    }
}