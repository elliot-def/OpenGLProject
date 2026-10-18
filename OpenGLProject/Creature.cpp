#include "Creature.h"

#include <algorithm>
#include <cmath>
#include <queue>

#include <glm/gtc/matrix_transform.hpp>

#include "ModelEntity.h"
#include "CollisionManager.h"
#include "CubeRenderer.h"
#include "Shader.h"

// Couleur du corps (émissive via le shader "cube/lightsource") : silhouette
// quasi noire, la tête de fropy est la seule partie colorée.
static const glm::vec3 kBodyColor(0.02f, 0.02f, 0.03f);

Creature::Creature(ModelEntity* head, const glm::vec3& spawnPosition)
    : m_head(head), m_position(spawnPosition) {
    m_position.y = 0.0f;
    syncHead();
}

void Creature::setPosition(const glm::vec3& position) {
    m_position = position;
    m_position.y = 0.0f;
    syncHead();
}

// Sphere du corps remontee de kStepHeight, comme pour le deplacement.
bool Creature::isFree(const glm::vec3& p, const CollisionManager* cm) const {
    return !cm->testSphereAll(glm::vec3(p.x, kBodyRadius + kStepHeight, p.z), kBodyRadius).hit;
}

bool Creature::lineClear(const glm::vec3& a, const glm::vec3& b, const CollisionManager* cm) const {
    const float len = glm::length(glm::vec2(b.x - a.x, b.z - a.z));
    const int n = std::max(1, (int)std::ceil(len / kCellSize));
    for (int i = 1; i <= n; ++i) {
        if (!isFree(glm::mix(a, b, (float)i / n), cm)) return false;
    }
    return true;
}

bool Creature::findPath(const glm::vec3& goal, const CollisionManager* cm) {
    const glm::vec3 start = m_position;
    const float minX = std::min(start.x, goal.x) - kPathMargin;
    const float minZ = std::min(start.z, goal.z) - kPathMargin;
    const int W = std::min(400, (int)std::ceil((std::max(start.x, goal.x) + kPathMargin - minX) / kCellSize));
    const int H = std::min(400, (int)std::ceil((std::max(start.z, goal.z) + kPathMargin - minZ) / kCellSize));

    auto toCell = [&](const glm::vec3& p, int& cx, int& cz) {
        cx = std::clamp((int)((p.x - minX) / kCellSize), 0, W - 1);
        cz = std::clamp((int)((p.z - minZ) / kCellSize), 0, H - 1);
    };
    auto toWorld = [&](int cx, int cz) {
        return glm::vec3(minX + (cx + 0.5f) * kCellSize, 0.0f, minZ + (cz + 0.5f) * kCellSize);
    };
    std::vector<uint8_t> walk((size_t)W * H, 2);   // 2 = non teste, 1 = libre, 0 = bloque
    auto free = [&](int cx, int cz) {
        if (cx < 0 || cz < 0 || cx >= W || cz >= H) return false;
        uint8_t& w = walk[(size_t)cz * W + cx];
        if (w == 2) w = isFree(toWorld(cx, cz), cm) ? 1 : 0;
        return w == 1;
    };

    int sx, sz, gx, gz;
    toCell(start, sx, sz);
    toCell(goal, gx, gz);
    // Cible dans un mur (joueur colle) : cellule libre la plus proche (<= 1 m).
    if (!free(gx, gz)) {
        int best = -1; int bestD = 1 << 30;
        const int R = (int)(1.0f / kCellSize);
        for (int dz = -R; dz <= R; ++dz)
            for (int dx = -R; dx <= R; ++dx)
                if (dx * dx + dz * dz < bestD && free(gx + dx, gz + dz)) {
                    bestD = dx * dx + dz * dz; best = (gz + dz) * W + gx + dx;
                }
        if (best < 0) return false;
        gx = best % W; gz = best / W;
    }

    const size_t N = (size_t)W * H;
    std::vector<float> g(N, 1e30f);
    std::vector<int>   parent(N, -1);
    using QE = std::pair<float, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> open;
    auto heur = [&](int cx, int cz) { return std::hypot((float)(cx - gx), (float)(cz - gz)); };

    const int startIdx = sz * W + sx;
    const int goalIdx = gz * W + gx;
    g[startIdx] = 0.0f;
    open.push({ heur(sx, sz), startIdx });
    bool found = false;
    for (int expanded = 0; !open.empty() && expanded < kMaxExpand; ++expanded) {
        const int cur = open.top().second;
        const float curF = open.top().first;
        open.pop();
        const int cx = cur % W, cz = cur / W;
        if (curF > g[cur] + heur(cx, cz) + 1e-3f) continue;   // entree perimee
        if (cur == goalIdx) { found = true; break; }
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dz) continue;
                const int nx = cx + dx, nz = cz + dz;
                if (!free(nx, nz)) continue;
                // Pas de coupe de coin en diagonale.
                if (dx && dz && (!free(cx + dx, cz) || !free(cx, cz + dz))) continue;
                const int ni = nz * W + nx;
                const float ng = g[cur] + (dx && dz ? 1.4142f : 1.0f);
                if (ng < g[ni]) {
                    g[ni] = ng; parent[ni] = cur;
                    open.push({ ng + heur(nx, nz), ni });
                }
            }
        }
    }
    if (!found) return false;

    std::vector<glm::vec3> raw;
    for (int i = goalIdx; i != -1 && i != startIdx; i = parent[i]) raw.push_back(toWorld(i % W, i / W));
    std::reverse(raw.begin(), raw.end());
    if (raw.empty()) return false;

    // Lissage : de chaque point, saute au waypoint le plus lointain en vue directe.
    m_path.clear();
    glm::vec3 from = start;
    for (size_t i = 0; i < raw.size();) {
        size_t j = raw.size() - 1;
        while (j > i && !lineClear(from, raw[j], cm)) --j;
        m_path.push_back(raw[j]);
        from = raw[j];
        i = j + 1;
    }
    m_pathIdx = 0;
    return true;
}

void Creature::update(float deltaTime, const glm::vec3& playerPos,
                      CollisionManager* collisionManager) {
    if (!(deltaTime > 0.0f)) deltaTime = 0.0f;

    glm::vec3 toPlayer = playerPos - m_position;
    toPlayer.y = 0.0f;
    const float dist = glm::length(toPlayer);
    m_moving = false;

    // Cible de marche : joueur en vue directe, sinon prochain waypoint A*
    // (contourne maisons et portes fermees). Sans collisionManager : joueur.
    glm::vec3 to = toPlayer;
    if (collisionManager && dist > kStopDistance) {
        m_repathTimer -= deltaTime;
        if (m_repathTimer <= 0.0f) {
            m_repathTimer = kRepathInterval;
            if (lineClear(m_position, playerPos, collisionManager) ||
                !findPath(playerPos, collisionManager)) {
                m_path.assign(1, playerPos);   // degage, ou repli (pas de chemin)
                m_pathIdx = 0;
            }
        }
        while (m_pathIdx + 1 < m_path.size() &&
               glm::length(glm::vec2(m_path[m_pathIdx].x - m_position.x,
                                     m_path[m_pathIdx].z - m_position.z)) < 0.3f) {
            ++m_pathIdx;
        }
        if (m_pathIdx < m_path.size()) {
            to = m_path[m_pathIdx] - m_position;
            to.y = 0.0f;
        }
    }
    const float stepDist = glm::length(to);

    if (stepDist > 0.001f) {
        const glm::vec3 dir = to / stepDist;

        // Cap : lissage angulaire (chemin le plus court) vers le joueur.
        const float targetAngle = std::atan2(dir.x, dir.z);
        const float curAngle    = std::atan2(m_faceDir.x, m_faceDir.z);
        float delta = targetAngle - curAngle;
        while (delta > 3.14159265f)  delta -= 6.2831853f;
        while (delta < -3.14159265f) delta += 6.2831853f;
        const float blend = 1.0f - std::exp(-deltaTime * kTurnRate);
        const float angle = curAngle + delta * blend;
        m_faceDir = glm::vec3(std::sin(angle), 0.0f, std::cos(angle));

        // Poursuite : s'arrête à kStopDistance. Le sweepSphere résout les
        // collisions contre la carte (glissement le long des murs).
        if (dist > kStopDistance) {
            glm::vec3 move = dir * (kSpeed * deltaTime);
            if (collisionManager) {
                // Sphère remontée de kStepHeight : le sol et les obstacles bas
                // (tapis, seuils) ne la bloquent plus, seuls les murs comptent.
                const glm::vec3 center = m_position + glm::vec3(0.0f, kBodyRadius + kStepHeight, 0.0f);
                move = collisionManager->sweepSphere(center, move, kBodyRadius) - center;
                move.y = 0.0f;
            }
            m_position += move;
            m_position.y = 0.0f;
            m_walkTime += deltaTime;
            m_moving = glm::length(move) > 0.0001f;
        }
    }

    syncHead();
}

void Creature::syncHead() {
    if (!m_head) return;
    m_head->setPosition(m_position + glm::vec3(0.0f, kHeadBaseY, 0.0f));
    m_head->setScale(kHeadScale);
    // Convention Direction (degrés) : getModelMatrix oriente le modèle via
    // atan2(dirVec.x, dirVec.z), et la face de fropy regarde son +Z local →
    // pour faire face à (fx,fz) il faut dirVec=(fx,fz), soit yaw=atan2(fz,fx).
    const double yawDeg = glm::degrees(std::atan2(m_faceDir.z, m_faceDir.x));
    m_head->setDirection(Direction(yawDeg, 0.0));
}

void Creature::submitBody(CubeRenderer* cubeRenderer, Shader* bodyShader) const {
    if (!cubeRenderer || !bodyShader) return;

    const float bodyYaw = std::atan2(m_faceDir.x, m_faceDir.z);
    const glm::mat4 base = glm::translate(glm::mat4(1.0f), m_position)
                         * glm::rotate(glm::mat4(1.0f), bodyYaw, glm::vec3(0.0f, 1.0f, 0.0f));

    const float swing = m_moving
        ? std::sin(m_walkTime * kWalkCycleHz * 6.2831853f) * kSwingDeg
        : 0.0f;

    struct Part { glm::vec3 pos; glm::vec3 size; float swingDeg; };
    const Part parts[] = {
        { {  0.00f, 1.02f, 0.00f }, { 0.42f, 0.80f, 0.26f },  0.0f },   // torse
        { {  0.00f, 1.47f, 0.00f }, { 0.16f, 0.14f, 0.16f },  0.0f },   // cou
        { { -0.14f, 0.36f, 0.00f }, { 0.15f, 0.72f, 0.20f },  swing },  // jambe G
        { {  0.14f, 0.36f, 0.00f }, { 0.15f, 0.72f, 0.20f }, -swing },  // jambe D
        { { -0.32f, 1.06f, 0.02f }, { 0.12f, 0.62f, 0.15f }, -swing },  // bras G
        { {  0.32f, 1.06f, 0.02f }, { 0.12f, 0.62f, 0.15f },  swing },  // bras D
    };

    for (const Part& p : parts) {
        const glm::mat4 model = base
            * glm::translate(glm::mat4(1.0f), p.pos)
            * glm::rotate(glm::mat4(1.0f), glm::radians(p.swingDeg), glm::vec3(1.0f, 0.0f, 0.0f))
            * glm::scale(glm::mat4(1.0f), p.size);
        cubeRenderer->submit(bodyShader, {}, model, kBodyColor);
    }
}
