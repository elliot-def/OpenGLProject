#include "LobbyChat.h"

// Image.h tire glad + GLFW (necessaire pour glfwGetKey ci-dessous).
#include "Image.h"
#include "TextRenderer.h"
#include "ShaderManager.h"
#include "constants/window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>

namespace {
// Couleurs du chat (RGB 0..1)
constexpr float kSystemR = 0.85f, kSystemG = 0.80f, kSystemB = 0.62f; // logs systeme
constexpr float kNameR   = 0.52f, kNameG   = 0.75f, kNameB   = 0.88f; // nom joueur
constexpr float kTextR   = 0.93f, kTextG   = 0.93f, kTextB   = 0.95f; // message joueur
constexpr float kInputR  = 1.00f, kInputG  = 1.00f, kInputB  = 1.00f; // ligne de saisie
constexpr float kShadow  = 0.0f;                                       // ombre portee
} // namespace

LobbyChat::LobbyChat(ShaderManager* shaderManager, GLFWwindow* window,
                     std::vector<std::unique_ptr<TextRenderer>>* textRenderers)
    : m_window(window), m_textRenderers(textRenderers) {
    if (!m_textRenderers || m_textRenderers->empty()) return;
    TextRenderer* tr = (*m_textRenderers)[0].get();
    if (!tr) return;

    // Hauteur d'une ligne + hauteur du panneau (messages + ligne de saisie).
    m_lineH = tr->getTextHeight("Ag", m_scale) + 8.0f;
    m_panelH = (MAX_VISIBLE_LINES + 1) * m_lineH + m_margin * 2.0f;

    // ── Panneau : texture 1x1 sombre + Image (shader "image", opacite) ──
    glGenTextures(1, &m_panelTexture);
    glBindTexture(GL_TEXTURE_2D, m_panelTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    unsigned char dark[4] = { 18, 18, 22, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, dark);
    glBindTexture(GL_TEXTURE_2D, 0);

    const float panelX = m_margin;
    const float panelY = static_cast<float>(Constants::Window::WINDOW_HEIGHT)
                         - m_margin - m_panelH;
    m_panel = std::make_unique<Image>(shaderManager->getShader("image"),
                                      m_panelTexture, panelX, panelY,
                                      m_panelW, m_panelH, 0.55f);
}

LobbyChat::~LobbyChat() {
    if (m_panelTexture != 0) {
        glDeleteTextures(1, &m_panelTexture);
    }
}

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------

void LobbyChat::addSystemMessage(const std::string& text) {
    if (m_messages.size() >= MAX_MESSAGES) {
        m_messages.erase(m_messages.begin());
    }
    m_messages.push_back({ text, true });
}

void LobbyChat::addUserMessage(const std::string& sender, const std::string& text) {
    if (m_messages.size() >= MAX_MESSAGES) {
        m_messages.erase(m_messages.begin());
    }
    m_messages.push_back({ sender + ": " + text, false });
}

void LobbyChat::clear() {
    m_messages.clear();
    m_inputText.clear();
    m_pendingChars.clear();
    m_inputActive = false;
}

// ---------------------------------------------------------------------------
// Saisie
// ---------------------------------------------------------------------------

void LobbyChat::onChar(unsigned int codepoint) {
    if (!m_inputActive) return;
    // On ignore les caracteres de controle (y compris Entree via le callback
    // de caractere : Entree est gere par sondage clavier dans update()).
    if (codepoint < 0x20 || codepoint == 0x7F) return;

    // Encodage UTF-8 du point de code.
    if (codepoint <= 0x7F) {
        m_pendingChars += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        m_pendingChars += static_cast<char>(0xC0 | (codepoint >> 6));
        m_pendingChars += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        m_pendingChars += static_cast<char>(0xE0 | (codepoint >> 12));
        m_pendingChars += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        m_pendingChars += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        m_pendingChars += static_cast<char>(0xF0 | (codepoint >> 18));
        m_pendingChars += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        m_pendingChars += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        m_pendingChars += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

void LobbyChat::closeInput() {
    m_inputActive = false;
    m_inputText.clear();
}

void LobbyChat::backspace() {
    if (m_inputText.empty()) return;
    // Remonter au debut du dernier point de code UTF-8 (octets de suite 10xxxxxx).
    size_t i = m_inputText.size();
    while (i > 0) {
        const unsigned char c = static_cast<unsigned char>(m_inputText[--i]);
        if ((c & 0xC0) != 0x80) break;
    }
    m_inputText.erase(i);
}

std::string LobbyChat::trim(const std::string& s) {
    const size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

void LobbyChat::flushPendingChars() {
    if (m_pendingChars.empty()) return;
    // Limite de longueur : on garde les premiers MAX_INPUT_LEN octets en
    // s'arretant sur une frontiere UTF-8 (ne pas couper un point de code).
    const size_t maxBytes = MAX_INPUT_LEN;
    if (m_inputText.size() < maxBytes) {
        size_t room = maxBytes - m_inputText.size();
        size_t take = std::min(room, m_pendingChars.size());
        if (take < m_pendingChars.size()) {
            // Ne pas couper un point de code : reculer jusqu'au debut du
            // caractere incomplet.
            while (take > 0 &&
                   (static_cast<unsigned char>(m_pendingChars[take]) & 0xC0) == 0x80) {
                --take;
            }
        }
        m_inputText += m_pendingChars.substr(0, take);
    }
    m_pendingChars.clear();
}

void LobbyChat::update(float dt) {
    flushPendingChars();
    m_cursorTimer += dt;

    const bool enterDown = glfwGetKey(m_window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                           glfwGetKey(m_window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    const bool escDown = glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    const bool backspaceDown = glfwGetKey(m_window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;

    // Entree : ouvre la saisie si fermee, sinon envoie le message.
    if (enterDown && !m_enterDownPrev) {
        if (!m_inputActive) {
            m_inputActive = true;
            m_cursorTimer = 0.0f;
        } else {
            const std::string msg = trim(m_inputText);
            if (!msg.empty()) {
                if (m_onSend) m_onSend(msg);
                addUserMessage(m_localName, msg);
            }
            m_inputText.clear();
            m_inputActive = false;
        }
    }

    // Echap : annule la saisie.
    if (m_inputActive && escDown && !m_escDownPrev) {
        m_inputActive = false;
        m_inputText.clear();
    }

    // Retour-arriere : efface le dernier caractere.
    if (m_inputActive && backspaceDown && !m_backspaceDownPrev) {
        backspace();
    }

    m_enterDownPrev = enterDown;
    m_escDownPrev = escDown;
    m_backspaceDownPrev = backspaceDown;
}

// ---------------------------------------------------------------------------
// Rendu
// ---------------------------------------------------------------------------

std::string LobbyChat::truncateToWidth(const std::string& s, float maxWidth,
                                       TextRenderer* tr) const {
    if (tr->getTextWidth(s, m_scale) <= maxWidth) return s;
    std::string out = s;
    while (!out.empty()) {
        // Raccourcir d'un point de code UTF-8 (les octets de suite ne sont
        // pas des debuts de caractere).
        size_t i = out.size();
        while (i > 0) {
            const unsigned char c = static_cast<unsigned char>(out[--i]);
            if ((c & 0xC0) != 0x80) break;
        }
        out.erase(i);
        if (tr->getTextWidth(out + "...", m_scale) <= maxWidth) break;
    }
    return out + "...";
}

void LobbyChat::draw() {
    if (!m_textRenderers || m_textRenderers->empty()) return;
    TextRenderer* tr = (*m_textRenderers)[0].get();
    if (!tr) return;

    const float panelTop = static_cast<float>(Constants::Window::WINDOW_HEIGHT)
                           - m_margin - m_panelH;
    const float textMaxW = m_panelW - m_margin * 2.0f;

    // Panneau sombre.
    if (m_panel) m_panel->draw();

    // ── Historique (les MAX_VISIBLE_LINES plus recents, du haut vers le bas) ──
    float y = panelTop + m_margin;
    const size_t start = m_messages.size() > MAX_VISIBLE_LINES
                         ? m_messages.size() - MAX_VISIBLE_LINES : 0;
    for (size_t i = start; i < m_messages.size(); ++i) {
        const Entry& e = m_messages[i];
        if (e.system) {
            const std::string line = truncateToWidth(e.text, textMaxW, tr);
            tr->renderText(line, m_margin + 2.0f, y + 2.0f, m_scale, kShadow, kShadow, kShadow);
            tr->renderText(line, m_margin, y, m_scale, kSystemR, kSystemG, kSystemB);
        } else {
            // Nom en couleur, message en clair : on decoupe apres le premier
            // ": " (ajoute par addUserMessage).
            const size_t sep = e.text.find(": ");
            const std::string name = sep == std::string::npos ? e.text : e.text.substr(0, sep + 2);
            std::string body = sep == std::string::npos ? "" : e.text.substr(sep + 2);
            const float nameW = tr->getTextWidth(name, m_scale);
            body = truncateToWidth(body, textMaxW - nameW, tr);
            const std::string line = name + body;
            tr->renderText(line, m_margin + 2.0f, y + 2.0f, m_scale, kShadow, kShadow, kShadow);
            tr->renderText(name, m_margin, y, m_scale, kNameR, kNameG, kNameB);
            tr->renderText(body, m_margin + nameW, y, m_scale, kTextR, kTextG, kTextB);
        }
        y += m_lineH;
    }

    // ── Ligne de saisie ──
    const float inputY = panelTop + m_panelH - m_lineH + 4.0f;
    std::string inputLine = "> " + m_inputText;
    if (m_inputActive && (static_cast<int>(m_cursorTimer * 2.0f) % 2 == 0)) {
        inputLine += "|";
    }
    // Texte long : garder la FIN visible (la ou se trouve le curseur) en
    // retirant les premiers caracteres jusqu'a ce que la ligne tienne.
    while (!inputLine.empty() && tr->getTextWidth(inputLine, m_scale) > textMaxW) {
        size_t i = 1;   // retirer le premier point de code UTF-8
        while (i < inputLine.size() &&
               (static_cast<unsigned char>(inputLine[i]) & 0xC0) == 0x80) {
            ++i;
        }
        inputLine.erase(0, i);
    }
    tr->renderText(inputLine, m_margin + 2.0f, inputY + 2.0f, m_scale, kShadow, kShadow, kShadow);
    tr->renderText(inputLine, m_margin, inputY, m_scale, kInputR, kInputG, kInputB);
}
