#include "Shape.h"
#include "Mesh.h"

Shape::Shape(Shader* shader, float x, float y, float width, float height)
    : m_mesh(nullptr),
      m_shader(shader),
      m_position(x, y, 0.0f),
      m_color(1.0f, 1.0f, 1.0f),
      m_size(width, height),
      m_rotation(0.0f) {
}

Shape::~Shape() {
    delete m_mesh;
}

void Shape::setPosition(float x, float y) {
    m_position = glm::vec3(x, y, 0.0f);
}

void Shape::setSize(float width, float height) {
    m_size = glm::vec2(width, height);
}

void Shape::setColor(float r, float g, float b) {
    m_color = glm::vec3(r, g, b);
}

void Shape::setRotation(float angle) {
    m_rotation = angle;
}