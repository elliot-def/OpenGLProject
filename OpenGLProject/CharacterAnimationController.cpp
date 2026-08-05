#include "CharacterAnimationController.h"
#include "ModelEntity.h"
#include "Animator.h"
#include "InputManager.h"
#include "Key.h"
#include "TextRenderer.h"
#include <glad/glad.h>

CharacterAnimationController::CharacterAnimationController(ModelEntity* entity, InputManager* input)
    : m_entity(entity), m_input(input) {
}

void CharacterAnimationController::update(const glm::vec3& playerPos, float dt, bool isSprinting) {
    if (!m_entity || !m_entity->hasAnimations()) return;

    // Detection du mouvement par le delta de position horizontal
    const glm::vec3 delta = playerPos - m_lastPos;
    const float horizSpeed = glm::length(glm::vec3(delta.x, 0.0f, delta.z))
                             / std::max(dt, 1e-5f);
    const bool isMoving = horizSpeed > 0.1f;
    m_lastPos = playerPos;

    const int idleIdx  = m_entity->getIdleAnimIndex();
    const int walkIdx  = m_entity->getWalkAnimIndex();
    const int runIdx   = m_entity->getRunAnimIndex();
    const int punchIdx = m_entity->getPunchAnimIndex();
    const int restIdx  = m_entity->getRestAnimIndex();

    // ── Jab (touche R = KEY_PUSH) : punch one-shot ──────────────────────
    const bool rDown = m_input->getKey("Push")->getStatus();
    if (rDown && !m_prevRDown && !m_punching && punchIdx >= 0) {
        m_entity->playAnimation(punchIdx, false);
        m_punching = true;
        m_lastAnimIdx = punchIdx;
    }
    m_prevRDown = rDown;

    // Fin du punch (animation non-loop terminee)
    if (m_punching && m_entity->getAnimator()->isFinished()) {
        m_punching = false;
    }

    // ── Selection de l'animation de repos (hors punch) ──────────────────
    int targetIdx = -1;
    if (!m_punching) {
        if (isMoving) {
            m_restTimer = 0.0f;
            targetIdx = (isSprinting && runIdx >= 0) ? runIdx : walkIdx;
        } else {
            // Immobile : \"Rest\" pendant le delai, puis idle
            if (restIdx >= 0 && m_restTimer < kRestToIdleDelay) {
                targetIdx = restIdx;
                m_restTimer += dt;
            } else {
                targetIdx = idleIdx;
            }
        }

        if (targetIdx >= 0 && targetIdx != m_lastAnimIdx) {
            m_entity->playAnimation(targetIdx, true);
            m_lastAnimIdx = targetIdx;
        }
    }

    m_entity->updateAnimation(dt);
}

// static
void CharacterAnimationController::drawDebugHUD(ModelEntity* entity,
                                                const std::vector<std::unique_ptr<TextRenderer>>& renderers) {
    if (!entity || !entity->hasAnimations() || renderers.empty()) return;

    const aiScene* scene = entity->getModel()->getScene();
    const Animator* animator = entity->getAnimator();
    const int idleIdx  = entity->getIdleAnimIndex();
    const int walkIdx  = entity->getWalkAnimIndex();
    const int runIdx   = entity->getRunAnimIndex();
    const int punchIdx = entity->getPunchAnimIndex();
    const int restIdx  = entity->getRestAnimIndex();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    TextRenderer* hud = renderers[0].get();
    const float scale = 0.30f;
    const float lineH = 34.0f;
    float y = 20.0f;

    hud->renderText("Animations du modele (vert = en cours)",
                    20.0f, y, scale, 0.85f, 0.85f, 0.85f);
    y += lineH;

    constexpr unsigned int MAX_HUD_ANIMS = 32;

    if (scene) {
        for (unsigned int i = 0; i < scene->mNumAnimations && i < MAX_HUD_ANIMS; i++) {
            const std::string name(scene->mAnimations[i]->mName.C_Str());
            std::string line = "[" + std::to_string(i) + "] " + name;
            if (idleIdx  >= 0 && i == static_cast<unsigned int>(idleIdx))  line += "  (idle)";
            if (walkIdx  >= 0 && i == static_cast<unsigned int>(walkIdx))  line += "  (walk)";
            if (runIdx   >= 0 && i == static_cast<unsigned int>(runIdx))   line += "  (run)";
            if (punchIdx >= 0 && i == static_cast<unsigned int>(punchIdx)) line += "  (punch)";
            if (restIdx  >= 0 && i == static_cast<unsigned int>(restIdx))  line += "  (rest)";

            const bool isCurrent = animator && (scene->mAnimations[i] == animator->getCurrentAnimation());
            hud->renderText(line, 20.0f, y, scale,
                isCurrent ? 0.35f : 0.85f,
                isCurrent ? 0.95f : 0.85f,
                isCurrent ? 0.45f : 0.85f);
            y += lineH;
        }
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
