#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct GLFWwindow;
class ShaderManager;
class TextRenderer;
class Image;

// ---------------------------------------------------------------------------
// LobbyChat : chat du lobby Steam (multijoueur).
//
// Affiche les logs systeme du lobby (lobby cree, joueur rejoint/quitte, ...)
// et les messages des joueurs. Pas de bouton a l'ecran : la saisie s'ouvre
// avec Entree, s'envoie avec Entree, s'annule avec Echap.
// ---------------------------------------------------------------------------
class LobbyChat {
public:
    LobbyChat(ShaderManager* shaderManager, GLFWwindow* window,
              std::vector<std::unique_ptr<TextRenderer>>* textRenderers);
    ~LobbyChat();

    // ── Messages ──────────────────────────────────────────────────────────
    void addSystemMessage(const std::string& text);
    void addUserMessage(const std::string& sender, const std::string& text);
    void clear();   // vide l'historique et la saisie (lobby quitte)

    // ── Saisie ────────────────────────────────────────────────────────────
    bool isInputActive() const { return m_inputActive; }
    void onChar(unsigned int codepoint);  // alimente par le char callback GLFW
    void closeInput();                    // annule la saisie en cours

    // Nom affiche a cote des messages locaux (persona name Steam).
    void setLocalName(const std::string& name) { m_localName = name; }

    // À appeler chaque frame (STATE_PLAYING, dans un lobby) : gere
    // Entree / Echap / Retour-arriere.
    void update(float dt);

    // Dessine le panneau + messages + ligne de saisie. À appeler entre
    // beginTextFrame() et flushTextFrame().
    void draw();

    // Envoi : appele avec le texte saisi (branche par Game sur
    // MultiplayerManager::sendChatMessage).
    void setOnSend(std::function<void(const std::string&)> cb) { m_onSend = std::move(cb); }

private:
    struct Entry {
        std::string text;
        bool system = false;   // true = log systeme, false = message joueur
    };

    void flushPendingChars();
    void backspace();
    static std::string trim(const std::string& s);
    // Tronque `s` pour tenir dans `maxWidth` pixels (avec "..."), sans couper
    // au milieu d'un point de code UTF-8.
    std::string truncateToWidth(const std::string& s, float maxWidth, TextRenderer* tr) const;

    GLFWwindow* m_window = nullptr;
    std::vector<std::unique_ptr<TextRenderer>>* m_textRenderers = nullptr;
    std::function<void(const std::string&)> m_onSend;

    std::vector<Entry> m_messages;   // historique (borne MAX_MESSAGES)
    std::string m_inputText;
    std::string m_pendingChars;      // caracteres arrives via le char callback
    bool m_inputActive = false;
    std::string m_localName;

    // Edge detection clavier (sondage glfwGetKey)
    bool m_enterDownPrev = false;
    bool m_escDownPrev = false;
    bool m_backspaceDownPrev = false;

    float m_cursorTimer = 0.0f;      // clignotement du curseur de saisie

    // Panneau (fond sombre semi-transparent)
    std::unique_ptr<Image> m_panel;
    unsigned int m_panelTexture = 0; // texture 1x1 generee (detruite ici)
    float m_scale = 0.35f;
    float m_margin = 16.0f;
    float m_panelW = 620.0f;
    float m_panelH = 0.0f;           // calcule a la construction
    float m_lineH = 0.0f;            // hauteur d'une ligne de texte

    static constexpr size_t MAX_MESSAGES = 50;
    static constexpr size_t MAX_INPUT_LEN = 120;
    static constexpr int MAX_VISIBLE_LINES = 6;
};
