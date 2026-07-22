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

#include "cstdio"

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
    m_ranges.push_back(new MenuRange(label, new RangeInput(shader, x, y, width, height, minValue, maxValue, defaultValue, onValueChanged)));
}

void Menu::addCheckbox(const std::string& label, float x, float y, float size,
    bool defaultValue, std::function<void(bool)> onValueChanged) {
    Shader* shader = m_shaderManager->getShader("shape");
    m_checkboxes.push_back(new MenuCheckbox(label, new CheckboxInput(shader, x, y, size, defaultValue, onValueChanged)));
}

void Menu::addSelect(const std::string& label, float x, float y, float width, float height,
    std::vector<std::string> options, int defaultIndex,
    std::function<void(int)> onValueChanged) {
    Shader* shader = m_shaderManager->getShader("shape");
    m_selects.push_back(new MenuSelect(label, new SelectInput(shader, x, y, width, height, std::move(options), defaultIndex, onValueChanged)));
}

void Menu::draw() {
	// Draw background
    Rectangle background = Rectangle(m_shaderManager->getShader("shape"), Constants::WINDOW_WIDTH / 2, Constants::WINDOW_HEIGHT / 2, Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT, Colors::SHADOW_GREY);
    background.draw();
    // Dessiner le titre si présent
    if (!m_title.empty()) {
        drawTextCentered(m_title, m_titleX, m_titleY, 1, Colors::LINEN);
    }

    // Dessiner les items du menu
    for (const auto& item : m_items) {
               
        // Texte centré
        float centerX = item.x + item.width / 2.0f;
        float centerY = item.y + item.height / 2.0f;

        if (item.isHovered) {
            drawTextCentered(item.text, centerX, centerY, 0, Colors::TOMATO_JAM);
        }
        else {
            drawTextCentered(item.text, centerX, centerY, 0, Colors::LINEN);

        }
    }
    for (const auto& shape : m_shapes) {
        shape.second->shape->draw();
    }

    // Dessiner les sliders (label a gauche + widget)
    for (const auto* range : m_ranges) {
        glm::vec2 pos = range->input->getPosition();
        glm::vec2 size = range->input->getSize();
        if (!range->label.empty()) {
            std::stringstream ss;

            float labelX = pos.x - size.x / 2.0f - 20.0f;
            float valueX = pos.x + size.x / 2.0f + 20.0f;
            //float labelX = pos.x - size.x - 20.0f;
            drawTextRightAligned(range->label, labelX, pos.y - range->input->getSize().y / 2.0f, 0, Colors::LINEN);

            float val = range->input->getValue();
            ss << std::fixed << std::setprecision(2) << val;

            drawTextLeftAligned(ss.str(), valueX, pos.y - range->input->getSize().y / 2.0f, 0, Colors::LINEN);
        }
        range->input->draw();
    }

    // Dessiner les checkbox (label a gauche + widget)
    for (const auto* checkbox : m_checkboxes) {
        glm::vec2 pos = checkbox->input->getPosition();
        float size = checkbox->input->getSize();
        if (!checkbox->label.empty()) {
            float labelX = pos.x - size / 2.0f - 20.0f;
            // float labelX = pos.x - size - 20.0f;
            drawTextRightAligned(checkbox->label, labelX, pos.y - size / 2.0f, 0, Colors::LINEN);
        }
        checkbox->input->draw();
    }

    // Dessiner les select (label a gauche, valeur selectionnee, options si ouvert)
    for (const auto* select : m_selects) {
        glm::vec2 pos = select->input->getPosition();
        if (!select->label.empty()) {
            float labelX = pos.x - 20.0f;
            drawTextRightAligned(select->label, labelX, pos.y - 30.0f, 0, Colors::LINEN, 0.4f);
        }
        select->input->draw();

        // Valeur selectionnee, toujours visible sur la case fermee
        drawTextCentered(select->input->getSelectedLabel(), pos.x, pos.y, 0, Colors::LINEN, 0.4f);

        // Libelles des options, seulement si la liste est ouverte
        if (select->input->isOpen()) {
            for (size_t i = 0; i < select->input->getOptionCount(); ++i) {
                glm::vec2 optPos = select->input->getOptionPosition(i);
                drawTextCentered(select->input->getOptionLabel(i), optPos.x, optPos.y, 0, Colors::LINEN, 0.4f);
            }
        }
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
    for (auto* checkbox : m_checkboxes) {
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
    for (auto* select : m_selects) {
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
    for (auto* range : m_ranges) {
        if (range->input->isPointInside(mouseX, mouseY)) {
            range->input->update(mouseX, mouseY, true);
            return true;
        }
    }

    return false;
}
