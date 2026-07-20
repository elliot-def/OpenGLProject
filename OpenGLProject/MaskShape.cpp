#include "MaskShape.h"
#include "SharedQuad.h"
#include "Shader.h"
#include "Transformation.h"

//#include <glad/glad.h>

MaskShape::MaskShape(Shader* shader, float x, float y, float width, float height)
    : Shape(shader, x, y, width, height) {
    setupBuffers();
}

void MaskShape::setTexture(unsigned int textureID) { m_textureID = textureID; }
void MaskShape::setOpacity(float opacity) { m_opacity = opacity; }

void MaskShape::setupBuffers() {
    SharedQuad::init();
}

void MaskShape::draw() {
    // Activer le blending pour respecter l'alpha du masque
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader->use();

    Transformation trans;
    trans.translate(m_position)
        .rotate(glm::vec3(0.0f, 0.0f, 1.0f), m_rotation)
        .scale(glm::vec3(m_size.x, m_size.y, 1.0f));

    m_shader->setTransformation("model", &trans);
    m_shader->setupMatrices2D(); // envoie la projection orthographique
    m_shader->setFloat("opacity", m_opacity);
    m_shader->setVec3("color", m_color);

    // Bind de la texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    m_shader->setInt("image", 0);

    SharedQuad::draw();
}