#include "Menu.h"
#include "TextRenderer.h"
#include "ShaderManager.h"
#include "MenuManager.h"
#include "TextureManager.h"
#include "Sound.h"
#include "SoundManager.h"
#include "CursorManager.h"
#include "Rectangle.h"
#include "Triangle.h"

#include "constants/window.h"
#include "constants/color.h"

#include "cstdio"
#include <algorithm>

void Menu::drawTextCentered(const std::string& text, float centerX, float centerY, int textRendererIndex, glm::vec3 color, float scale) {
    float textWidth = m_textRenderers->at(textRendererIndex)->getTextWidth(text, scale);
    float textHeight = m_textRenderers->at(textRendererIndex)->getTextHeight(text, scale); // Ajoutez cette méthode si elle n'existe pas
    
    float startX = centerX - textWidth / 2.0f;
    float startY = centerY + textHeight / 2.0f; // Centrage vertical basé sur la hauteur réelle
    
    m_textRenderers->at(textRendererIndex)->renderText(text, startX, startY, scale, color.r, color.g, color.b);
}

void Menu::drawTextRightAligned(const std::string& text, float rightX, float centerY, int textRendererIndex, glm::vec3 color, float scale) {
    float textWidth = m_textRenderers->at(textRendererIndex)->getTextWidth(text, scale);
    float textHeight = m_textRenderers->at(textRendererIndex)->getTextHeight(text, scale); // Ajoutez cette méthode si elle n'existe pas

    float startX = rightX - textWidth;
    float startY = centerY + textHeight / 2.0f; // Centrage vertical basé sur la hauteur réelle

    m_textRenderers->at(textRendererIndex)->renderText(text, startX, startY, scale, color.r, color.g, color.b);
}

void Menu::drawTextLeftAligned(const std::string& text, float leftX, float centerY, int textRendererIndex, glm::vec3 color, float scale) {
    float textWidth = m_textRenderers->at(textRendererIndex)->getTextWidth(text, scale);
    float textHeight = m_textRenderers->at(textRendererIndex)->getTextHeight(text, scale); // Ajoutez cette méthode si elle n'existe pas

    float startX = leftX;
    float startY = centerY + textHeight / 2.0f; // Centrage vertical basé sur la hauteur réelle

    m_textRenderers->at(textRendererIndex)->renderText(text, startX, startY, scale, color.r, color.g, color.b);
}

void Menu::addRange(const std::string& label, float x, float y, float width, float height,
    float minValue, float maxValue, float defaultValue,
    std::function<void(float)> onValueChanged) {
    Shader* shader = m_shaderManager->getShader("shape");
    m_inputs.push_back(std::make_unique<MenuRange>(label, new RangeInput(shader, x, y, width, height, minValue, maxValue, defaultValue, onValueChanged)));
    m_focusDirty = true;
}

void Menu::addCheckbox(const std::string& label, float x, float y, float size,
    bool defaultValue, std::function<void(bool)> onValueChanged) {
    Shader* shader = m_shaderManager->getShader("shape");
    m_inputs.push_back(std::make_unique<MenuCheckbox>(label, new CheckboxInput(shader, x, y, size, defaultValue, onValueChanged)));
    m_focusDirty = true;
}

void Menu::addSelect(const std::string& label, float x, float y, float width, float height,
    std::vector<std::string> options, int defaultIndex,
    std::function<void(int)> onValueChanged) {
    Shader* shader = m_shaderManager->getShader("shape");
    m_inputs.push_back(std::make_unique<MenuSelect>(label, new SelectInput(shader, x, y, width, height, std::move(options), defaultIndex, onValueChanged)));
    m_focusDirty = true;
}

void Menu::draw() {
	// Draw background — Rectangle créé une seule fois, réutilisé chaque frame
    ensureBackground();
    m_background->draw();
    // Dessiner le titre si présent
    if (!m_title.empty()) {
        drawTextCentered(m_title, m_titleX, m_titleY, 1, Constants::Color::LINEN);
    }

    // Dessiner les items du menu
    for (const auto& item : m_items) {
               
        // Texte centré
        float centerX = item.x + item.width / 2.0f;
        float centerY = item.y + item.height / 2.0f;

        if (item.isHovered) {
            drawTextCentered(item.text, centerX, centerY, 0, Constants::Color::TOMATO_JAM);
        }
        else {
            drawTextCentered(item.text, centerX, centerY, 0, Constants::Color::LINEN);

        }
    }
    for (const auto& shape : m_shapes) {
        // Shapes de premier plan : dessinés après le texte (drawOverlays)
        if (std::find(m_overlayShapes.begin(), m_overlayShapes.end(), shape.second->shape.get()) != m_overlayShapes.end()) {
            continue;
        }
        shape.second->shape->draw();
    }

    // Dessiner les widgets (slider/checkbox/select) : libellé + valeur + widget
    for (const auto& w : m_inputs) {
        if (w) w->draw(*this);
    }
}

void Menu::drawOverlays() {
    // Shapes de premier plan : à dessiner APRÈS le flush du texte pour passer
    // devant lui (ex. Easter egg DVD du MainMenu).
    for (auto* shape : m_overlayShapes) {
        shape->draw();
    }
}

bool Menu::handleClick(double mouseX, double mouseY) {
    //printf("mouseX = %f ; mouseY = %f\n", mouseX, mouseY);
    /*
    if (m_items == std::vector<MenuItem>()) {
        printf("Aucun item dans le menu");
        return false;
    }*/

    // Un widget de liste OUVERT capture le clic en priorite : ses options
    // peuvent recouvrir des items/checkbox dessines derriere. Sans cela, un
    // clic sur une option qui chevauche un bouton declenchait le bouton (les
    // items sont testes avant les widgets) au lieu de selectionner l'option.
    // Un clic en dehors referme simplement la liste (consomme, sans declencher
    // l'element masque) — comportement deja attendu par handleClick().
    for (auto& w : m_inputs) {
        if (!w || !w->isOpen()) continue;
        w->handleClick(mouseX, mouseY);
        playClickSound();
        return true;
    }

    for (auto& item : m_items) {
        if (item.contains(mouseX, mouseY) && item.callback) {
            item.callback();
            playClickSound();
            return true;
        }
    }

    for (const auto& shape : m_shapes) {
        if (shape.second->contains(mouseX, mouseY) && shape.second->callback) {
            shape.second->callback();
            playClickSound();
            return true;
        }
    }

    // Widgets fermes : checkbox (toggle), select (ouverture), range (clic =
    // position de la valeur). Chaque handleClick() retourne true si le clic a
    // ete consomme ; le slider ne joue pas de son (playsInteractionSound).
    for (auto& w : m_inputs) {
        if (!w) continue;
        if (w->handleClick(mouseX, mouseY)) {
            if (w->playsInteractionSound()) playClickSound();
            return true;
        }
    }

    return false;
}

Menu::~Menu() {
    clear();
}

void Menu::ensureBackground() {
    if (!m_background) {
        m_background = std::make_unique<Rectangle>(
            m_shaderManager->getShader("shape"),
            static_cast<float>(Constants::Window::WINDOW_WIDTH) / 2.0f,
            static_cast<float>(Constants::Window::WINDOW_HEIGHT) / 2.0f,
            static_cast<float>(Constants::Window::WINDOW_WIDTH),
            static_cast<float>(Constants::Window::WINDOW_HEIGHT),
            Constants::Color::SHADOW_GREY
        );
    }
}

// ── Navigation manette (discrète) ─────────────────────────────────────────

void Menu::rebuildFocusTargets() {
    m_focusTargets.clear();

    // Items : seuls ceux avec un callback sont actionnables (le texte
    // d'aide sans callback est ignoré par la navigation).
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].callback) continue;
        m_focusTargets.push_back({ FocusType::Item, static_cast<int>(i),
            m_items[i].y + m_items[i].height / 2.0f });
    }
    for (size_t i = 0; i < m_inputs.size(); ++i) {
        if (!m_inputs[i]) continue;
        m_focusTargets.push_back({ FocusType::Input, static_cast<int>(i),
            m_inputs[i]->getPosition().y });
    }

    std::sort(m_focusTargets.begin(), m_focusTargets.end(),
        [](const FocusTarget& a, const FocusTarget& b) { return a.centerY < b.centerY; });

    if (m_focusIndex < 0 || m_focusIndex >= static_cast<int>(m_focusTargets.size())) {
        m_focusIndex = 0;
    }
    m_focusDirty = false;
}

void Menu::applyFocusHighlight() {
    // Retire tout survol/focus existant (souris comme manette)
    for (auto& item : m_items) item.isHovered = false;
    for (auto& w : m_inputs) if (w) w->setFocused(false);

    if (m_focusDirty) rebuildFocusTargets();
    if (m_focusIndex < 0 || m_focusIndex >= static_cast<int>(m_focusTargets.size())) return;

    const FocusTarget& t = m_focusTargets[m_focusIndex];
    if (t.type == FocusType::Item) {
        m_items[t.index].isHovered = true;
    } else {
        m_inputs[t.index]->setFocused(true);
    }
}

void Menu::navigate(int direction) {
    if (m_focusDirty) rebuildFocusTargets();
    int n = static_cast<int>(m_focusTargets.size());
    if (n == 0) return;
    m_focusIndex = ((m_focusIndex + direction) % n + n) % n;
    applyFocusHighlight();
}

void Menu::activateSelected() {
    if (m_focusDirty) rebuildFocusTargets();
    if (m_focusIndex < 0 || m_focusIndex >= static_cast<int>(m_focusTargets.size())) return;

    const FocusTarget& t = m_focusTargets[m_focusIndex];
    if (t.type == FocusType::Item) {
        auto& item = m_items[t.index];
        if (item.callback) {
            playClickSound();
            item.callback();
        }
    } else {
        m_inputs[t.index]->activate();
        if (m_inputs[t.index]->playsInteractionSound()) playClickSound();
    }
}

void Menu::adjustSelected(int direction) {
    if (m_focusDirty) rebuildFocusTargets();
    if (m_focusIndex < 0 || m_focusIndex >= static_cast<int>(m_focusTargets.size())) return;

    const FocusTarget& t = m_focusTargets[m_focusIndex];
    if (t.type == FocusType::Input) {
        m_inputs[t.index]->adjust(direction);
    }
}

void Menu::resetFocus() {
    m_focusIndex = 0;
    applyFocusHighlight();
}

void Menu::setFocusIndex(int index) {
    if (m_focusDirty) rebuildFocusTargets();
    const int n = static_cast<int>(m_focusTargets.size());
    if (n == 0) {
        m_focusIndex = 0;
        return;
    }
    // Borne la position restauree a la liste courante (les elements peuvent
    // avoir change entre-temps, ex: rebuildItems d'un menu de bindings).
    m_focusIndex = (index < 0) ? 0 : (index >= n ? n - 1 : index);
    applyFocusHighlight();
}

void Menu::playClickSound() {
    Sound* clickSound = m_soundManager->get("menu_click_sound");
    if (clickSound) {
        clickSound->play();
    }
}

// ── Implémentations MenuInput ────────────────────────────────────────────────
// Chaque sous-classe dessine son widget + ses libellés/valeurs via les helpers
// de rendu texte de Menu, et délègue les interactions au widget sous-jacent.

void MenuRange::draw(Menu& menu) {
    glm::vec2 pos = input->getPosition();
    glm::vec2 size = input->getSize();
    if (!label.empty()) {
        float labelX = pos.x - size.x / 2.0f - 20.0f;
        float valueX = pos.x + size.x / 2.0f + 20.0f;
        menu.drawTextRightAligned(label, labelX, pos.y - size.y / 2.0f, 0, Constants::Color::LINEN);

        // Formatage léger (snprintf sur buffer stack) au lieu d'un
        // std::stringstream alloué + ss.str() à chaque frame.
        char valueBuf[32];
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", input->getValue());
        menu.drawTextLeftAligned(valueBuf, valueX, pos.y - size.y / 2.0f, 0, Constants::Color::LINEN);
    }
    input->draw();
}

bool MenuRange::handleClick(double mouseX, double mouseY) {
    if (!input->isPointInside(mouseX, mouseY)) return false;
    input->update(mouseX, mouseY, true);
    return true;
}

bool MenuRange::isPointInside(double mouseX, double mouseY) const {
    return input->isPointInside(mouseX, mouseY);
}

glm::vec2 MenuRange::getPosition() const { return input->getPosition(); }

void MenuRange::setFocused(bool focused) { input->setFocused(focused); }

void MenuRange::updateDrag(double mouseX, double mouseY, bool pressed) {
    input->update(mouseX, mouseY, pressed);
}

void MenuRange::activate() { input->nudge(1.0f); }

void MenuRange::adjust(int direction) { input->nudge(static_cast<float>(direction)); }

void MenuCheckbox::draw(Menu& menu) {
    glm::vec2 pos = input->getPosition();
    float size = input->getSize();
    if (!label.empty()) {
        float labelX = pos.x - size / 2.0f - 20.0f;
        menu.drawTextRightAligned(label, labelX, pos.y - size / 2.0f, 0, Constants::Color::LINEN);
    }
    input->draw();
}

bool MenuCheckbox::handleClick(double mouseX, double mouseY) {
    if (!input->isPointInside(mouseX, mouseY)) return false;
    input->toggle();
    return true;
}

bool MenuCheckbox::isPointInside(double mouseX, double mouseY) const {
    return input->isPointInside(mouseX, mouseY);
}

glm::vec2 MenuCheckbox::getPosition() const { return input->getPosition(); }

void MenuCheckbox::setFocused(bool focused) { input->setFocused(focused); }

void MenuCheckbox::activate() { input->toggle(); }

void MenuSelect::draw(Menu& menu) {
    glm::vec2 pos = input->getPosition();
    if (!label.empty()) {
        float labelX = pos.x - 20.0f;
        menu.drawTextRightAligned(label, labelX, pos.y - 30.0f, 0, Constants::Color::LINEN, 0.4f);
    }
    input->draw();

    // Valeur selectionnee, toujours visible sur la case fermee
    menu.drawTextCentered(input->getSelectedLabel(), pos.x, pos.y, 0, Constants::Color::LINEN, 0.4f);

    // Libelles des options, seulement si la liste est ouverte
    if (input->isOpen()) {
        const int hovered = input->getHoveredIndex();
        for (size_t i = 0; i < input->getOptionCount(); ++i) {
            glm::vec2 optPos = input->getOptionPosition(i);
            // Libelle sombre sur la rangee survolee (accent TOMATO_JAM)
            const glm::vec3 labelColor = (static_cast<int>(i) == hovered)
                ? Constants::Color::SHADOW_GREY : Constants::Color::LINEN;
            menu.drawTextCentered(input->getOptionLabel(i), optPos.x, optPos.y, 0, labelColor, 0.4f);
        }
    }
}

bool MenuSelect::handleClick(double mouseX, double mouseY) {
    const bool wasOpen = input->isOpen();
    input->handleClick(mouseX, mouseY);
    return wasOpen || input->isOpen();
}

bool MenuSelect::isPointInside(double mouseX, double mouseY) const {
    return input->isPointInside(mouseX, mouseY);
}

glm::vec2 MenuSelect::getPosition() const { return input->getPosition(); }

void MenuSelect::setFocused(bool focused) { input->setFocused(focused); }

void MenuSelect::updateHover(double mouseX, double mouseY) {
    input->updateHover(mouseX, mouseY);
}

void MenuSelect::activate() {
    if (input->isOpen()) input->close(); else input->open();
}

void MenuSelect::adjust(int direction) { input->cycleOption(direction); }

bool MenuSelect::isOpen() const { return input->isOpen(); }
