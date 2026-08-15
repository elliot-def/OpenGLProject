#include "InputNotification.h"
#include "TextRenderer.h"

#include "constants/window.h"

// Encode un point de code Unicode (ex: U+E000, zone privee des icones
// kenney) en chaine UTF-8 pour renderText().
static std::string utf8Encode(unsigned int codepoint) {
    std::string out;
    if (codepoint < 0x80) {
        out += static_cast<char>(codepoint);
    }
    else if (codepoint < 0x800) {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    else {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    return out;
}

void InputNotification::show(InputSource source) {
    m_source = source;
    m_kind = NotificationKind::SOURCE_SWITCH;
    m_visible = true;
    m_shownAt = std::chrono::steady_clock::now();
}

void InputNotification::showConnected() {
    m_source = InputSource::CONTROLLER;
    m_kind = NotificationKind::CONNECTED;
    m_visible = true;
    m_shownAt = std::chrono::steady_clock::now();
}

void InputNotification::showDisconnected() {
    m_source = InputSource::CONTROLLER;
    m_kind = NotificationKind::DISCONNECTED;
    m_visible = true;
    m_shownAt = std::chrono::steady_clock::now();
}

bool InputNotification::isVisible() const {
    if (!m_visible) return false;
    float elapsed = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - m_shownAt).count();
    return elapsed < NOTIFICATION_DURATION;
}

void InputNotification::draw(TextRenderer* controllerIcons, TextRenderer* keyboardIcons,
                             TextRenderer* textRenderer) {
    if (!isVisible() || !controllerIcons || !keyboardIcons || !textRenderer) return;

    // ── Contenu selon le type de notification ──
    // Pas d'accents : l'ecran est rendu avec une police ASCII (voir agent.md).
    std::string label;
    std::string icon1;  // icone principale
    std::string icon2;  // icone secondaire (souris en mode clavier/souris)
    TextRenderer* iconRenderer = nullptr;

    switch (m_kind) {
    case NotificationKind::CONNECTED:
        label = "Manette branchee";
        icon1 = utf8Encode(0xE000);  // controller_steam = U+E000 (police manette)
        iconRenderer = controllerIcons;
        break;
    case NotificationKind::DISCONNECTED:
        label = "Manette debranchee";
        icon1 = utf8Encode(0xE000);
        iconRenderer = controllerIcons;
        break;
    case NotificationKind::SOURCE_SWITCH:
    default:
        if (m_source == InputSource::CONTROLLER) {
            label = "Manette";
            icon1 = utf8Encode(0xE000);
            iconRenderer = controllerIcons;
        } else {
            label = "Clavier & Souris";
            // keyboard = U+E000, mouse = U+E0E1 dans la police clavier/souris kenney
            icon1 = utf8Encode(0xE000);
            icon2 = utf8Encode(0xE0E1);
            iconRenderer = keyboardIcons;
        }
        break;
    }

    // ── Largeurs pour centrer le groupe [icones][texte] ──
    float iconW1 = iconRenderer->getTextWidth(icon1, ICON_SCALE);
    float iconH = iconRenderer->getTextHeight(icon1, ICON_SCALE);
    float iconW2 = icon2.empty() ? 0.0f : iconRenderer->getTextWidth(icon2, ICON_SCALE);
    float textW = textRenderer->getTextWidth(label, TEXT_SCALE);
    float textH = textRenderer->getTextHeight(label, TEXT_SCALE);

    float totalW = iconW1 + (icon2.empty() ? 0.0f : GAP + iconW2) + GAP + textW;
    float centerX = static_cast<float>(Constants::Window::WINDOW_WIDTH) / 2.0f;
    float x = centerX - totalW / 2.0f;
    float iconY = NOTIFICATION_Y;
    float textY = NOTIFICATION_Y + (iconH - textH) / 2.0f;

    // ── Ombre portee puis texte principal (contraste sur tout fond) ──
    const float shadowOffset = 2.0f;
    const float shadowColor[3] = { 0.1f, 0.1f, 0.1f };
    const float mainColor[3] = { 1.0f, 1.0f, 1.0f };

    for (int pass = 0; pass < 2; ++pass) {
        const float* color = (pass == 0) ? shadowColor : mainColor;
        float dx = (pass == 0) ? shadowOffset : 0.0f;

        float cx = x + dx;
        iconRenderer->renderText(icon1, cx, iconY + dx, ICON_SCALE, color[0], color[1], color[2]);
        cx += iconW1;
        if (!icon2.empty()) {
            cx += GAP;
            iconRenderer->renderText(icon2, cx, iconY + dx, ICON_SCALE, color[0], color[1], color[2]);
            cx += iconW2;
        }
        cx += GAP;
        textRenderer->renderText(label, cx, textY + dx, TEXT_SCALE, color[0], color[1], color[2]);
    }
}
