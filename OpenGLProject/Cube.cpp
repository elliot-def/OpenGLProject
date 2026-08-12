#include "Cube.h"

#include <cmath>

#include "Renderer.h"

#include <glm/gtc/matrix_transform.hpp>

Cube::Cube(glm::vec3 center, float edge, Shader* shader,
           std::vector<Texture*> textures, Renderer* renderer)
    : m_center(center), m_edge(edge), m_shader(shader),
      m_textures(std::move(textures)), m_renderer(renderer) {
    rebuildModelMatrix();
}

void Cube::setSpin(float speedDegPerSec, const glm::vec3& axis) {
    m_spinSpeedDeg = speedDegPerSec;
    m_spinAxis = axis;
}

void Cube::update() {
    if (m_spinSpeedDeg != 0.0f && m_renderer) {
        m_spinAngle = std::fmod(m_spinAngle + m_spinSpeedDeg * m_renderer->getDeltaTime(), 360.0f);
        rebuildModelMatrix();
    }
}

// Matrice modele : translate(center) · spin · scale(edge). Le cube unitaire est
// centre a l'origine, donc la rotation (spin) s'applique autour de son propre
// centre avant la translation.
void Cube::rebuildModelMatrix() {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_center);
    if (m_spinAngle != 0.0f && glm::dot(m_spinAxis, m_spinAxis) > 0.0f) {
        model = model * glm::rotate(glm::mat4(1.0f), glm::radians(m_spinAngle), m_spinAxis);
    }
    model = model * glm::scale(glm::mat4(1.0f), glm::vec3(m_edge, m_edge, m_edge));
    m_modelMatrix = model;
}
