#include "Menu.h"
#include "TextRenderer.h"
#include "ShaderManager.h"
#include "MenuManager.h"
#include "TextureManager.h"
#include "Rectangle.h"
#include "Triangle.h"

void Menu::drawTextCentered(const std::string& text, float centerX, float centerY, int textRendererIndex, glm::vec3 color, float scale) {
    float textWidth = m_textRenderers->at(textRendererIndex)->getTextWidth(text, scale);
    float textHeight = m_textRenderers->at(textRendererIndex)->getTextHeight(text, scale); // Ajoutez cette méthode si elle n'existe pas
    
    float startX = centerX - textWidth / 2.0f;
    float startY = centerY; // Centrage vertical basé sur la hauteur réelle
    
    m_textRenderers->at(textRendererIndex)->renderText(text, startX, startY, scale, color.r, color.g, color.b);
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
        shape.second->draw();
    }
}