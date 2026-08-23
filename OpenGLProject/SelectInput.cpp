#include "SelectInput.h"
#include "Rectangle.h"
#include "constants/color.h"

#include <algorithm>

SelectInput::SelectInput(Shader* shader, float x, float y, float width, float height,
    std::vector<std::string> options, int defaultIndex,
    std::function<void(int)> onValueChanged)
    : m_shader(shader), m_options(std::move(options)),
    m_selectedIndex(std::clamp(defaultIndex, 0, static_cast<int>(m_options.size()) - 1)),
    m_position(x, y), m_size(width, height),
    m_onValueChanged(onValueChanged)
{
    m_box = new Rectangle(shader, x, y, width, height, glm::vec3(0.3f, 0.3f, 0.3f));

    // Fond sombre derriere la liste d'options (visible seulement quand la
    // liste est ouverte) : detache le dropdown du fond de menu. Levegerment
    // plus large que les rangees pour un effet de panneau.
    const float padX = 8.0f, padY = 4.0f;
    const size_t n = m_options.size();
    m_backdrop = new Rectangle(shader, x,
        y + height * (static_cast<float>(n) + 1.0f) / 2.0f,
        width + 2.0f * padX, height * static_cast<float>(n) + 2.0f * padY,
        glm::vec3(0.12f, 0.11f, 0.11f));

    // Une rangee par option, empilee juste en dessous de la case principale
    for (size_t i = 0; i < n; ++i) {
        glm::vec2 pos = getOptionPosition(i);
        m_optionBoxes.push_back(new Rectangle(shader, pos.x, pos.y, width, height, Constants::Color::SHADOW_GREY));
    }
}

SelectInput::~SelectInput() {
    delete m_box;
    delete m_backdrop;
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

bool SelectInput::isPointInside(double px, double py) const {
    if (isPointInsideBox(px, py, m_position)) return true;
    if (!m_isOpen) return false;
    for (size_t i = 0; i < m_optionBoxes.size(); ++i) {
        if (isPointInsideBox(px, py, getOptionPosition(i))) return true;
    }
    return false;
}

void SelectInput::handleClick(double mouseX, double mouseY) {
    if (!m_isOpen) {
        // Ferme : un clic sur la case l'ouvre
        if (isPointInsideBox(mouseX, mouseY, m_position)) {
            m_isOpen = true;
            m_hoveredIndex = -1;
        }
        return;
    }

    // Ouvert : un clic sur une option la selectionne et referme la liste
    for (size_t i = 0; i < m_optionBoxes.size(); ++i) {
        if (isPointInsideBox(mouseX, mouseY, getOptionPosition(i))) {
            m_selectedIndex = static_cast<int>(i);
            m_isOpen = false;
            m_hoveredIndex = -1;
            if (m_onValueChanged) {
                m_onValueChanged(m_selectedIndex);
            }
            return;
        }
    }

    // Clic en dehors de toute option : on referme simplement la liste
    m_isOpen = false;
    m_hoveredIndex = -1;
}

void SelectInput::updateHover(double mouseX, double mouseY) {
    m_hoveredIndex = -1;
    if (!m_isOpen) return;
    for (size_t i = 0; i < m_options.size(); ++i) {
        if (isPointInsideBox(mouseX, mouseY, getOptionPosition(i))) {
            m_hoveredIndex = static_cast<int>(i);
            return;
        }
    }
}

void SelectInput::cycleOption(int direction) {
    int n = static_cast<int>(m_options.size());
    if (n == 0) return;
    m_selectedIndex = ((m_selectedIndex + direction) % n + n) % n;
    if (m_onValueChanged) {
        m_onValueChanged(m_selectedIndex);
    }
}

void SelectInput::draw() {
    // Case en couleur d'accent quand la liste est selectionnee (navigation manette)
    glm::vec3 boxColor = m_focused ? Constants::Color::TOMATO_JAM
                                   : glm::vec3(0.3f, 0.3f, 0.3f);
    m_box->setColor(boxColor.r, boxColor.g, boxColor.b);

    m_box->draw();

    if (m_isOpen) {
        // Fond du dropdown puis rangees ; l'option survolee est mise en
        // evidence avec la couleur d'accent des menus.
        m_backdrop->draw();
        for (size_t i = 0; i < m_optionBoxes.size(); ++i) {
            Rectangle* r = m_optionBoxes[i];
            if (static_cast<int>(i) == m_hoveredIndex) {
                r->setColor(Constants::Color::TOMATO_JAM.r,
                            Constants::Color::TOMATO_JAM.g,
                            Constants::Color::TOMATO_JAM.b);
            } else {
                r->setColor(Constants::Color::SHADOW_GREY.r,
                            Constants::Color::SHADOW_GREY.g,
                            Constants::Color::SHADOW_GREY.b);
            }
            r->draw();
        }
    }
}
