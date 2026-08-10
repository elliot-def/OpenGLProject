#include "DialogManager.h"
#include "NPC.h"
#include "TextRenderer.h"

#include <algorithm>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Contrôle du dialog
// ─────────────────────────────────────────────────────────────────────────────

const DialogNode* DialogManager::getCurrentNode() const {
    if (!m_activeNPC) return nullptr;
    return m_activeNPC->getDialog().getNode(m_currentNodeId);
}

void DialogManager::startDialog(NPC* npc) {
    if (!npc || !npc->hasDialog()) return;

    m_activeNPC = npc;
    const DialogNode* root = npc->getDialog().getRoot();
    if (!root) return;

    m_currentNodeId = root->id;
    m_isActive = true;
    m_charTimer = 0.0f;
    m_visibleChars = 0;
    m_textComplete = false;

    npc->setState(NPC::State::TALKING);
}

void DialogManager::advanceTo(const std::string& nodeId) {
    if (!m_activeNPC) return;

    const DialogNode* node = m_activeNPC->getDialog().getNode(nodeId);
    if (!node) {
        endDialog();
        return;
    }

    m_currentNodeId = nodeId;
    m_charTimer = 0.0f;
    m_visibleChars = 0;
    m_textComplete = false;

    // Si c'est un nœud terminal sans choix, le texte s'affiche en entier
    if (node->choices.empty() && node->isEnd) {
        m_textComplete = true;
        m_visibleChars = static_cast<int>(node->text.length());
    }
}

void DialogManager::endDialog() {
    if (m_activeNPC) {
        m_activeNPC->setState(NPC::State::IDLE);
    }
    m_activeNPC = nullptr;
    m_currentNodeId.clear();
    m_isActive = false;
}

bool DialogManager::handleChoice(int index) {
    const DialogNode* node = getCurrentNode();
    if (!node || index < 0 || index >= static_cast<int>(node->choices.size()))
        return false;

    const DialogChoice& choice = node->choices[index];
    advanceTo(choice.nextNodeId);
    return true;
}

bool DialogManager::advance() {
    const DialogNode* node = getCurrentNode();
    if (!node) return false;

    // Si le texte n'est pas fini, on le complète d'un coup
    if (!m_textComplete) {
        m_visibleChars = static_cast<int>(node->text.length());
        m_textComplete = true;
        return true;
    }

    // Si c'est une feuille, fermer le dialog
    if (node->choices.empty()) {
        endDialog();
        return false;
    }

    // Un seul choix non-feuille : avancer automatiquement
    if (node->choices.size() == 1) {
        advanceTo(node->choices[0].nextNodeId);
        return true;
    }

    return false; // plusieurs choix : le joueur doit en choisir un
}

// ─────────────────────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────────────────────

void DialogManager::update(float deltaTime) {
    if (!m_isActive || !m_activeNPC) return;

    // Effet machine à écrire
    if (!m_textComplete) {
        const DialogNode* node = getCurrentNode();
        if (node) {
            m_charTimer += deltaTime;
            int targetChars = static_cast<int>(m_charTimer / kCharDelay);
            int maxChars = static_cast<int>(node->text.length());
            m_visibleChars = std::min(targetChars, maxChars);
            if (m_visibleChars >= maxChars) {
                m_textComplete = true;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendu de l'UI dialog (style professionnel avec ombre portée)
// ─────────────────────────────────────────────────────────────────────────────

void DialogManager::render(TextRenderer* renderer, int screenWidth, int screenHeight) const {
    if (!m_isActive || !m_activeNPC || !renderer) return;

    const DialogNode* node = getCurrentNode();
    if (!node) return;

    // Dimensions de la boîte de dialog
    constexpr float boxHeight = 160.0f;
    constexpr float boxPaddingX = 40.0f;
    constexpr float boxPaddingY = 30.0f;
    constexpr float boxWidth = 600.0f;
    const float boxX = (screenWidth - boxWidth) / 2.0f;
    const float boxY = screenHeight - boxHeight - 20.0f;

    // ── Ombre portée : rendu du texte en noir décalé pour lisibilité ─────
    constexpr float shadowOffset = 2.0f;
    auto renderWithShadow = [&](const std::string& text, float x, float y,
                                 float scale, float r, float g, float b) {
        // Ombre
        renderer->renderText(text, x + shadowOffset, y - shadowOffset, scale,
                            0.0f, 0.0f, 0.0f);
        // Texte principal
        renderer->renderText(text, x, y, scale, r, g, b);
    };

    // ── Nom du PNJ ────────────────────────────────────────────────────────
    float nameX = boxX + boxPaddingX;
    float nameY = boxY + boxPaddingY;
    renderWithShadow(node->speakerName, nameX, nameY, 0.5f, 1.0f, 0.85f, 0.2f);

    // ── Texte du dialog (effet machine à écrire) ──────────────────────────
    std::string visibleText = node->text.substr(0, m_visibleChars);
    if (!m_textComplete) {
        visibleText += "_";
    }

    float textY = nameY + 35.0f;
    std::istringstream stream(visibleText);
    std::string line;
    while (std::getline(stream, line, '\n')) {
        renderWithShadow(line, nameX, textY, 0.4f, 1.0f, 1.0f, 1.0f);
        textY += 24.0f;
    }

    // ── Choix (affichés seulement quand le texte est complet) ─────────────
    if (m_textComplete && !node->choices.empty()) {
        float choiceY = boxY + boxHeight - boxPaddingY - 20.0f;
        for (size_t i = 0; i < node->choices.size(); i++) {
            std::string choiceText = std::to_string(i + 1) + ". " + node->choices[i].label;
            renderWithShadow(choiceText, nameX, choiceY, 0.35f, 1.0f, 0.85f, 0.2f);

            std::string hint = "[ " + std::to_string(i + 1) + " ]";
            float hintWidth = renderer->getTextWidth(hint, 0.3f);
            renderWithShadow(hint, boxX + boxWidth - boxPaddingX - hintWidth,
                            choiceY, 0.3f, 0.5f, 0.5f, 0.5f);
            choiceY -= 22.0f;
        }
    } else if (m_textComplete && node->choices.empty()) {
        // Indice de fermeture
        std::string closeHint = "[F] Fermer";
        float hintWidth = renderer->getTextWidth(closeHint, 0.3f);
        renderWithShadow(closeHint,
                        boxX + boxWidth - boxPaddingX - hintWidth,
                        boxY + boxHeight - boxPaddingY - 20.0f,
                        0.3f, 0.5f, 0.5f, 0.5f);
    }
}

void DialogManager::renderDialogBox(TextRenderer* renderer, int screenW, int screenH) const {
    // Le fond de boîte est simulé via l'ombre portée du texte (renderWithShadow).
    // Pour un vrai fond semi-transparent, il faudrait un quad OpenGL avec blending.
    (void)renderer;
    (void)screenW;
    (void)screenH;
}
