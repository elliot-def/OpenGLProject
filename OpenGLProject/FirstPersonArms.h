#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

class Model;
class Mesh;
class Shader;
class Camera;
class LightManager;
class TextureManager;
class Animator;

class FirstPersonArms {
public:
    FirstPersonArms(Camera* camera, LightManager* lightManager,
                    const std::string& modelPath, TextureManager* textureManager);
    ~FirstPersonArms();

    void draw(Shader* shader);
    void update(float deltaTime, const glm::vec3& playerPos, bool isSprinting);

    // Joue une fois l'animation de tir ("finger_gun_fire") puis revient a l'idle
    void triggerFire();

    // Joue une fois l'animation de push puis revient a l'idle
    void triggerPush();

    // Joue une fois l'animation de grab ("grab") puis revient a l'idle
    void triggerGrab();

    Model* getModel() { return m_model.get(); }
    const std::vector<Mesh*>& getMeshes();

private:
    // Diagnostique au premier draw : AABB CPU, matrices NaN/Inf, positions
    // des joints, test de frustum. N'affecte pas le rendu.
    void debugPrintFirstDraw(const glm::mat4& armModel,
                             const std::vector<Mesh*>& meshes);

    // Detecte les index des animations connues dans le rig (idle, fire,
    // push, grab, walk). Appele une fois dans le constructeur.
    void detectAnimations();

    // Applique les offsets de pose aux DEUX bras du viewmodel : elevation du
    // bras GAUCHE (le droit est deja leve par l'animation finger_gun_idle) +
    // abduction symetrique des epaules (L +Z / R -Z) pour ecarter les mains.
    // Appele par update() dans les deux modes (y compris pendant le tir).
    // Angles dans constants/firstPersonArms.h
    void applyViewmodelOffsets();

    // Index de l'animation de repos (hors tir) selon l'etat : "relax" (marche)
    // si le joueur bouge, finger_gun_idle sinon. -1 si aucune n'est trouvee.
    int restAnimIndex(bool isMoving) const {
        if (isMoving && m_walkAnimIndex >= 0) return m_walkAnimIndex;
        return m_idleAnimIndex;
    }

    Camera* m_camera = nullptr;
    LightManager* m_lightManager = nullptr;
    std::unique_ptr<Model> m_model;
    std::unique_ptr<Animator> m_animator;
    unsigned int m_fallbackTexture = 0;

    float m_animTime = 0.0f;
    int m_idleAnimIndex = -1;   // index de "finger_gun_idle" (joueur immobile)
    int m_walkAnimIndex = -1;   // index de "relax" (marche — en attendant une vraie anim)
    int m_fireAnimIndex = -1;   // index de "finger_gun_fire" (clic gauche)
    int m_pushAnimIndex = -1;   // index de push (main droite ou les deux)
    int m_grabAnimIndex = -1;   // index de grab (main gauche, grab.L)
    glm::vec3 m_playerPos{ 0.0f }; // position du joueur (attachement world-space 3P)

    bool m_viewmodelOffsetsActive = false; // offsets de pose communs 1P/3P appliques ?

    // ── Tilt de l'animation relax ───────────────────────────────────────
    // Rotation X (pitch vers le bas) appliquee sur le bone racine du
    // squelette UNIQUEMENT pendant l'animation "relax" (marche).
    static constexpr float kRelaxTiltDeg = 10.0f; // angle de tilt (degres)
    std::string m_spineBoneName;                   // nom du bone racine (id=0)
    bool m_relaxTiltActive = false;                // tilt actuellement applique ?

    // ── Idle vs marche (animations du fichier) ──────────────────────────────
    // L'Animator joue finger_gun_idle quand le joueur est immobile et "relax"
    // quand il bouge. La detection du mouvement se fait par la variation de
    // position du joueur entre 2 frames (m_wantsToMove n'est pas maintenu).
    glm::vec3 m_lastPlayerPos{ 0.0f }; // position joueur a la frame precedente
    bool      m_playerPosInitialized = false; // premiere frame : pas de delta

    // ── Debounce idle ↔ marche ────────────────────────────────────────────
    static constexpr float kAnimSwitchCooldown = 0.3f; // delai mini entre 2 switchs
    float m_animSwitchCooldown = 0.0f;                  // compteur restant (s)

    // ── Filtrage des bones visibles (bras, coudes, mains uniquement) ────
    // Construit au premier draw : set des IDs des bones dont le nom contient
    // "upper_arm", "forearm" ou "hand" (hors "handIK"). Tous les autres
    // bones (torse, epaules, doigts, IK, camera) sont collapses.
    // ── Filtrage des bones visibles (bras, coudes, mains uniquement) ────
    std::unordered_set<int> m_armBoneIds;
    bool m_armBonesBuilt = false;
};
