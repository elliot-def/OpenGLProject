#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

class ModelEntity;
class InputManager;
class TextRenderer;

// ─────────────────────────────────────────────────────────────────────────────
// CharacterAnimationController
//
// Machine a etats d'animation du personnage 3P (Remy) :
//  - idle / walk / run (forward)
//  - left/right strafe (lateral)
//  - left/right turn (one-shot)
//  - jump / running jump (one-shot)
//  - punch (one-shot, jab sur R)
//  - Affichage du HUD debug
// ─────────────────────────────────────────────────────────────────────────────

class CharacterAnimationController {
public:
    CharacterAnimationController(ModelEntity* entity, InputManager* input);

    // Appele chaque frame dans Game::update().
    // isGrounded : true si le joueur touche le sol (pour arreter le saut).
    void update(const glm::vec3& playerPos, float dt, bool isSprinting, bool isGrounded);

    // Dessine le HUD de debug listant les animations du modele (3P seulement).
    static void drawDebugHUD(ModelEntity* entity,
                             const std::vector<std::unique_ptr<TextRenderer>>& renderers);

private:
    ModelEntity* m_entity;
    InputManager* m_input;

    // ── Etat ──
    glm::vec3 m_lastPos{0.0f};
    float m_lastYaw = 0.0f;
    bool m_yawInit = false;
    int m_lastAnimIdx = -1;
    float m_restTimer = 0.0f;

    // One-shots en cours
    bool m_punching = false;
    bool m_jumping = false;
    bool m_turning = false;

    bool m_prevRDown = false;

    // ── Hysterese anti-clignotement (collision contre un mur) ───────────
    // Quand le joueur pousse contre un mur, la vitesse horizontale oscille
    // pres de zero, ce qui faisait clignoter idle/walk. On utilise un latch :
    // on demarre le mouvement a un seuil haut, on l'arrete a un seuil bas.
    bool m_isMoving = false;
    bool m_isStrafing = false;
    float m_animChangeTimer = 0.0f;  // cooldown entre changements d'anim

    static constexpr float kRestToIdleDelay = 2.5f;
    static constexpr float kTurnYawThreshold = 30.0f; // degres pour declencher turn
    static constexpr float kJumpThreshold = 0.005f;    // delta.y min pour detecter un saut
    static constexpr float kMoveStartSpeed = 0.25f;    // seuil haut pour commencer a bouger
    static constexpr float kMoveStopSpeed  = 0.06f;    // seuil bas pour arreter de bouger
    static constexpr float kAnimChangeCooldown = 0.15f; // delai min entre 2 changements
};
