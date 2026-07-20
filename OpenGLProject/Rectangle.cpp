#include "Rectangle.h"
#include "Shader.h"
#include "SharedQuad.h"

Rectangle::Rectangle(Shader* shader, float x, float y, float width, float height, glm::vec3 color)
    : Shape(shader, x, y, width, height) {
	setColor(color.r, color.g, color.b);
    setupBuffers();
}

Rectangle::~Rectangle() = default;

void Rectangle::draw() {
    m_shader->use();

    // Creer la transformation complète avec votre classe
    Transformation trans;
    trans.translate(m_position)                                  // 1. Position
        .rotate(glm::vec3(0.0f, 0.0f, 1.0f), m_rotation)         // 2. Rotation
        .scale(glm::vec3(m_size.x, m_size.y, 1.0f));             // 3. Taille

    // Envoyer au shader
    m_shader->setTransformation("model", &trans);
    m_shader->setupMatrices2D();
    m_shader->setVec3("color", m_color);

    SharedQuad::draw();
}


void Rectangle::setupBuffers() {
    SharedQuad::init();
}