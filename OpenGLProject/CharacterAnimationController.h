#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

#include "constants/player.h"

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

    // Retourne le facteur de vitesse post-atterrissage (0..1).
    // 1.0 = vitesse normale, <1.0 = ralenti apres chute/saut.
    float getPostLandSpeedFactor() const { return m_postLandSpeedFactor; }

    // Reinitialise l'etat de la machine a animations (suivi position/cap,
    // one-shots, cooldowns). A appeler a la sortie du no-clip : pendant le
    // no-clip, update() n'est pas appele, donc m_lastPos/m_lastYaw sont
    // perimes. Sans reset, le premier update calcule un delta geant -> faux
    // saut/turn/fall qui laisse le modele (et la camera 1P qui suit la tete)
    // dans une pose incorrecte qui ne se resout pas immediatement.
    void resetState(const glm::vec3& currentPos, float currentYaw);

    // Dessine le HUD de debug listant les animations du modele (3P seulement).
    static void drawDebugHUD(ModelEntity* entity,
                             const std::vector<std::unique_ptr<TextRenderer>>& renderers);

private:
    // Declenche le ralenti post-atterrissage (apres saut ou chute).
    // speedFactor : facteur de vitesse cible (0..1). 1.0 = pas de ralenti.
    // duration : duree du ralenti en secondes.
    void startPostLandSlowdown(float speedFactor = Constants::Player::POST_LAND_SPEED_FACTOR,
                               float duration = Constants::Player::POST_LAND_SLOWDOWN_DURATION);

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
    bool m_falling = false;        // en chute libre (descente)
    bool m_landing = false;        // animation "falling to landing" en cours

    bool m_prevRDown = false;

    // Position Y au début de la chute (pour calculer la hauteur de chute)
    float m_fallStartY = 0.0f;

    // Post-atterrissage : facteur de vitesse progressif (1.0 -> targetFactor -> 1.0)
    float m_postLandSpeedFactor = 1.0f;  // 1.0 = pas de ralenti
    float m_postLandTargetFactor = Constants::Player::POST_LAND_SPEED_FACTOR; // facteur cible
    float m_postLandTimer = 0.0f;        // temps ecoule depuis l'atterrissage
    float m_postLandDuration = Constants::Player::POST_LAND_SLOWDOWN_DURATION; // duree totale

    // Landing : timer minimum pour que l'animation "Falling To Landing"
    // joue complètement avant de revenir aux animations normales.
    float m_landingAnimTimer = 0.0f;     // temps restant de l'anim de landing

    // Sprint memorise au sol : Player::getIsSprinting() coupe le sprint en
    // l'air, donc au moment du saut (1er frame decolle) isSprinting vaut deja
    // false. On se souvient de l'etat au sol pour choisir running jump / jump.
    bool m_wasSprintingWhenGrounded = false;

    // ── Hysterese anti-clignotement (collision contre un mur) ───────────
    // Quand le joueur pousse contre un mur, la vitesse horizontale oscille
    // pres de zero, ce qui faisait clignoter idle/walk. On utilise un latch :
    // on demarre le mouvement a un seuil haut, on l'arrete a un seuil bas.
    bool m_isMoving = false;
    bool m_isStrafing = false;
    float m_animChangeTimer = 0.0f;  // cooldown entre changements d'anim
    float m_modelYawOffsetDeg = 0.0f; // offset de cap lisse applique au modele 3P

    static constexpr float kRestToIdleDelay = 2.5f;
    static constexpr float kTurnYawThreshold = 30.0f; // degres pour declencher turn
    static constexpr float kJumpThreshold = 0.005f;    // delta.y min pour detecter un saut
    static constexpr float kMoveStartSpeed = 0.25f;    // seuil haut pour commencer a bouger
    static constexpr float kMoveStopSpeed  = 0.06f;    // seuil bas pour arreter de bouger
    static constexpr float kAnimChangeCooldown = 0.15f; // delai min entre 2 changements
    // Composante avant/arriere max au-dessus de laquelle on prefere la marche
    // avant/arriere au strafe : des qu'on avance ou recule en meme temps que
    // lateralement (diagonale), on joue walk/run/walkback, pas le strafe.
    static constexpr float kStrafeMaxForwardSpeed = 0.15f;
    // Vitesse (1/s) du pivotement du modele vers le sens de marche (diagonales).
    // Plus la valeur est haute, plus le pivot est rapide (12 ≈ 80 ms).
    static constexpr float kYawOffsetSmoothRate = 12.0f;
};
