#include "CharacterAnimationController.h"
#include "ModelEntity.h"
#include "Animator.h"
#include "InputManager.h"
#include "Key.h"
#include "TextRenderer.h"
#include "Direction.h"
#include <glad/glad.h>
#include <cmath>

CharacterAnimationController::CharacterAnimationController(ModelEntity* entity, InputManager* input)
    : m_entity(entity), m_input(input) {
    // m_lastYaw sera initialise au premier update (evite un faux turn)
}

void CharacterAnimationController::update(const glm::vec3& playerPos, float dt, bool isSprinting, bool isGrounded) {
    if (!m_entity || !m_entity->hasAnimations()) return;

    // ── Detection du mouvement (avec hysterese anti-clignotement) ────────
    const glm::vec3 delta = playerPos - m_lastPos;
    const glm::vec2 horizDelta(delta.x, delta.z);
    const float horizSpeed = glm::length(horizDelta) / std::max(dt, 1e-5f);
    const bool isJumpingUp = delta.y > kJumpThreshold;
    m_lastPos = playerPos;

    // Hysterese : on ne change d'etat moving/not-moving qu'avec des
    // seuils decales pour eviter l'oscillation quand on pousse un mur.
    if (horizSpeed > kMoveStartSpeed)
        m_isMoving = true;
    else if (horizSpeed < kMoveStopSpeed)
        m_isMoving = false;

    // Direction du joueur (pour decomposer forward/lateral)
    Direction* dir = m_entity->getDirection();
    const float yaw = dir ? static_cast<float>(dir->getYaw()) : 0.0f;

    // Premier update : initialiser le yaw sans declencher de turn
    if (!m_yawInit) { m_lastYaw = yaw; m_yawInit = true; }
    const float yawDelta = yaw - m_lastYaw;

    // Detection de turn (changement brusque de yaw, pas accumule)
    const bool isTurning = std::abs(yawDelta) > kTurnYawThreshold;
    m_lastYaw = yaw;  // toujours mettre a jour pour ne pas accumuler

    // Decomposition du mouvement horizontal en forward / lateral
    float forwardSpeed = 0.0f, lateralSpeed = 0.0f;
    bool movingRight = false;
    if (m_isMoving && horizSpeed > 0.1f) {
        const glm::vec2 moveDir = glm::normalize(horizDelta);
        const float yawRad = static_cast<float>(glm::radians(yaw));
        const glm::vec2 forwardDir(std::cos(yawRad), std::sin(yawRad));
        const glm::vec2 rightDir(-forwardDir.y, forwardDir.x); // +90 deg
        forwardSpeed = glm::dot(moveDir, forwardDir) * horizSpeed;
        lateralSpeed = glm::dot(moveDir, rightDir) * horizSpeed;
        movingRight = lateralSpeed > 0.1f;
    }

    // Hysterese strafe : idem, seuils decales
    const float absLat = std::abs(lateralSpeed);
    if (absLat > 0.25f && absLat > std::abs(forwardSpeed) * 0.7f)
        m_isStrafing = true;
    else if (absLat < 0.08f || absLat < std::abs(forwardSpeed) * 0.4f)
        m_isStrafing = false;

    // ── Indices ──────────────────────────────────────────────────────────
    const int idleIdx      = m_entity->getIdleAnimIndex();
    const int walkIdx      = m_entity->getWalkAnimIndex();
    const int runIdx       = m_entity->getRunAnimIndex();
    const int punchIdx     = m_entity->getPunchAnimIndex();
    const int restIdx      = m_entity->getRestAnimIndex();
    const int strafeLIdx   = m_entity->getStrafeLeftIdx();
    const int strafeRIdx   = m_entity->getStrafeRightIdx();
    const int strafeWLIdx  = m_entity->getStrafeWalkLeftIdx();
    const int strafeWRIdx  = m_entity->getStrafeWalkRightIdx();
    const int turnLIdx     = m_entity->getTurnLeftIdx();
    const int turnRIdx     = m_entity->getTurnRightIdx();
    const int jumpIdx      = m_entity->getJumpIdx();
    const int runJumpIdx   = m_entity->getRunJumpIdx();
    const int walkBackIdx  = m_entity->getWalkBackIdx();

    // ── One-shot: punch (R) ────────────────────────────────────────────
    const bool rDown = m_input->getKey("Push")->getStatus();
    if (rDown && !m_prevRDown && !m_punching && !m_jumping && !m_turning && punchIdx >= 0) {
        m_entity->playAnimation(punchIdx, false);
        m_punching = true;
        m_lastAnimIdx = punchIdx;
    }
    m_prevRDown = rDown;
    if (m_punching && m_entity->getAnimator()->isFinished()) {
        m_punching = false;
    }

    // ── One-shot: jump ─────────────────────────────────────────────────
    if (isJumpingUp && !m_jumping && !m_punching && !m_turning) {
        const int jmp = (isSprinting && runJumpIdx >= 0) ? runJumpIdx : jumpIdx;
        if (jmp >= 0) {
            m_entity->playAnimation(jmp, false);
            m_jumping = true;
            m_lastAnimIdx = jmp;
        }
    }
    // Fin du saut : animation terminee OU joueur a atterri (grounded).
    // On laisse l'animation jouer en boucle tant que le joueur est en l'air
    // (fin de la chute). Si l'animation se termine avant l'atterrissage,
    // on la relance pour eviter un trou visuel.
    if (m_jumping) {
        if (isGrounded || m_entity->getAnimator()->isFinished()) {
            // Si l'animation est finie mais pas encore au sol : relance
            if (!isGrounded && m_entity->getAnimator()->isFinished()) {
                const int jmp = (isSprinting && runJumpIdx >= 0) ? runJumpIdx : jumpIdx;
                if (jmp >= 0) m_entity->playAnimation(jmp, false);
            } else {
                m_jumping = false;
            }
        }
    }

    // ── One-shot: turn ─────────────────────────────────────────────────
    if (isTurning && !m_turning && !m_jumping && !m_punching) {
        const int turn = (yawDelta < 0.0f ? turnLIdx : turnRIdx);
        if (turn >= 0) {
            m_entity->playAnimation(turn, false);
            m_turning = true;
            m_lastAnimIdx = turn;
        }
    }
    if (m_turning && m_entity->getAnimator()->isFinished()) {
        m_turning = false;
    }

    // ── Boucle continue (hors one-shot) ─────────────────────────────────
    if (!m_punching && !m_jumping && !m_turning) {
        int targetIdx = -1;

        if (m_isStrafing) {
            m_restTimer = 0.0f;
            if (isSprinting) {
                targetIdx = movingRight ? strafeRIdx : strafeLIdx;
            } else {
                targetIdx = movingRight ? strafeWRIdx : strafeWLIdx;
            }
        } else if (m_isMoving) {
            m_restTimer = 0.0f;
            if (forwardSpeed < -0.15f && walkBackIdx >= 0) {
                targetIdx = walkBackIdx;
            } else if (isSprinting && runIdx >= 0) {
                targetIdx = runIdx;
            } else {
                targetIdx = walkIdx;
            }
        } else {
            // Immobile : rest -> idle
            if (restIdx >= 0 && m_restTimer < kRestToIdleDelay) {
                targetIdx = restIdx;
                m_restTimer += dt;
            } else {
                targetIdx = idleIdx;
            }
        }

        // Cooldown anti-clignotement : on ne change d'animation que si
        // le delai minimum est ecoule depuis le dernier changement.
        // Evite les oscillations idle/walk quand on pousse contre un mur.
        m_animChangeTimer = std::max(0.0f, m_animChangeTimer - dt);
        if (targetIdx >= 0 && targetIdx != m_lastAnimIdx && m_animChangeTimer <= 0.0f) {
            m_entity->playAnimation(targetIdx, true);
            m_lastAnimIdx = targetIdx;
            m_animChangeTimer = kAnimChangeCooldown;
        }
    } else {
        // Reset du cooldown quand on sort d'un one-shot
        m_animChangeTimer = 0.0f;
    }

    m_entity->updateAnimation(dt);
}

// static
void CharacterAnimationController::drawDebugHUD(ModelEntity* entity,
                                                const std::vector<std::unique_ptr<TextRenderer>>& renderers) {
    if (!entity || !entity->hasAnimations() || renderers.empty()) return;

    Model* model = entity->getModel();
    const Animator* animator = entity->getAnimator();
    const size_t numAnims = model->getNumAnimations();
    const int idleIdx  = entity->getIdleAnimIndex();
    const int walkIdx  = entity->getWalkAnimIndex();
    const int runIdx   = entity->getRunAnimIndex();
    const int punchIdx = entity->getPunchAnimIndex();
    const int restIdx  = entity->getRestAnimIndex();
    const int strafeL  = entity->getStrafeLeftIdx();
    const int strafeR  = entity->getStrafeRightIdx();
    const int strafeWL = entity->getStrafeWalkLeftIdx();
    const int strafeWR = entity->getStrafeWalkRightIdx();
    const int turnL    = entity->getTurnLeftIdx();
    const int turnR    = entity->getTurnRightIdx();
    const int jumpI    = entity->getJumpIdx();
    const int runJumpI = entity->getRunJumpIdx();
    const int walkBackI = entity->getWalkBackIdx();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    TextRenderer* hud = renderers[0].get();
    const float scale = 0.28f;
    const float lineH = 30.0f;
    float y = 20.0f;

    hud->renderText("Animations (vert = en cours)",
                    20.0f, y, scale, 0.85f, 0.85f, 0.85f);
    y += lineH;

    constexpr size_t MAX_HUD_ANIMS = 32;

    for (size_t i = 0; i < numAnims && i < MAX_HUD_ANIMS; i++) {
        const aiAnimation* anim = model->getAnimation(i);
        if (!anim) continue;
        const std::string name(anim->mName.C_Str());
        std::string line = "[" + std::to_string(i) + "] " + name;
        int idx = static_cast<int>(i);
        if (idx == idleIdx)   line += "  IDLE";
        if (idx == walkIdx)   line += "  WALK";
        if (idx == runIdx)    line += "  RUN";
        if (idx == punchIdx)  line += "  PUNCH";
        if (idx == restIdx)   line += "  REST";
        if (idx == strafeL)   line += "  STRAFE_L";
        if (idx == strafeR)   line += "  STRAFE_R";
        if (idx == strafeWL)  line += "  STRAFE_W_L";
        if (idx == strafeWR)  line += "  STRAFE_W_R";
        if (idx == turnL)     line += "  TURN_L";
        if (idx == turnR)     line += "  TURN_R";
        if (idx == jumpI)     line += "  JUMP";
        if (idx == runJumpI)  line += "  SPRINT";
        if (idx == walkBackI) line += "  WALK_BACK";

        const bool isCurrent = animator && (anim == animator->getCurrentAnimation());
        hud->renderText(line, 20.0f, y, scale,
            isCurrent ? 0.35f : 0.85f,
            isCurrent ? 0.95f : 0.85f,
            isCurrent ? 0.45f : 0.85f);
        y += lineH;
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
