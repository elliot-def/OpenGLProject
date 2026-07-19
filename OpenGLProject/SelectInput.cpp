#include "SelectInput.h"
#include "Rectangle.h"
#include "constants.h"

#include <algorithm>

SelectInput::SelectInput(CursorManager* cursorManager, Shader* shader, float x, float y, float width, float height,
    std::vector<std::string> options, int defaultIndex,
    std::function<void(int)> onValueChanged)
    : m_cursorManager(cursorManager), m_shader(shader), m_options(std::move(options)),
    m_selectedIndex(std::clamp(defaultIndex, 0, static_cast<int>(m_options.size()) - 1)),
    m_position(x, y), m_size(width, height),
    m_onValueChanged(onValueChanged)
{
    m_box = new Rectangle(shader, x, y, width, height, glm::vec3(0.3f, 0.3f, 0.3f));

    // Une rangee par option, empilee juste en dessous de la case principale
    for (size_t i = 0; i < m_options.size(); ++i) {
        glm::vec2 pos = getOptionPosition(i);
        m_optionBoxes.push_back(new Rectangle(shader, pos.x, pos.y, width, height, Colors::SHADOW_GREY));
    }
}

SelectInput::~SelectInput() {
    delete m_box;
    for (Rectangle* r : m_optionBoxes) {
        delete r;
    }
}

glm::vec2 SelectInput::getOptionPosition(size_t i) const {
    // Rangee i sous la case principale (i = 0 juste en dessous)
    return glm::vec2(m_position.x, m_position.y + m_size.y * static_cast<float>(i + 1));
}

bool SelectInput::isPointInsideBox(double px, double py, glm::vec2 center) const {
    return px >= center.x - m_size.x / 2.0 && px <= center.x + m_size.x / 2.0 &&
        py >= center.y - m_size.y / 2.0 && py <= center.y + m_size.y / 2.0;
}

void SelectInput::handleClick(double mouseX, double mouseY) {
    if (!m_isOpen) {
        // Ferme : un clic sur la case l'ouvre
        if (isPointInsideBox(mouseX, mouseY, m_position)) {
            m_isOpen = true;
        }
        return;
    }

    // Ouvert : un clic sur une option la selectionne et referme la liste
    for (size_t i = 0; i < m_optionBoxes.size(); ++i) {
        if (isPointInsideBox(mouseX, mouseY, getOptionPosition(i))) {
            m_selectedIndex = static_cast<int>(i);
            m_isOpen = false;
            if (m_onValueChanged) {
                m_onValueChanged(m_selectedIndex);
            }
            return;
        }
    }

    // Clic en dehors de toute option : on referme simplement la liste
    m_isOpen = false;
}

void SelectInput::draw() {
    m_box->draw();

    if (m_isOpen) {
        for (Rectangle* r : m_optionBoxes) {
            r->draw();
        }
    }
}
