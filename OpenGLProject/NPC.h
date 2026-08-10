#pragma once

#include "ModelEntity.h"
#include "DialogNode.h"

class Camera;
class LightManager;
class Renderer;
class TextureManager;

// ─────────────────────────────────────────────────────────────────────────────
// NPC : Personnage Non-Joueur avec modèle 3D et dialog interactif
//
// Hérite de ModelEntity pour le rendu, les animations et le raycasting.
// Ajoute un arbre de dialog et une détection de proximité pour l'interaction.
//
// Usage typique :
//   NPC* npc = new NPC(camera, lightMgr, renderer, "model.fbx", texMgr);
//   npc->setPosition(glm::vec3(5, 0, 0));
//   npc->setDialog(DialogTree::createExample());
//
//   // Dans la boucle de rendu :
//   if (npc->isPlayerLookingAt(camera)) {
//       // afficher le prompt "Appuyez sur F pour parler"
//   }
// ─────────────────────────────────────────────────────────────────────────────
class NPC : public ModelEntity {
public:
    NPC(Camera* camera, LightManager* lightManager, Renderer* renderer,
        const std::string& modelPath, TextureManager* textureManager);

    ~NPC() = default;

    // ── Dialog ────────────────────────────────────────────────────────────
    void setDialog(const DialogTree& tree) { m_dialog = tree; }
    const DialogTree& getDialog() const { return m_dialog; }
    DialogTree& getDialog() { return m_dialog; }
    bool hasDialog() const { return !m_dialog.isEmpty(); }

    // ── Interaction ───────────────────────────────────────────────────────
    // Distance maximale à laquelle le joueur peut interagir avec ce PNJ
    void setInteractionRadius(float radius) { m_interactionRadius = radius; }
    float getInteractionRadius() const { return m_interactionRadius; }

    // Vérifie si le joueur (position caméra) est assez proche ET regarde le PNJ
    // Retourne la distance si le PNJ est "ciblé", -1 sinon.
    float isPlayerLookingAt(const Camera* camera) const;

    // Le PNJ tourne la tête/le corps pour faire face à une position cible
    // (appelé pendant le dialog pour que le PNJ regarde le joueur)
    void lookAt(const glm::vec3& target);

    // ── État du PNJ ──────────────────────────────────────────────────────
    enum class State { IDLE, TALKING };
    void setState(State state) { m_state = state; }
    State getState() const { return m_state; }
    bool isTalking() const { return m_state == State::TALKING; }

    // ── Update ────────────────────────────────────────────────────────────
    void update(float deltaTime);

private:
    DialogTree m_dialog;
    float m_interactionRadius = 3.0f;  // distance max d'interaction (unités monde)
    State m_state = State::IDLE;
    float m_lookAtTimer = 0.0f;        // durée restante du lookAt
    glm::vec3 m_lookAtTarget{ 0.0f };
};
