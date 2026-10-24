#pragma once

#include <random>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

#include "constants/creature.h"

// Evenement sonore : position + portee (m). Emis par la voix des joueurs ; plus tard
// par les quetes, etc. (cf. Scene::addNoise). La creature l'entend si elle est
// dans la portee, et va enqueter.
struct NoiseEvent {
    glm::vec3 position{ 0.0f };
    float radius = 0.0f;
};

class ModelEntity;
class CollisionManager;

// ─────────────────────────────────────────────────────────────────────────────
// Creature : ennemi qui POURSUIT le joueur.
//
// Aspect : modele articule (corps low poly + tete de fropy, squelette Mixamo :
// res/models/fropy/fropy_player.glb), anime par la Scene (repos / marche /
// course selon getMoveSpeed).
//
// IA : A* sur grille (XZ + hauteur du sol : prend les escaliers) evitant murs et
// portes fermees, recalcule toutes les kRepathInterval s ; ligne droite si degagee. Marche vers le joueur à kSpeed u/s (plus rapide que la marche,
// rattrapable en sprint), s'arrête à kStopDistance. Le déplacement passe par
// CollisionManager::sweepSphere → la créature glisse le long des murs au lieu
// de les traverser. La tête fait face au joueur (cap lissé).
// ─────────────────────────────────────────────────────────────────────────────
class Creature {
public:
    // model : modele articule de fropy (possede par la Scene, deja charge ; peut etre null).
    Creature(ModelEntity* model, const glm::vec3& spawnPosition);

    // Poursuite + cap + synchronisation de la tête. collisionManager peut
    // être null (déplacement sans collision, utile pour les tests).
    void update(float deltaTime, const glm::vec3& playerPos,
                CollisionManager* collisionManager);

    // IA complete : perception (portee + ligne de vue), poursuite, perte
    // d'agro (trop loin ou cible hors de vue trop longtemps) puis rondes sur la
    // carte. `targets` = positions de TOUS les joueurs ; `sightRange` = portee
    // de vue (brouillard). collisionManager null : pas de ligne de vue.
    void update(float deltaTime, const std::vector<glm::vec3>& targets,
                float sightRange, CollisionManager* collisionManager);

    // Idem + audition : `noises` = bruits emis CETTE frame. Un bruit a portee
    // attire la creature (enquete sur place, puis ronde) ; il entretient aussi
    // l'agro quand la cible est hors de vue.
    void update(float deltaTime, const std::vector<glm::vec3>& targets,
                float sightRange, const std::vector<NoiseEvent>& noises,
                CollisionManager* collisionManager);

    bool isChasing() const { return m_state == State::Chase; }
    // Fige la creature `seconds` s et lui fait perdre l'agro (apres avoir touche un joueur).
    void stun(float seconds) { m_stunTimer = seconds; m_state = State::Patrol; m_hasPatrolGoal = false; m_path.clear(); m_lostSightTimer = 0.0f; }
    // Difficulte : multiplicateurs de vitesse et de sens (vue + ouie).
    void setDifficulty(float speedMul, float senseMul) { m_speedMul = speedMul; m_senseMul = senseMul; }
    // Zone des rondes (x, z) : par defaut celle du village.
    void setPatrolBounds(const glm::vec2& min, const glm::vec2& max) {
        m_patrolMinX = min.x; m_patrolMinZ = min.y; m_patrolMaxX = max.x; m_patrolMaxZ = max.y;
        std::fill(m_cellVisit.begin(), m_cellVisit.end(), -1.0f);
        m_hasPatrolGoal = false;
    }
    bool isInvestigating() const { return m_state == State::Investigate; }

    // Téléporte la créature (spawn / respawn après auto-test).
    void setPosition(const glm::vec3& position);

    const glm::vec3& getPosition() const { return m_position; }
    float getSpeed() const { return kSpeed; }
    // Vitesse reelle de deplacement (m/s, 0 a l'arret / etourdi) : choix et cadence de l'animation.
    float getMoveSpeed() const { return m_moving ? m_moveSpeed : 0.0f; }
    // Point vise par le deplacement courant (prochain waypoint ou cible) et cap.
    // La Scene ouvre les portes fermees qui barrent [position, cible] (comme un joueur).
    const glm::vec3& getSteerTarget() const { return m_steerTarget; }
    // Cherche a avancer CETTE frame (meme bloque contre une porte fermee) : ni etourdi, ni en pause, ni arrive.
    bool isSteering() const { return m_steering; }
    const glm::vec3& getFaceDir() const { return m_faceDir; }

private:
    void syncHead();

    enum class State { Patrol, Chase, Investigate };

    // Deplacement (A*, rotation, sweepSphere) vers `goal` a `speed`, arret a stopDist.
    void move(float deltaTime, const glm::vec3& goal, float speed, float stopDist,
              CollisionManager* collisionManager);
    bool hasLineOfSight(const glm::vec3& from, const glm::vec3& to, const CollisionManager* cm) const;
    void pickPatrolWaypoint(const CollisionManager* cm);
    int  patrolCellIndex(const glm::vec3& p) const;
    bool noiseHasLineOfSight(const glm::vec3& source, const CollisionManager* cm);
    float rnd01() { return (m_rng() >> 8) / 16777216.0f; }

    // Chemin A* jusqu a goal (waypoints monde, lisses par visibilite).
    // false si aucun chemin (cible enfermee / trop loin) -> repli ligne droite.
    bool findPath(const glm::vec3& goal, const CollisionManager* cm);
    bool isFree(const glm::vec3& p, const CollisionManager* cm) const;
    bool lineClear(const glm::vec3& a, const glm::vec3& b, const CollisionManager* cm) const;
    // Hauteur du sol sous (x, z) cherchee de `top` a `top - range` (sphere etroite
    // qui descend) : NaN = vide, +inf = `top` deja dans un obstacle.
    float groundHeight(float x, float z, float top, float range, const CollisionManager* cm) const;
    // Sol a une marche pres de `y` (monte <= kStepHeight, descend <= kStepDown),
    // au centre de la case de grille (mis en cache : la carte ne bouge pas).
    float stepGround(float x, float z, float y, const CollisionManager* cm) const;
    // Pose `p` sur le sol (marches) ; sans sol a portee d'une marche : chute.
    void followGround(glm::vec3& p, float deltaTime, const CollisionManager* cm);

    // ── Réglages ──────────────────────────────────────────────────────────
    static constexpr float kSpeed = Constants::Creature::SPEED; // u/s (marche 2.5, sprint 5.5)
    static constexpr float kStopDistance = Constants::Creature::STOP_DISTANCE; // distance d'arrêt (« attaque »)

    // ── Agro / rondes ─────────────────────────────────────────────────────
    static constexpr float kDeaggroDistance = Constants::Creature::DEAGGRO_DISTANCE; // plus loin que ca du joueur le plus proche : agro perdue
    static constexpr float kLoseSightTime = Constants::Creature::LOSE_SIGHT_TIME; // s hors de vue avant de perdre l'agro
    static constexpr float kVisionInterval = Constants::Creature::VISION_INTERVAL; // s entre deux tests de vision (cout)
    static constexpr float kSightStep = Constants::Creature::SIGHT_STEP; // pas d'echantillonnage de la ligne de vue (m)
    static constexpr float kEyeHeight = Constants::Creature::EYE_HEIGHT; // hauteur des yeux pour la ligne de vue
    static constexpr float kInvestigateTime = Constants::Creature::INVESTIGATE_TIME; // s passees sur place apres le dernier bruit
    static constexpr float kInvestigateStopDistance = Constants::Creature::INVESTIGATE_STOP_DISTANCE;
    static constexpr float kPatrolSpeed = Constants::Creature::PATROL_SPEED; // u/s pendant les rondes
    static constexpr float kPatrolStopDistance = Constants::Creature::PATROL_STOP_DISTANCE; // arrivee au point de ronde
    static constexpr float kPatrolWaitMin = Constants::Creature::PATROL_WAIT_MIN; // pause a chaque point (s)
    static constexpr float kPatrolWaitMax = Constants::Creature::PATROL_WAIT_MAX;
    static constexpr float kPatrolStuckTime = Constants::Creature::PATROL_STUCK_TIME; // s sans avancer -> nouveau point
    // Ronde = couverture de TOUTE la carte : la zone (sol du village moins une marge)
    // est decoupee en kPatrolCellsX x kPatrolCellsZ cellules ; la creature note
    // l'instant de son dernier passage dans chacune et va toujours vers la plus
    // ancienne (jamais visitee d'abord, la plus proche en cas d'egalite).
    static constexpr float kPatrolMinX = Constants::Creature::PATROL_MIN_X, kPatrolMaxX = Constants::Creature::PATROL_MAX_X;
    static constexpr float kPatrolMinZ = Constants::Creature::PATROL_MIN_Z, kPatrolMaxZ = Constants::Creature::PATROL_MAX_Z;
    static constexpr int kPatrolCellsX = Constants::Creature::PATROL_CELLS_X, kPatrolCellsZ = Constants::Creature::PATROL_CELLS_Z;
    static constexpr float kPatrolDistWeight = Constants::Creature::PATROL_DIST_WEIGHT; // s de "fraicheur" echangees contre 1 m de trajet

    // ── Audition ──────────────────────────────────────────────────────────
    static constexpr float kHearOccludedFactor = Constants::Creature::HEAR_OCCLUDED_FACTOR; // portee du bruit sans ligne de vue (murs, arbres)
    static constexpr float kNoiseLosCacheTime = Constants::Creature::NOISE_LOS_CACHE_TIME; // s de validite du test de ligne de vue d'un bruit
    static constexpr float kBodyRadius = Constants::Creature::BODY_RADIUS; // rayon du sweepSphere
    static constexpr float kStepHeight = Constants::Creature::STEP_HEIGHT; // marche montee sans blocage
    static constexpr float kStepDown = Constants::Creature::STEP_DOWN;     // marche descendue (au-dela : chute)
    static constexpr float kFootRadius = Constants::Creature::FOOT_RADIUS; // sonde de sol
    static constexpr float kCellSize = Constants::Creature::CELL_SIZE; // pas de la grille A* (< passage d une porte)
    static constexpr float kPathMargin = Constants::Creature::PATH_MARGIN; // marge autour du segment creature-joueur
    static constexpr int kMaxExpand = Constants::Creature::MAX_EXPAND; // garde-fou cout A*
    static constexpr float kPathMarginOtherFloor = Constants::Creature::PATH_MARGIN_OTHER_FLOOR; // marge si cible a un autre etage
    static constexpr int kMaxExpandOtherFloor = Constants::Creature::MAX_EXPAND_OTHER_FLOOR;
    static constexpr float kSameFloorHeight = Constants::Creature::SAME_FLOOR_HEIGHT; // denivele au-dela duquel = autre etage
    static constexpr float kClimbRun = Constants::Creature::CLIMB_RUN; // heuristique A* : m de marche par m de denivele
    static constexpr float kDoorPathCost = Constants::Creature::DOOR_PATH_COST; // A* : surcout (cases) d'une porte a ouvrir
    static constexpr float kRepathInterval = Constants::Creature::REPATH_INTERVAL; // s (voit les portes ouvertes/fermees)
    static constexpr float kTurnRate = Constants::Creature::TURN_RATE; // lissage du cap (1/s)

    ModelEntity* m_head = nullptr;   // modele articule complet (nom historique : la tete seule autrefois)

    // Cache "libre ?" des colliders STATIQUES (cellule de kCellSize alignee monde) :
    // la carte ne bouge pas, seul le dynamique (portes) est reteste a chaque fois.
    mutable std::unordered_map<long long, bool> m_staticFreeCache;
    mutable std::unordered_map<long long, float> m_stepGroundCache;   // idem pour stepGround
    std::vector<glm::vec3> m_path;    // waypoints (m_pathIdx = courant)
    size_t m_pathIdx = 0;
    float  m_repathTimer = 0.0f;

    State     m_state = State::Patrol;
    float     m_visionTimer = 0.0f;
    bool      m_seesTarget = false;
    glm::vec3 m_chaseTarget{ 0.0f };   // cible visible ou derniere position vue
    float     m_lostSightTimer = 0.0f;
    glm::vec3 m_noiseTarget{ 0.0f };   // dernier bruit entendu
    float     m_investigateTimer = 0.0f;
    float     m_patrolMinX = kPatrolMinX, m_patrolMaxX = kPatrolMaxX;
    float     m_patrolMinZ = kPatrolMinZ, m_patrolMaxZ = kPatrolMaxZ;
    float     m_stunTimer = 0.0f;
    float     m_speedMul = 1.0f;
    float     m_senseMul = 1.0f;
    float     m_time = 0.0f;                 // temps ecoule (instants de visite)
    std::vector<float> m_cellVisit = std::vector<float>(kPatrolCellsX * kPatrolCellsZ, -1.0f);   // -1 = jamais
    int       m_patrolCell = -1;             // cellule visee par la ronde
    glm::vec3 m_noiseOccPos{ 0.0f };         // cache du test de ligne de vue d'un bruit
    bool      m_noiseOccResult = false;
    float     m_noiseOccTime = -1e9f;
    glm::vec3 m_patrolGoal{ 0.0f };
    bool      m_hasPatrolGoal = false;
    float     m_patrolWait = 0.0f;
    glm::vec3 m_stuckRef{ 0.0f };
    float     m_stuckClock = 0.0f;
    std::mt19937 m_rng{ 4242u };       // seed fixe : rondes reproductibles

    glm::vec3 m_position{ 0.0f };     // pieds (y = hauteur du sol)
    float     m_fallSpeed = 0.0f;     // vitesse de chute (m/s, > 0 vers le bas)
    glm::vec3 m_faceDir{ 0.0f, 0.0f, 1.0f };  // cap horizontal (normalisé)
    float     m_moveSpeed = 0.0f;     // vitesse du dernier deplacement (m/s)
    glm::vec3 m_steerTarget{ 0.0f };  // point vise par le dernier deplacement
    bool      m_steering = false;     // move() a voulu avancer cette frame
    bool      m_moving   = false;
};
