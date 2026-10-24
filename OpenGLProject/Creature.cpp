#include "Creature.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

#include <glm/gtc/matrix_transform.hpp>

#include "ModelEntity.h"
#include "Log.h"
#include "CollisionManager.h"
#include "constants/physics.h"

Creature::Creature(ModelEntity* head, const glm::vec3& spawnPosition)
    : m_head(head), m_position(spawnPosition) {
    syncHead();
}

void Creature::setPosition(const glm::vec3& position) {
    m_position = position;
    m_fallSpeed = 0.0f;
    syncHead();
}

// Cle de cache : case de grille (alignee monde) + hauteur au decimetre.
static long long cellKey(float x, float z, float y) {
    const long long ix = std::lround(x / Constants::Creature::CELL_SIZE) & 0x1FFFFF;
    const long long iz = std::lround(z / Constants::Creature::CELL_SIZE) & 0x1FFFFF;
    const long long iy = std::lround(y * 10.0f) & 0x1FFFFF;
    return (ix << 42) | (iz << 21) | iy;
}

// Sphere du corps remontee de kStepHeight, comme pour le deplacement.
bool Creature::isFree(const glm::vec3& p, const CollisionManager* cm) const {
    // Portes (seuls colliders dynamiques) ignorees : fropy les ouvre (cf. Scene, comme un joueur).
    const glm::vec3 c(p.x, p.y + kBodyRadius + kStepHeight, p.z);
    const long long key = cellKey(p.x, p.z, p.y);
    const auto it = m_staticFreeCache.find(key);
    if (it != m_staticFreeCache.end()) return it->second;
    const bool freeStatic = !cm->testSphereStatic(c, kBodyRadius).hit;
    m_staticFreeCache.emplace(key, freeStatic);
    return freeStatic;
}

float Creature::groundHeight(float x, float z, float top, float range, const CollisionManager* cm) const {
    auto hit = [&](float y) { return cm->testSphereStatic(glm::vec3(x, y + kFootRadius, z), kFootRadius).hit; };
    if (hit(top)) return std::numeric_limits<float>::infinity();
    // Descente par pas d'un diametre de sonde (deux sondes successives se touchent :
    // un plancher mince ne peut pas passer entre), puis dichotomie sur le dernier pas.
    float lo = 0.0f, hi = 0.0f;   // lo : libre, hi : contact
    for (;;) {
        hi = std::min(lo + 2.0f * kFootRadius, range);
        if (hit(top - hi)) break;
        if (hi >= range) return std::numeric_limits<float>::quiet_NaN();
        lo = hi;
    }
    while (hi - lo > 0.005f) {
        const float mid = 0.5f * (lo + hi);
        if (hit(top - mid)) hi = mid; else lo = mid;
    }
    return top - lo;
}

float Creature::stepGround(float x, float z, float y, const CollisionManager* cm) const {
    const float sx = (std::floor(x / kCellSize) + 0.5f) * kCellSize;
    const float sz = (std::floor(z / kCellSize) + 0.5f) * kCellSize;
    const long long key = cellKey(sx, sz, y);
    const auto it = m_stepGroundCache.find(key);
    if (it != m_stepGroundCache.end()) return it->second;
    // Toujours depuis le haut : le 1er contact est le dessus reel. Partir des pieds
    // trouvait un sol enfoui DANS un massif de marches (une sonde dans une boite
    // pleine ne touche aucune face) et fropy ne montait plus.
    float gy = groundHeight(sx, sz, y + kStepHeight, kStepHeight + kStepDown, cm);
    if (std::isinf(gy)) gy = std::numeric_limits<float>::quiet_NaN();   // marche trop haute / mur
    m_stepGroundCache.emplace(key, gy);
    return gy;
}

void Creature::followGround(glm::vec3& p, float deltaTime, const CollisionManager* cm) {
    const float gy = groundHeight(p.x, p.z, p.y + kStepHeight, kStepHeight + kStepDown, cm);   // depuis le haut (cf. stepGround)
    if (std::isinf(gy)) return;   // sonde dans un obstacle : on garde la hauteur
    if (!std::isnan(gy)) { p.y = gy; m_fallSpeed = 0.0f; return; }
    // Rien a portee d'une marche : chute (le sol est retrouve des qu'il passe sous kStepDown).
    m_fallSpeed -= Constants::Physics::GRAVITY * deltaTime;
    p.y -= std::min(m_fallSpeed * deltaTime, kStepDown);
}

// Segment praticable en suivant le sol (marches comprises) et arrivant au niveau de b.
bool Creature::lineClear(const glm::vec3& a, const glm::vec3& b, const CollisionManager* cm) const {
    const float len = glm::length(glm::vec2(b.x - a.x, b.z - a.z));
    const int n = std::max(1, (int)std::ceil(len / kCellSize));
    float y = a.y;
    for (int i = 1; i <= n; ++i) {
        glm::vec3 p = glm::mix(a, b, (float)i / n);
        p.y = stepGround(p.x, p.z, y, cm);
        if (std::isnan(p.y) || !isFree(p, cm)) return false;
        y = p.y;
    }
    return std::abs(y - b.y) < kSameFloorHeight;
}

bool Creature::findPath(const glm::vec3& goal, const CollisionManager* cm) {
    const glm::vec3 start = m_position;
    // Cible a un autre etage : zone et budget elargis pour trouver l'escalier.
    const bool otherFloor = std::abs(goal.y - start.y) > kSameFloorHeight;
    const float margin = otherFloor ? kPathMarginOtherFloor : kPathMargin;
    const int maxExpand = otherFloor ? kMaxExpandOtherFloor : kMaxExpand;
    // Grille alignee sur le monde : les centres de cases tombent sur les cles du cache.
    const float minX = std::floor((std::min(start.x, goal.x) - margin) / kCellSize) * kCellSize;
    const float minZ = std::floor((std::min(start.z, goal.z) - margin) / kCellSize) * kCellSize;
    const int W = std::min(400, (int)std::ceil((std::max(start.x, goal.x) + margin - minX) / kCellSize));
    const int H = std::min(400, (int)std::ceil((std::max(start.z, goal.z) + margin - minZ) / kCellSize));

    auto toCell = [&](const glm::vec3& p, int& cx, int& cz) {
        cx = std::clamp((int)((p.x - minX) / kCellSize), 0, W - 1);
        cz = std::clamp((int)((p.z - minZ) / kCellSize), 0, H - 1);
    };
    auto center = [&](int cx, int cz) {
        return glm::vec2(minX + (cx + 0.5f) * kCellSize, minZ + (cz + 0.5f) * kCellSize);
    };
    // Sol praticable de la case a une marche de y (NaN sinon : mur, vide, marche trop haute).
    const float kNaN = std::numeric_limits<float>::quiet_NaN();
    auto walkable = [&](int cx, int cz, float y) {
        if (cx < 0 || cz < 0 || cx >= W || cz >= H) return kNaN;
        const glm::vec2 c = center(cx, cz);
        const float gy = stepGround(c.x, c.y, y, cm);
        return (!std::isnan(gy) && isFree(glm::vec3(c.x, gy, c.y), cm)) ? gy : kNaN;
    };

    int sx, sz, gx, gz;
    toCell(start, sx, sz);
    toCell(goal, gx, gz);
    float goalY = goal.y;
    // Cible dans un mur (joueur colle) : cellule libre la plus proche (<= 1 m).
    if (std::isnan(walkable(gx, gz, goalY))) {
        int bestX = -1, bestZ = -1, bestD = 1 << 30;
        const int R = (int)(1.0f / kCellSize);
        for (int dz = -R; dz <= R; ++dz)
            for (int dx = -R; dx <= R; ++dx)
                if (dx * dx + dz * dz < bestD && !std::isnan(walkable(gx + dx, gz + dz, goalY))) {
                    bestD = dx * dx + dz * dz; bestX = gx + dx; bestZ = gz + dz;
                }
        if (bestX < 0) return false;
        gx = bestX; gz = bestZ;
        goalY = walkable(gx, gz, goalY);
    }

    // Noeud = (case, hauteur du sol) : une meme case a plusieurs etages (escalier en colimacon).
    struct Node { int cx, cz; float y; };
    std::vector<Node> nodes;
    std::vector<float> g;
    std::vector<int> parent;
    std::vector<char> closed;   // deja expanse : pas de reouverture (A* pondere, sinon re-expansions en boucle)
    std::unordered_map<long long, int> index;
    auto nodeId = [&](int cx, int cz, float y) {
        const long long key = (long long)cx | ((long long)cz << 12) | ((long long)(std::lround(y * 10.0f) + (1 << 20)) << 24);
        const auto ins = index.emplace(key, (int)nodes.size());
        if (ins.second) { nodes.push_back({ cx, cz, y }); g.push_back(1e30f); parent.push_back(-1); closed.push_back(0); }
        return ins.first->second;
    };

    using QE = std::pair<float, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> open;
    // A* pondere (x2.5) : beaucoup moins de cases explorees (chaque case = un test de collision), chemin
    // legerement sous-optimal mais lisse ensuite par visibilite. Le denivele compte aussi : un
    // escalier demande ~kClimbRun m de marche par m de montee (sinon l'etage entier est explore d'abord).
    auto heur = [&](const Node& n) {
        return 2.5f * (std::hypot((float)(n.cx - gx), (float)(n.cz - gz)) + kClimbRun * std::abs(n.y - goalY) / kCellSize);
    };

    const int startIdx = nodeId(sx, sz, start.y);
    g[startIdx] = 0.0f;
    open.push({ heur(nodes[startIdx]), startIdx });
    int goalIdx = -1;
    for (int expanded = 0; !open.empty() && expanded < maxExpand;) {
        const int cur = open.top().second;
        open.pop();
        if (closed[cur]) continue;   // entree perimee : ne compte pas dans le budget
        closed[cur] = 1;
        ++expanded;
        const Node n = nodes[cur];   // copie : nodes grandit pendant l'expansion
        if (n.cx == gx && n.cz == gz && std::abs(n.y - goalY) < kSameFloorHeight) { goalIdx = cur; break; }
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dz) continue;
                const float ny = walkable(n.cx + dx, n.cz + dz, n.y);
                if (std::isnan(ny)) continue;
                // Pas de coupe de coin en diagonale.
                if (dx && dz && (std::isnan(walkable(n.cx + dx, n.cz, n.y)) || std::isnan(walkable(n.cx, n.cz + dz, n.y)))) continue;
                const int ni = nodeId(n.cx + dx, n.cz + dz, ny);
                if (closed[ni]) continue;
                // Passer une porte coute le temps de l'ouvrir : un passage deja libre est prefere.
                const glm::vec2 nc = center(n.cx + dx, n.cz + dz);
                const bool door = cm->testSphereDynamic(glm::vec3(nc.x, ny + kBodyRadius + kStepHeight, nc.y), kBodyRadius).hit;
                const float ng = g[cur] + (dx && dz ? 1.4142f : 1.0f) + (door ? kDoorPathCost : 0.0f);
                if (ng < g[ni]) {
                    g[ni] = ng; parent[ni] = cur;
                    open.push({ ng + heur(nodes[ni]), ni });
                }
            }
        }
    }
    if (goalIdx < 0) return false;

    std::vector<glm::vec3> raw;
    for (int i = goalIdx; i != -1 && i != startIdx; i = parent[i]) {
        const glm::vec2 c = center(nodes[i].cx, nodes[i].cz);
        raw.push_back(glm::vec3(c.x, nodes[i].y, c.y));
    }
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
    move(deltaTime, playerPos, kSpeed, kStopDistance, collisionManager);
}

// Ligne de vue : echantillonne le segment a hauteur des yeux ; murs, portes
// fermees, troncs et feuillages la coupent.
bool Creature::hasLineOfSight(const glm::vec3& from, const glm::vec3& to, const CollisionManager* cm) const {
    if (!cm) return true;
    const glm::vec3 a(from.x, from.y + kEyeHeight, from.z);
    const glm::vec3 b(to.x, to.y + kEyeHeight, to.z);
    const float len = glm::length(b - a);
    const int n = static_cast<int>(len / kSightStep);
    for (int i = 1; i < n; ++i) {   // ni le point de depart ni la cible elle-meme
        if (cm->testSphereAll(glm::mix(a, b, static_cast<float>(i) / n), 0.1f).hit) return false;
    }
    return true;
}

int Creature::patrolCellIndex(const glm::vec3& p) const {
    const int cx = std::clamp(static_cast<int>((p.x - m_patrolMinX) / (m_patrolMaxX - m_patrolMinX) * kPatrolCellsX), 0, kPatrolCellsX - 1);
    const int cz = std::clamp(static_cast<int>((p.z - m_patrolMinZ) / (m_patrolMaxZ - m_patrolMinZ) * kPatrolCellsZ), 0, kPatrolCellsZ - 1);
    return cz * kPatrolCellsX + cx;
}

// Ligne de vue d'un bruit (test mis en cache : un bruit de voix est re-emis a chaque frame).
bool Creature::noiseHasLineOfSight(const glm::vec3& source, const CollisionManager* cm) {
    if (m_time - m_noiseOccTime < kNoiseLosCacheTime && glm::distance(source, m_noiseOccPos) < 1.5f) {
        return m_noiseOccResult;
    }
    m_noiseOccResult = hasLineOfSight(m_position, source, cm);
    m_noiseOccPos = source;
    m_noiseOccTime = m_time;
    return m_noiseOccResult;
}

// Prochain point de ronde : cellule la plus anciennement visitee (jamais visitee
// d'abord), penalisee par la distance pour eviter les allers-retours inutiles.
void Creature::pickPatrolWaypoint(const CollisionManager* cm) {
    m_hasPatrolGoal = false;
    const float cw = (m_patrolMaxX - m_patrolMinX) / kPatrolCellsX;
    const float chh = (m_patrolMaxZ - m_patrolMinZ) / kPatrolCellsZ;
    for (int attempt = 0; attempt < 6; ++attempt) {
        int best = -1;
        float bestKey = 1e30f;
        for (int i = 0; i < kPatrolCellsX * kPatrolCellsZ; ++i) {
            const glm::vec2 c(m_patrolMinX + (i % kPatrolCellsX + 0.5f) * cw, m_patrolMinZ + (i / kPatrolCellsX + 0.5f) * chh);
            const float visit = m_cellVisit[i] < 0.0f ? -1.0e6f : m_cellVisit[i];
            const float key = visit + glm::length(c - glm::vec2(m_position.x, m_position.z)) * kPatrolDistWeight;
            if (key < bestKey) { bestKey = key; best = i; }
        }
        // point libre au hasard dans la cellule (pas dans un mur / un arbre)
        const float x0 = m_patrolMinX + (best % kPatrolCellsX) * cw;
        const float z0 = m_patrolMinZ + (best / kPatrolCellsX) * chh;
        for (int tries = 0; tries < 25; ++tries) {
            glm::vec3 p(x0 + cw * (0.1f + 0.8f * rnd01()), m_position.y, z0 + chh * (0.1f + 0.8f * rnd01()));
            if (cm) {   // au meme etage que fropy
                p.y = stepGround(p.x, p.z, p.y, cm);
                if (std::isnan(p.y) || !isFree(p, cm)) continue;
            }
            m_patrolGoal = p;
            m_patrolCell = best;
            m_hasPatrolGoal = true;
            m_repathTimer = 0.0f;   // calcul du chemin immediat
            return;
        }
        m_cellVisit[best] = m_time;   // cellule sans point libre (batiment...) : consideree visitee
    }
}

void Creature::update(float deltaTime, const std::vector<glm::vec3>& targets,
                      float sightRange, CollisionManager* collisionManager) {
    update(deltaTime, targets, sightRange, std::vector<NoiseEvent>(), collisionManager);
}

void Creature::update(float deltaTime, const std::vector<glm::vec3>& targets,
                      float sightRange, const std::vector<NoiseEvent>& noises,
                      CollisionManager* collisionManager) {
    if (!(deltaTime > 0.0f)) deltaTime = 0.0f;
    m_time += deltaTime;
    m_steering = false;   // remis a true par move() s'il cherche a avancer
    sightRange *= m_senseMul;
    if (m_stunTimer > 0.0f) {             // etourdi : immobile
        m_stunTimer -= deltaTime;
        m_moving = false;
        syncHead();
        return;
    }
    m_cellVisit[patrolCellIndex(m_position)] = m_time;   // la cellule traversee compte comme visitee

    // ── Perception (toutes les kVisionInterval s) ───────────────────────────
    float nearestAny = 1e30f;   // joueur le plus proche, vu ou non (3D)
    m_visionTimer -= deltaTime;
    if (m_visionTimer <= 0.0f) {
        m_visionTimer = kVisionInterval;
        m_seesTarget = false;
        float bestVisible = 1e30f;
        for (const glm::vec3& t : targets) {
            const float d = glm::distance(m_position, t);
            if (d > sightRange || d >= bestVisible) continue;
            if (!hasLineOfSight(m_position, t, collisionManager)) continue;
            bestVisible = d;
            m_chaseTarget = t;   // derniere position vue
            m_seesTarget = true;
        }
    }
    for (const glm::vec3& t : targets) nearestAny = std::min(nearestAny, glm::distance(m_position, t));

    // ── Audition ────────────────────────────────────────────────────────────
    // Distances au carre (aucune racine) ; le test de ligne de vue, plus couteux,
    // n'est fait que pour un bruit entre le rayon etouffe (kHearOccludedFactor) et
    // son rayon plein : au-dela d'un mur ou d'un arbre, la portee est reduite.
    bool hears = false;
    glm::vec3 heardPos(0.0f);
    float bestD2 = 1e30f;
    for (const NoiseEvent& n : noises) {
        const glm::vec3 diff = n.position - m_position;
        const float d2 = glm::dot(diff, diff);
        const float radius = n.radius * m_senseMul;
        if (d2 > radius * radius || d2 >= bestD2) continue;
        const float rOcc = radius * kHearOccludedFactor;
        if (d2 > rOcc * rOcc && !noiseHasLineOfSight(n.position, collisionManager)) continue;
        bestD2 = d2;
        heardPos = n.position;
        hears = true;
    }

    // ── Etats ───────────────────────────────────────────────────────────────
    if (m_state != State::Chase && m_seesTarget) {
        m_state = State::Chase;           // vu : poursuite
        m_lostSightTimer = 0.0f;
        m_path.clear();
        m_repathTimer = 0.0f;
    } else if (m_state == State::Patrol) {
        if (hears) {                      // entendu : va voir d'ou vient le bruit
            m_state = State::Investigate;
            m_noiseTarget = heardPos;
            m_investigateTimer = 0.0f;
            m_hasPatrolGoal = false;
            m_path.clear();
            m_repathTimer = 0.0f;
        }
    } else if (m_state == State::Investigate) {
        if (hears) {                      // le bruit continue : cible mise a jour
            m_noiseTarget = heardPos;
            m_investigateTimer = 0.0f;
        } else {
            m_investigateTimer += deltaTime;
            if (m_investigateTimer > kInvestigateTime) {   // plus rien : retour aux rondes
                m_state = State::Patrol;
                m_hasPatrolGoal = false;
                m_patrolWait = 0.0f;
                m_path.clear();
            }
        }
    } else {
        // Chase : un bruit entretient l'agro (et corrige la derniere position connue)
        if (hears && !m_seesTarget) {
            m_chaseTarget = heardPos;
            m_lostSightTimer = 0.0f;
        } else {
            m_lostSightTimer = m_seesTarget ? 0.0f : m_lostSightTimer + deltaTime;
        }
        if (nearestAny > kDeaggroDistance || m_lostSightTimer > kLoseSightTime) {
            m_state = State::Patrol;       // agro perdue : retour aux rondes
            m_hasPatrolGoal = false;
            m_patrolWait = 0.0f;
            m_path.clear();
        }
    }

    // ── Action ──────────────────────────────────────────────────────────────
    if (m_state == State::Investigate) {
        move(deltaTime, m_noiseTarget, kSpeed * m_speedMul, kInvestigateStopDistance, collisionManager);
        return;
    }
    if (m_state == State::Chase) {
        // Vers la cible visible, sinon vers sa derniere position connue.
        move(deltaTime, m_chaseTarget, kSpeed * m_speedMul, kStopDistance, collisionManager);
        return;
    }

    // Ronde : points aleatoires (seed fixe), pause a chaque arrivee.
    m_moving = false;
    if (!m_hasPatrolGoal) {
        if (m_patrolWait > 0.0f) {
            m_patrolWait -= deltaTime;
            syncHead();
            return;
        }
        pickPatrolWaypoint(collisionManager);
        m_stuckRef = m_position;
        m_stuckClock = 0.0f;
        if (!m_hasPatrolGoal) { syncHead(); return; }
    }
    move(deltaTime, m_patrolGoal, kPatrolSpeed * m_speedMul, kPatrolStopDistance, collisionManager);

    const float toGoal = glm::length(glm::vec2(m_patrolGoal.x - m_position.x, m_patrolGoal.z - m_position.z));
    m_stuckClock += deltaTime;
    if (m_stuckClock >= kPatrolStuckTime) {   // bloquee (obstacle) : autre point
        if (glm::length(glm::vec2(m_position.x - m_stuckRef.x, m_position.z - m_stuckRef.z)) < 0.5f) {
            m_hasPatrolGoal = false;
            if (m_patrolCell >= 0) m_cellVisit[m_patrolCell] = m_time;   // inaccessible : on passe a une autre cellule
        }
        m_stuckRef = m_position;
        m_stuckClock = 0.0f;
    }
    if (toGoal <= kPatrolStopDistance) {      // arrivee : pause puis prochain point
        m_hasPatrolGoal = false;
        m_patrolWait = kPatrolWaitMin + (kPatrolWaitMax - kPatrolWaitMin) * rnd01();
    }
}

void Creature::move(float deltaTime, const glm::vec3& playerPos, float speed, float stopDist,
                    CollisionManager* collisionManager) {
    if (!(deltaTime > 0.0f)) deltaTime = 0.0f;

    glm::vec3 toPlayer = playerPos - m_position;
    const float dy = toPlayer.y;
    toPlayer.y = 0.0f;
    // Cible juste au-dessus / au-dessous (autre etage) : pas encore arrive.
    const float dist = glm::length(toPlayer) + (std::abs(dy) > kSameFloorHeight ? std::abs(dy) : 0.0f);
    m_moving = false;

    // Cible de marche : joueur en vue directe, sinon prochain waypoint A*
    // (contourne maisons et portes fermees). Sans collisionManager : joueur.
    glm::vec3 to = toPlayer;
    if (collisionManager && dist > stopDist) {
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
    m_steerTarget = m_position + to;   // trajectoire immediate (portes a ouvrir, cf. getSteerTarget)
    m_steering = stepDist > 0.001f && dist > stopDist;

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
        if (dist > stopDist) {
            glm::vec3 move = dir * (speed * deltaTime);
            if (collisionManager) {
                // Sphère remontée de kStepHeight : le sol, les marches et les obstacles
                // bas ne la bloquent plus, seuls les murs comptent (followGround monte).
                const glm::vec3 center = m_position + glm::vec3(0.0f, kBodyRadius + kStepHeight, 0.0f);
                move = collisionManager->sweepSphere(center, move, kBodyRadius) - center;
                move.y = 0.0f;
            }
            m_position += move;
            m_moveSpeed = deltaTime > 0.0f ? glm::length(move) / deltaTime : 0.0f;
            m_moving = glm::length(move) > 0.0001f;
        }
    }
    if (collisionManager) followGround(m_position, deltaTime, collisionManager);

    syncHead();
}

void Creature::syncHead() {
    if (!m_head) return;
    // Modele complet (corps articule + tete) : pieds a la position de fropy.
    m_head->setPosition(m_position);
    // Convention Direction (degrés) : getModelMatrix oriente le modèle via
    // atan2(dirVec.x, dirVec.z), et fropy regarde son +Z local (Mixamo) →
    // pour faire face à (fx,fz) il faut dirVec=(fx,fz), soit yaw=atan2(fz,fx).
    const double yawDeg = glm::degrees(std::atan2(m_faceDir.z, m_faceDir.x));
    m_head->setDirection(Direction(yawDeg, 0.0));
}