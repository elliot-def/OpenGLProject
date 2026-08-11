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

void CharacterAnimationController::startPostLandSlowdown(float speedFactor, float duration) {
    m_postLandSpeedFactor = 1.0f;  // debut du fondu a partir de 1.0
    m_postLandTargetFactor = speedFactor;  // facteur cible (ex: 0.35 pour chute)
    m_postLandDuration = duration;         // duree totale du ralenti
    m_postLandTimer = duration;            // timer decomptant
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

    // Hysterese strafe : le strafe ne se joue que pour un deplacement (quasi)
    // purement lateral. Des qu'on avance OU recule en meme temps (diagonale),
    // on joue la marche avant/arriere a la place du strafe : la composante
    // avant est alors >= kStrafeMaxForwardSpeed, le meme seuil que le walkback.
    const float absLat = std::abs(lateralSpeed);
    const float absFwd = std::abs(forwardSpeed);
    if (absLat > 0.25f && absLat > absFwd * 0.7f && absFwd < kStrafeMaxForwardSpeed)
        m_isStrafing = true;
    else if (absLat < 0.08f || absLat < absFwd * 0.4f || absFwd >= kStrafeMaxForwardSpeed)
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
    if (rDown && !m_prevRDown && !m_punching && !m_jumping && !m_turning
        && !m_falling && !m_landing && punchIdx >= 0) {
        m_entity->playAnimation(punchIdx, false);
        m_punching = true;
        m_lastAnimIdx = punchIdx;
    }
    m_prevRDown = rDown;
    if (m_punching && m_entity->getAnimator()->isFinished()) {
        m_punching = false;
    }

    // ── Detection de la chute (descente libre) ──────────────────────────
    // Le joueur est en chute quand il descend suffisamment vite (vitesse
    // verticale < FALL_VELOCITY_THRESHOLD) ET qu'il n'est pas au sol ET
    // qu'il n'est pas deja en train de sauter (m_jumping couvre la phase
    // montante). On divise delta.y par dt pour obtenir la vitesse reelle.
    const float verticalVelocity = (dt > 1e-5f) ? (delta.y / dt) : 0.0f;
    const bool isFallingDown = verticalVelocity < Constants::Player::FALL_VELOCITY_THRESHOLD
                             && !isGrounded && !m_jumping;

    // Memo du sprint au sol : utilise pour choisir running jump / jump (le
    // sprint est coupe en l'air par Player::getIsSprinting).
    if (isGrounded) m_wasSprintingWhenGrounded = isSprinting;

    // ── One-shot: jump ─────────────────────────────────────────────────
    // Running jump uniquement si on sprintait ET qu'on se deplacait reellement
    // (m_isMoving) : debout + Shift tenu + saut → jump simple.
    // PAS de jump si en cours de landing (l'animation doit se terminer).
    if (isJumpingUp && !m_jumping && !m_punching && !m_turning && !m_landing && !m_falling) {
        const int jmp = (m_wasSprintingWhenGrounded && m_isMoving && runJumpIdx >= 0) ? runJumpIdx : jumpIdx;
        if (jmp >= 0) {
            m_entity->playAnimation(jmp, false);
            m_jumping = true;
            m_falling = false;
            m_lastAnimIdx = jmp;
        }
    }
    // Fin du saut : animation terminee OU joueur a atterri (grounded).
    // Si le joueur atterrit, on enclenche l'animation "Falling To Landing"
    // (one-shot) avant de revenir aux animations normales.
    // Si l'animation est finie mais pas encore au sol (descente), on
    // bascule sur "Falling Idle" pour la phase de chute libre.
    if (m_jumping) {
        if (isGrounded || m_entity->getAnimator()->isFinished()) {
            if (isGrounded) {
                // Atterrissage direct depuis le saut : pas de ralenti.
                m_jumping = false;
            } else {
                // Animation de saut finie, toujours en l'air → transition
                // vers "Falling Idle" si le joueur descend.
                if (verticalVelocity < 0.0f) {
                    m_jumping = false;
                    const int fallIdle = m_entity->getFallingIdleIdx();
                    if (fallIdle >= 0) {
                        m_entity->playAnimation(fallIdle, true);
                        m_falling = true;
                        m_fallStartY = playerPos.y;  // debut de la phase chute
                        m_lastAnimIdx = fallIdle;
                    }
                } else {
                    // Encore en montée → relancer l'animation de saut
                    const int jmp = (m_wasSprintingWhenGrounded && m_isMoving && runJumpIdx >= 0) ? runJumpIdx : jumpIdx;
                    if (jmp >= 0) m_entity->playAnimation(jmp, false);
                }
            }
        }
    }

    // ── One-shot: falling (descente libre) ──────────────────────────────
    // Declenche quand le joueur descend assez vite ET n'est pas au sol
    // ET n'est pas deja en train de sauter (la phase montante est geree
    // par m_jumping). Joue "Falling Idle" en boucle.
    if (isFallingDown && !m_jumping && !m_falling && !m_landing
        && !m_punching && !m_turning) {
        const int fallIdle = m_entity->getFallingIdleIdx();
        if (fallIdle >= 0) {
            m_entity->playAnimation(fallIdle, true);
            m_falling = true;
            m_fallStartY = playerPos.y;  // memoriser la hauteur de debut de chute
            m_lastAnimIdx = fallIdle;
        }
    }
    // Fin de chute : le joueur touche le sol → jouer "Falling To Landing"
    // (one-shot, non-loop) SEULEMENT si la hauteur de chute depasse le seuil.
    // Le ralenti post-atterrissage est active.
    // L'animation doit jouer JUSQU'AU BOUT sans etre interrompue.
    if (m_falling && isGrounded) {
        m_falling = false;
        const float fallHeight = m_fallStartY - playerPos.y;
        const bool hasFallenEnough = fallHeight >= Constants::Player::FALL_HEIGHT_LANDING_THRESHOLD;
        if (hasFallenEnough) {
            const int landAnim = m_entity->getFallingToLandingIdx();
            if (landAnim >= 0) {
                m_entity->playAnimation(landAnim, false);
                m_landing = true;
                m_lastAnimIdx = landAnim;
                m_landingAnimTimer = Constants::Player::LANDING_ANIM_MIN_DURATION;
                // Ralenti post-chute : utiliser FALL_SLOWDOWN_FACTOR et FALL_SLOWDOWN_DURATION
                startPostLandSlowdown(Constants::Player::FALL_SLOWDOWN_FACTOR,
                                      Constants::Player::FALL_SLOWDOWN_DURATION);
            }
        }
        // Note : pas de ralenti pour les chutes courtes (inferieures au seuil).
    }
    // Fin de l'animation de landing : relancer idle/walk/run
    // L'animation doit jouer JUSQU'AU BOUT : on attend isFinished()
    // ET que le timer minimum soit ecoule.
    if (m_landing) {
        // Decrementer le timer
        m_landingAnimTimer -= dt;

        const Animator* anim = m_entity->getAnimator();
        const bool stillLanding = anim && anim->getCurrentAnimation()
            && m_lastAnimIdx == m_entity->getFallingToLandingIdx();
        // Sortir de l'etat landing SEULEMENT si :
        // 1. L'animation est terminee (isFinished)
        // 2. Le timer minimum est ecoule
        // 3. ET aucun autre playAnime() n'a pris le relais
        if (!stillLanding) {
            m_landing = false;
            m_landingAnimTimer = 0.0f;
        } else if (anim->isFinished() && m_landingAnimTimer <= 0.0f) {
            m_landing = false;
            m_landingAnimTimer = 0.0f;
        }
    }

    // ── One-shot: turn ─────────────────────────────────────────────────
    // PAS de turn si en cours de landing ou de falling.
    if (isTurning && !m_turning && !m_jumping && !m_punching && !m_landing && !m_falling) {
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
    // Offset de cap cible : en marche avant (walk/run, dont les diagonales
    // Z+Q / Z+D), le modele pivote vers le sens reel du deplacement pour que
    // l'animation de marche soit alignee avec la trajectoire. Strafe (lateral
    // pur), recul et idle gardent le modele face a la camera (offset 0).
    float targetYawOffsetDeg = 0.0f;
    if (!m_punching && !m_jumping && !m_turning && !m_falling && !m_landing) {
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
            if (forwardSpeed < -kStrafeMaxForwardSpeed && walkBackIdx >= 0) {
                targetIdx = walkBackIdx;
            } else if (isSprinting && runIdx >= 0) {
                targetIdx = runIdx;
            } else {
                targetIdx = walkIdx;
            }
            // Marche avant OU recul (y compris diagonales) : on oriente le
            // modele pour aligner l'animation avec la trajectoire. thetaDeg =
            // angle du deplacement par rapport a la direction de la camera.
            //  - Avant (walk/run)  : le modele fait face au deplacement
            //    (ex: Z+Q → offset +45°, Z+D → -45°).
            //  - Recul  (walkback) : le modele fait face a l'OPPOSE du
            //    deplacement (offset 180° - theta), pour que les pas arriere
            //    de l'anim (vers l'arriere du corps) suivent la trajectoire.
            //    Ex: S+Q → -45° (modele face avant-droite, pas vers
            //    l'arriere-gauche). Recul droit (S seul) → offset 0°.
            if (targetIdx >= 0 && (targetIdx == runIdx || targetIdx == walkIdx || targetIdx == walkBackIdx)) {
                const float thetaDeg = glm::degrees(std::atan2(lateralSpeed, forwardSpeed));
                if (targetIdx == walkBackIdx) {
                    targetYawOffsetDeg = 180.0f - thetaDeg;
                    if (targetYawOffsetDeg > 180.0f) targetYawOffsetDeg -= 360.0f;
                } else {
                    targetYawOffsetDeg = glm::clamp(-thetaDeg, -90.0f, 90.0f);
                }
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

    // ── Post-atterrissage : mise a jour du timer de ralenti ──────────────
    if (m_postLandTimer > 0.0f) {
        m_postLandTimer -= dt;
        if (m_postLandTimer <= 0.0f) {
            m_postLandTimer = 0.0f;
            m_postLandSpeedFactor = 1.0f;
        } else {
            // interpolation lineaire de 1.0 → targetFactor → 1.0
            const float halfDur = m_postLandDuration * 0.5f;
            const float elapsed = m_postLandDuration - m_postLandTimer;
            if (elapsed < halfDur) {
                // Premiere moitie : 1.0 → targetFactor
                const float t = elapsed / halfDur;
                m_postLandSpeedFactor = 1.0f + (m_postLandTargetFactor - 1.0f) * t;
            } else {
                // Deuxieme moitie : targetFactor → 1.0
                const float t = (elapsed - halfDur) / halfDur;
                m_postLandSpeedFactor = m_postLandTargetFactor + (1.0f - m_postLandTargetFactor) * t;
            }
        }
    }

    // Lissage de l'offset de cap (pivotement progressif, pas de snap) puis
    // application au modele (utilise par getModelMatrix).
    const float blend = 1.0f - std::exp(-dt * kYawOffsetSmoothRate);
    m_modelYawOffsetDeg += (targetYawOffsetDeg - m_modelYawOffsetDeg) * blend;
    m_entity->setYawOffsetDeg(m_modelYawOffsetDeg);

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
    const int fallIdleI = entity->getFallingIdleIdx();
    const int fallLandI = entity->getFallingToLandingIdx();

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
        if (idx == runJumpI)  line += "  RUNNING JUMP";
        if (idx == walkBackI) line += "  WALK_BACK";
        if (idx == fallIdleI) line += "  FALLING_IDLE";
        if (idx == fallLandI) line += "  FALLING_LANDING";

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
