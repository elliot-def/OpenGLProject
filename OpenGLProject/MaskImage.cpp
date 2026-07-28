#include "MaskImage.h"
#include "SharedQuad.h"
#include "Shader.h"
#include "ShaderType.h"
#include "Image.h"

#include <glad/glad.h>  // GL_TRUE/GL_FALSE/glDepthMask - requise par l'outline pass

MaskImage::MaskImage(Shader* shader, const std::string& texturePath, float x, float y, float width, float height)
    : Image(shader, texturePath, x, y, width, height), m_color(1.0f, 1.0f, 1.0f) {
    setupBuffers();
}

void MaskImage::draw(glm::vec3 color) {
    if (m_textureID == 0) return;

    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(Constants::WINDOW_WIDTH),
        static_cast<float>(Constants::WINDOW_HEIGHT), 0.0f,
        -1.0f, 1.0f
    );

    // ── OUTLINE PASS ─────────────────────────────────────────────────────────────
    if (m_outlineEnabled && m_outlineShader) {
        m_outlineShader->use();
        glm::vec2 outline_size = m_size * (1.0f + m_outlineThickness);
        glm::mat4 outline_model = glm::mat4(1.0f);
        outline_model = glm::translate(outline_model, m_position);
        outline_model = glm::translate(outline_model, glm::vec3(outline_size.x * 0.5f, outline_size.y * 0.5f, 0.0f));
        outline_model = glm::rotate(outline_model, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        outline_model = glm::translate(outline_model, glm::vec3(-outline_size.x * 0.5f, -outline_size.y * 0.5f, 0.0f));
        outline_model = glm::scale(outline_model, glm::vec3(outline_size.x, outline_size.y, 1.0f));
        m_outlineShader->setMat4("uModel", outline_model);
        m_outlineShader->setMat4("uView", glm::mat4(1.0f));
        m_outlineShader->setMat4("uProjection", projection);
        m_outlineShader->setVec3("uOutlineColor", m_outlineColor);

        glDepthMask(GL_FALSE);
        SharedQuad::draw();
        glDepthMask(GL_TRUE);
    }

    m_shader->use();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    model = glm::translate(model, glm::vec3(m_size.x/2, m_size.y/2, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(m_size.x, m_size.y, 1.0f));

    m_shader->setMat4("model", model);
    m_shader->setMat4("projection", projection);
    m_shader->setFloat("opacity", m_opacity);

    if (m_shader->getType() == ShaderType::Mask) {
        m_shader->setVec3("color", color);

    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    m_shader->setInt("image", 0);

    SharedQuad::draw();

    glDisable(GL_BLEND);
}