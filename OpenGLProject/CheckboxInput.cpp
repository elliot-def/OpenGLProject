#include "CheckboxInput.h"
#include "Rectangle.h"
#include "constants.h"

CheckboxInput::CheckboxInput(Shader* shader, float x, float y, float size,
    bool defaultValue, std::function<void(bool)> onValueChanged)
    : m_position(x, y), m_size(size), m_value(defaultValue), m_onValueChanged(onValueChanged)
{
    m_box = new Rectangle(shader, x, y, size, size, glm::vec3(0.3f, 0.3f, 0.3f));
    // La coche est un peu plus petite que le cadre, avec une marge interieure
    m_check = new Rectangle(shader, x, y, size * 0.6f, size * 0.6f, Colors::TROPICAL_TEAL);
}

CheckboxInput::~CheckboxInput() {
    delete m_box;
    delete m_check;
}

void CheckboxInput::toggle() {
    setValue(!m_value);
}

void CheckboxInput::setValue(bool value) {
    m_value = value;
    if (m_onValueChanged) {
        m_onValueChanged(m_value);
    }
}

bool CheckboxInput::isPointInside(double px, double py) const {
    float half = m_size / 2.0f;
    return px >= m_position.x - half && px <= m_position.x + half &&
        py >= m_position.y - half && py <= m_position.y + half;
}

void CheckboxInput::draw() {
    m_box->draw();
    if (m_value) {
        m_check->draw();
    }
}
