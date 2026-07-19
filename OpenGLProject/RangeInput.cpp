#include "RangeInput.h"
#include "Rectangle.h"
#include "CursorManager.h"
#include "constants.h"

#include <algorithm>

RangeInput::RangeInput(CursorManager* cursorManager, Shader* shader, float x, float y, float width, float height,
    float minValue, float maxValue, float defaultValue,
    std::function<void(float)> onValueChanged)
    : m_cursorManager(cursorManager),
    m_position(x, y), m_size(width, height),
    m_handleWidth(height * 0.8f), // le curseur est un peu plus large que la piste n'est haute
    m_minValue(minValue), m_maxValue(maxValue),
    m_value(std::clamp(defaultValue, minValue, maxValue)),
    m_onValueChanged(onValueChanged)
{
    // Piste de fond
    m_track = new Rectangle(shader, x, y, width, height, Colors::SHADOW_GREY);

    // Remplissage : sa largeur/position sera recalculee dans updateHandlePosition()
    m_fill = new Rectangle(shader, x, y, width, height, Colors::TROPICAL_TEAL);

    // Curseur
    m_handle = new Rectangle(shader, x, y, m_handleWidth, height * 1.4f, Colors::VANILLA_CUSTARD);

    updateHandlePosition();
}

RangeInput::~RangeInput() {
    delete m_track;
    delete m_fill;
    delete m_handle;
}

void RangeInput::setValue(float value) {
    m_value = std::clamp(value, m_minValue, m_maxValue);
    updateHandlePosition();
}

void RangeInput::setValueFromMouseX(double mouseX) {
    float left = m_position.x - m_size.x / 2.0f;
    float t = static_cast<float>(mouseX - left) / m_size.x;
    t = std::clamp(t, 0.0f, 1.0f);

    m_value = std::round((m_minValue + t * (m_maxValue - m_minValue)) * 100.0f) / 100.0f;
    updateHandlePosition();

    if (m_onValueChanged) {
        m_onValueChanged(m_value);
    }
}

void RangeInput::updateHandlePosition() {
    float t = (m_value - m_minValue) / (m_maxValue - m_minValue);
    t = std::clamp(t, 0.0f, 1.0f);

    float left = m_position.x - m_size.x / 2.0f;
    float handleX = left + t * m_size.x;

    m_handle->setPosition(handleX, m_position.y);

    // Le fill va du bord gauche jusqu'au curseur
    float fillWidth = std::max(t * m_size.x, 1.0f);
    m_fill->setSize(fillWidth, m_size.y);
    m_fill->setPosition(left + fillWidth / 2.0f, m_position.y);
}

bool RangeInput::isPointInside(double px, double py) const {
    // Zone cliquable = toute la piste, un peu elargie verticalement pour faciliter le clic
    float left = m_position.x - m_size.x / 2.0f;
    float right = m_position.x + m_size.x / 2.0f;
    float top = m_position.y - m_size.y * 2.0f;
    float bottom = m_position.y + m_size.y * 2.0f;

    return px >= left && px <= right && py >= top && py <= bottom;
}

void RangeInput::update(double mouseX, double mouseY, bool mousePressed) {
    if (mousePressed) {
        if (!m_isDragging && isPointInside(mouseX, mouseY)) {
            m_isDragging = true;
			
        }
        if (m_isDragging) {
            setValueFromMouseX(mouseX);
        }
    }
    else {
        m_isDragging = false;

    }
}

void RangeInput::draw() {
    m_track->draw();
    m_fill->draw();
    m_handle->draw();
}
