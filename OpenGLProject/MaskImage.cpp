#include "MaskImage.h"
#include "SharedQuad.h"
#include "Shader.h"
#include "ShaderType.h"
#include "Image.h"

#include "constants/window.h"

#include <glad/glad.h>  // GL_TRUE/GL_FALSE/glDepthMask - requise par l'outline pass

MaskImage::MaskImage(Shader* shader, const std::string& texturePath, float x, float y, float width, float height)
    : Image(shader, texturePath, x, y, width, height), m_color(1.0f, 1.0f, 1.0f) {
    setupBuffers();
}

void MaskImage::draw(glm::vec3 color) {
    if (m_textureID == 0) return;

    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(Constants::Window::WINDOW_WIDTH),
        static_cast<float>(Constants::Window::WINDOW_HEIGHT), 0.0f,
        -1.0f, 1.0f
    );

    // ── OUTLINE PASS ─────────────────────────────────────────────────────────────
    if (m_outlineEnabled && m_outlineShader) {
        Outline::draw2D(m_outlineShader, m_outlineColor, m_outlineThickness,
                        m_position, m_size, m_rotation, projection);
    }

    m_shader->use();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    model = glm::translate(model, glm::vec3(m_size.x/2, m_size.y/2, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(m_size.x, m_size.y, 1.0f));

    ensureUniformLocations();
    m_shader->setMat4(m_uniformLocations.model, model);
    m_shader->setMat4(m_uniformLocations.projection, projection);
    m_shader->setFloat(m_uniformLocations.opacity, m_opacity);

    if (m_shader->getType() == ShaderType::Mask) {
        m_shader->setVec3(m_uniformLocations.color, color);

    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    m_shader->setInt(m_uniformLocations.image, 0);

    SharedQuad::draw();

    glDisable(GL_BLEND);
}