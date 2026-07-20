#include "Image.h"
#include "ImageLoader.h"
#include "Mesh.h"
#include "Shader.h"
#include "Vertex.h"
#include "constants.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <iostream>

// --- Constructeur fichier ---
Image::Image(Shader* shader, const std::string& imagePath,
    float x, float y, float width, float height, float opacity)
    : Shape(shader, x, y, width, height),
    m_opacity(opacity),
    m_ownsTexture(true)
{
    loadTexture(imagePath);
    setupBuffers();
}

// --- Constructeur ID externe ---
Image::Image(Shader* shader, unsigned int textureID,
    float x, float y, float width, float height, float opacity)
    : Shape(shader, x, y, width, height),
    m_textureID(textureID),
    m_opacity(opacity),
    m_ownsTexture(false)
{
    setupBuffers();
}

Image::~Image() {
    if (m_ownsTexture && m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
    }
    // m_mesh est détruit par Shape::~Shape()
}

void Image::loadTexture(const std::string& imagePath) {
    // wrapMode = GL_CLAMP_TO_EDGE (comportement d'origine : un sprite 2D ne doit pas boucler sur les bords)
    // requestedChannels = 0 : on garde la detection automatique du format (RGBA/RGB/etc.), comme avant.
    // Le ImageLoader leve une exception en cas d'echec, message equivalent a celui d'origine.
    ImageLoader loader(imagePath, /*flipVertically=*/true, /*generateMipmaps=*/true, GL_CLAMP_TO_EDGE, /*requestedChannels=*/0);
    m_textureID = loader.releaseTextureID(); // Image reste proprietaire de la texture (m_ownsTexture gere sa destruction)
}

void Image::setupBuffers() {
    std::vector<Vertex> vertices = {
        {{ 0.0f, 0.0f, 0.0f }, { 0,0,1 }, { 1,1,1 }, { 0.0f, 1.0f }},
        {{ 1.0f, 0.0f, 0.0f }, { 0,0,1 }, { 1,1,1 }, { 1.0f, 1.0f }},
        {{ 1.0f, 1.0f, 0.0f }, { 0,0,1 }, { 1,1,1 }, { 1.0f, 0.0f }},
        {{ 0.0f, 1.0f, 0.0f }, { 0,0,1 }, { 1,1,1 }, { 0.0f, 0.0f }},
    };
    std::vector<unsigned int> indices = { 0, 1, 2, 0, 2, 3 };

    m_mesh = new Mesh(vertices, indices, 0b1111, { m_textureID });
}

void Image::draw() {
    if (m_textureID == 0) return;

    m_shader->use();

    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(Constants::WINDOW_WIDTH),
        static_cast<float>(Constants::WINDOW_HEIGHT), 0.0f,
        -1.0f, 1.0f
    );

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    model = glm::translate(model, glm::vec3(m_size.x * 0.5f, m_size.y * 0.5f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::translate(model, glm::vec3(-m_size.x * 0.5f, -m_size.y * 0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(m_size.x, m_size.y, 1.0f));

    m_shader->setMat4("model", model);
    m_shader->setMat4("projection", projection);
    m_shader->setFloat("opacity", m_opacity);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_mesh->draw();

    glDisable(GL_BLEND);
}

void Image::drawGradient(glm::vec3 color) {
    if (m_textureID == 0) return;

    m_shader->use();

    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(Constants::WINDOW_WIDTH),
        static_cast<float>(Constants::WINDOW_HEIGHT), 0.0f,
        -1.0f, 1.0f
    );

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    model = glm::translate(model, glm::vec3(m_size.x * 0.5f, m_size.y * 0.5f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::translate(model, glm::vec3(-m_size.x * 0.5f, -m_size.y * 0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(m_size.x, m_size.y, 1.0f));

    m_shader->setMat4("model", model);
    m_shader->setMat4("projection", projection);
    m_shader->setFloat("opacity", m_opacity);

    if(m_shader->getName() == "image/gradient") {
        m_shader->setVec3("customColor", color);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_mesh->draw();

    glDisable(GL_BLEND);
}
