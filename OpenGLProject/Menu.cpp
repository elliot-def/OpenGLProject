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
    m_ranges.push_back(std::make_unique<MenuRange>(label, new RangeInput(shader, x, y, width, height, minValue, maxValue, defaultValue, onValueChanged)));
    m_focusDirty = true;
}

void Menu::addCheckbox(const std::string& label, float x, float y, float size,
    bool defaultValue, std::function<void(bool)> onValueChanged) {
    Shader* shader = m_shaderManager->getShader("shape");
    m_checkboxes.push_back(std::make_unique<MenuCheckbox>(label, new CheckboxInput(shader, x, y, size, defaultValue, onValueChanged)));
    m_focusDirty = true;
}

void Menu::addSelect(const std::string& label, float x, float y, float width, float height,
    std::vector<std::string> options, int defaultIndex,
    std::function<void(int)> onValueChanged) {
    Shader* shader = m_shaderManager->getShader("shape");
    m_selects.push_back(std::make_unique<MenuSelect>(label, new SelectInput(shader, x, y, width, height, std::move(options), defaultIndex, onValueChanged)));
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

    // Dessiner les sliders (label a gauche + widget)
    for (const auto& range : m_ranges) {
        if (!range || !range->input) continue;
        glm::vec2 pos = range->input->getPosition();
        glm::vec2 size = range->input->getSize();
        if (!range->label.empty()) {
            float labelX = pos.x - size.x / 2.0f - 20.0f;
            float valueX = pos.x + size.x / 2.0f + 20.0f;
            //float labelX = pos.x - size.x - 20.0f;
            drawTextRightAligned(range->label, labelX, pos.y - range->input->getSize().y / 2.0f, 0, Constants::Color::LINEN);

            // Formatage léger (snprintf sur buffer stack) au lieu d'un
            // std::stringstream alloué + ss.str() à chaque frame.
            char valueBuf[32];
            snprintf(valueBuf, sizeof(valueBuf), "%.2f", range->input->getValue());

            drawTextLeftAligned(valueBuf, valueX, pos.y - range->input->getSize().y / 2.0f, 0, Constants::Color::LINEN);
        }
        range->input->draw();
    }

    // Dessiner les checkbox (label a gauche + widget)
    for (const auto& checkbox : m_checkboxes) {
        if (!checkbox || !checkbox->input) continue;
        glm::vec2 pos = checkbox->input->getPosition();
        float size = checkbox->input->getSize();
        if (!checkbox->label.empty()) {
            float labelX = pos.x - size / 2.0f - 20.0f;
            // float labelX = pos.x - size - 20.0f;
            drawTextRightAligned(checkbox->label, labelX, pos.y - size / 2.0f, 0, Constants::Color::LINEN);
        }
        checkbox->input->draw();
    }

    // Dessiner les select (label a gauche, valeur selectionnee, options si ouvert)
    for (const auto& select : m_selects) {
        if (!select || !select->input) continue;
        glm::vec2 pos = select->input->getPosition();
        if (!select->label.empty()) {
            float labelX = pos.x - 20.0f;
            drawTextRightAligned(select->label, labelX, pos.y - 30.0f, 0, Constants::Color::LINEN, 0.4f);
        }
        select->input->draw();

        // Valeur selectionnee, toujours visible sur la case fermee
        drawTextCentered(select->input->getSelectedLabel(), pos.x, pos.y, 0, Constants::Color::LINEN, 0.4f);

        // Libelles des options, seulement si la liste est ouverte
        if (select->input->isOpen()) {
            for (size_t i = 0; i < select->input->getOptionCount(); ++i) {
                glm::vec2 optPos = select->input->getOptionPosition(i);
                drawTextCentered(select->input->getOptionLabel(i), optPos.x, optPos.y, 0, Constants::Color::LINEN, 0.4f);
            }
        }
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

    for (auto& item : m_items) {
        if (item.contains(mouseX, mouseY) && item.callback) {
            item.callback();
            Sound* clickSound = m_soundManager->get("menu_click_sound");
            if (clickSound) {
                clickSound->play();
            }
            return true;
        }
    }

    for (const auto& shape : m_shapes) {
        if (shape.second->contains(mouseX, mouseY) && shape.second->callback) {
            shape.second->callback();
            Sound* clickSound = m_soundManager->get("menu_click_sound");
            if (clickSound) {
                clickSound->play();
            }
            return true;
        }
    }

    // Checkbox : un clic dedans bascule son etat (le callback est appele dans toggle())
    for (auto& checkbox : m_checkboxes) {
        if (!checkbox || !checkbox->input) continue;
        if (checkbox->input->isPointInside(mouseX, mouseY)) {
            checkbox->input->toggle();
            Sound* clickSound = m_soundManager->get("menu_click_sound");
            if (clickSound) {
                clickSound->play();
            }
            return true;
        }
    }

    // Select : ouvre/ferme la liste ou selectionne une option (handleClick gere tout, y compris le clic en dehors qui referme)
    for (auto& select : m_selects) {
        if (!select || !select->input) continue;
        bool wasOpen = select->input->isOpen();
        select->input->handleClick(mouseX, mouseY);
        if (wasOpen || select->input->isOpen()) {
            Sound* clickSound = m_soundManager->get("menu_click_sound");
            if (clickSound) {
                clickSound->play();
            }
            return true;
        }
    }

    // Range : un simple clic (sans drag) positionne aussi la valeur - le drag continu passe par updateDrag()
    for (auto& range : m_ranges) {
        if (!range || !range->input) continue;
        if (range->input->isPointInside(mouseX, mouseY)) {
            range->input->update(mouseX, mouseY, true);
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
    for (size_t i = 0; i < m_checkboxes.size(); ++i) {
        if (!m_checkboxes[i] || !m_checkboxes[i]->input) continue;
        m_focusTargets.push_back({ FocusType::Checkbox, static_cast<int>(i),
            m_checkboxes[i]->input->getPosition().y });
    }
    for (size_t i = 0; i < m_selects.size(); ++i) {
        if (!m_selects[i] || !m_selects[i]->input) continue;
        m_focusTargets.push_back({ FocusType::Select, static_cast<int>(i),
            m_selects[i]->input->getPosition().y });
    }
    for (size_t i = 0; i < m_ranges.size(); ++i) {
        if (!m_ranges[i] || !m_ranges[i]->input) continue;
        m_focusTargets.push_back({ FocusType::Range, static_cast<int>(i),
            m_ranges[i]->input->getPosition().y });
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
    for (auto& cb : m_checkboxes) if (cb && cb->input) cb->input->setFocused(false);
    for (auto& sel : m_selects) if (sel && sel->input) sel->input->setFocused(false);
    for (auto& rg : m_ranges) if (rg && rg->input) rg->input->setFocused(false);

    if (m_focusDirty) rebuildFocusTargets();
    if (m_focusIndex < 0 || m_focusIndex >= static_cast<int>(m_focusTargets.size())) return;

    const FocusTarget& t = m_focusTargets[m_focusIndex];
    switch (t.type) {
    case FocusType::Item:     m_items[t.index].isHovered = true; break;
    case FocusType::Checkbox: m_checkboxes[t.index]->input->setFocused(true); break;
    case FocusType::Select:   m_selects[t.index]->input->setFocused(true); break;
    case FocusType::Range:    m_ranges[t.index]->input->setFocused(true); break;
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
    switch (t.type) {
    case FocusType::Item: {
        auto& item = m_items[t.index];
        if (item.callback) {
            playClickSound();
            item.callback();
        }
        break;
    }
    case FocusType::Checkbox: {
        m_checkboxes[t.index]->input->toggle();
        playClickSound();
        break;
    }
    case FocusType::Select: {
        // Ouvre/ferme la liste ; l'option se change via adjustSelected (gauche/droite)
        SelectInput* sel = m_selects[t.index]->input;
        if (sel->isOpen()) sel->close(); else sel->open();
        playClickSound();
        break;
    }
    case FocusType::Range: {
        // Le slider se règle via gauche/droite ; A le pousse d'un cran à droite.
        m_ranges[t.index]->input->nudge(1.0f);
        break;
    }
    }
}

void Menu::adjustSelected(int direction) {
    if (m_focusDirty) rebuildFocusTargets();
    if (m_focusIndex < 0 || m_focusIndex >= static_cast<int>(m_focusTargets.size())) return;

    const FocusTarget& t = m_focusTargets[m_focusIndex];
    if (t.type == FocusType::Range) {
        m_ranges[t.index]->input->nudge(static_cast<float>(direction));
    }
    else if (t.type == FocusType::Select) {
        m_selects[t.index]->input->cycleOption(direction);
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
