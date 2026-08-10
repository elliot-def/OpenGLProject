#pragma once

#include "DialogNode.h"
#include <string>

class NPC;
class TextRenderer;
class Camera;

// ─────────────────────────────────────────────────────────────────────────────
// DialogManager : gère l'état du dialog actif et le rendu de l'UI
//
// Usage :
//   DialogManager dm;
//   dm.startDialog(npc);          // commence le dialog avec un PNJ
//   dm.update(playerPos, dt);     // chaque frame (lookAt du PNJ + timer)
//   dm.handleChoice(0);           // sélectionne le choix n°0
//   dm.render(textRenderer, w, h); // dessine l'UI de dialog
//   dm.endDialog();               // termine le dialog
// ─────────────────────────────────────────────────────────────────────────────
class DialogManager {
public:
    DialogManager() = default;

    // ── Contrôle du dialog ────────────────────────────────────────────────
    bool isActive() const { return m_isActive; }
    NPC* getActiveNPC() const { return m_activeNPC; }
    const DialogNode* getCurrentNode() const;

    void startDialog(NPC* npc);
    void advanceTo(const std::string& nodeId);
    void endDialog();
    void cancelDialog() { endDialog(); }  // alias sémantique : annulation = fin

    // Sélectionne un choix par son index (0-based)
    // Retourne true si le choix a été appliqué
    bool handleChoice(int index);

    // Avance au nœud suivant (pour les monologues : clic = avancer)
    // Utile quand il y a un seul choix ou pas de choix
    bool advance();

    // ── Update ────────────────────────────────────────────────────────────
    // deltaTime : temps écoulé depuis la dernière frame
    void update(float deltaTime);

    // ── Rendu de l'UI dialog ──────────────────────────────────────────────
    // Affiche la boîte de dialog en bas de l'écran avec le texte du PNJ
    // et les choix disponibles.
    // screenWidth / screenHeight : dimensions de la fenêtre en pixels
    void render(TextRenderer* renderer, int screenWidth, int screenHeight) const;

private:
    NPC* m_activeNPC = nullptr;
    std::string m_currentNodeId;
    bool m_isActive = false;
    float m_charTimer = 0.0f;      // timer pour l'effet machine à écrire
    int m_visibleChars = 0;        // nombre de caractères visibles
    bool m_textComplete = false;   // tout le texte est-il affiché ?

    static constexpr float kCharDelay = 0.025f;  // délai entre chaque caractère
    static constexpr int kMaxVisibleChars = 1024;

    // Rendu d'une boîte de dialog centrée horizontalement
    void renderDialogBox(TextRenderer* renderer, int screenW, int screenH) const;
};
