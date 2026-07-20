#include "MaskImage.h"
#include "SharedQuad.h"
#include "Shader.h"
#include "Image.h"

//#include <glad/glad.h>

MaskImage::MaskImage(Shader* shader, const std::string& texturePath, float x, float y, float width, float height)
    : Image(shader, texturePath, x, y, width, height), m_color(1.0f, 1.0f, 1.0f) {
    setupBuffers();
}

void MaskImage::draw(glm::vec3 color) {
    if (m_textureID == 0) return;

    m_shader->use();

    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(Constants::WINDOW_WIDTH),
        static_cast<float>(Constants::WINDOW_HEIGHT), 0.0f,
        -1.0f, 1.0f
    );

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    model = glm::rotate(model, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(m_size.x, m_size.y, 1.0f));

    m_shader->setMat4("model", model);
    m_shader->setMat4("projection", projection);
    m_shader->setFloat("opacity", m_opacity);

    if (m_shader->getName() == "image/masque") {
        m_shader->setVec3("color", color);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    m_shader->setInt("image", 0);

    SharedQuad::init();

    glDisable(GL_BLEND);
}