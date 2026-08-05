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
// Encapsule la machine a etats d'animation du personnage 3P (humain) :
//  - Detection du mouvement par delta de position
//  - Selection idle/marche/course/repos
//  - Jab one-shot (punch sur R)
//  - Affichage du HUD debug (liste des animations)
//
// Extrait de Game::update()/draw() pour reduire la taille de Game (~120 lignes).
// ─────────────────────────────────────────────────────────────────────────────

class CharacterAnimationController {
public:
    CharacterAnimationController(ModelEntity* entity, InputManager* input);

    // Appele chaque frame dans Game::update().
    // playerPos : position actuelle du joueur
    // dt         : delta time
    // isSprinting: le joueur sprinte-t-il ?
    void update(const glm::vec3& playerPos, float dt, bool isSprinting);

    // Dessine le HUD de debug listant les animations du modele (3P seulement).
    // Appele dans Game::draw() apres le rendu opaque, avant le flush texte.
    static void drawDebugHUD(ModelEntity* entity,
                             const std::vector<std::unique_ptr<TextRenderer>>& renderers);

private:
    ModelEntity* m_entity;
    InputManager* m_input;

    // ── Etat de la machine (instance, plus de static locals dans Game) ──
    glm::vec3 m_lastPos{0.0f};
    int m_lastAnimIdx = -1;
    float m_restTimer = 0.0f;
    bool m_prevRDown = false;
    bool m_punching = false;

    static constexpr float kRestToIdleDelay = 2.5f;
};
