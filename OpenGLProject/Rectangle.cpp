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
    // ── OUTLINE PASS ─────────────────────────────────────────────────────────────
    if (m_outlineEnabled && m_outlineShader) {
        m_outlineShader->use();
        Transformation outline_trans;
        glm::vec2 outline_size = m_size * (1.0f + m_outlineThickness);
        outline_trans.translate(m_position)
            .rotate(glm::vec3(0.0f, 0.0f, 1.0f), m_rotation)
            .scale(glm::vec3(outline_size.x, outline_size.y, 1.0f));
        m_outlineShader->setTransformation("uModel", &outline_trans);
        m_outlineShader->setupMatrices2D();
        m_outlineShader->setMat4("uView", glm::mat4(1.0f));
        m_outlineShader->setVec3("uOutlineColor", m_outlineColor);

        glDepthMask(GL_FALSE);
        SharedQuad::draw();
        glDepthMask(GL_TRUE);
    }

    m_shader->use();

    // Creer la transformation compl�te avec votre classe
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