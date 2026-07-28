#include "TextRenderer.h"
#include "ShaderManager.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>
#include <fstream>
#include <vector>
#include <iostream>

#include <glad/glad.h>  // GL_FALSE / glUniformMatrix4fv / glDrawArrays / etc.

TextRenderer::TextRenderer(ShaderManager* shaderManager)
    : m_shaderManager(shaderManager), m_fontSize(48.0f), m_VAO(0), m_VBO(0), m_shaderProgram(0),
    m_screenWidth(Constants::WINDOW_WIDTH), m_screenHeight(Constants::WINDOW_HEIGHT) {

    // Cr�er le shader
	m_shaderProgram = m_shaderManager->getShader("text")->getID();

    // Configuration du VAO/VBO
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

TextRenderer::~TextRenderer() {
    for (auto& pair : m_characters) {
        glDeleteTextures(1, &pair.second.textureID);
    }
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteProgram(m_shaderProgram);
}
void TextRenderer::setScreenSize(int width, int height) {
    m_screenWidth = width;
    m_screenHeight = height;
}

bool TextRenderer::loadFont(const std::string& fontPath, float fontSize) {
    m_fontSize = fontSize;

    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erreur: Impossible d'ouvrir " << fontPath << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return false;

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, buffer.data(), 0)) return false;

    float scale = stbtt_ScaleForPixelHeight(&font, fontSize);

    // D�sactiver l'alignement par d�faut d'OpenGL
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++) {
        int width, height, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, scale,
            c, &width, &height, &xoff, &yoff);

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0,
            GL_RED, GL_UNSIGNED_BYTE, bitmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int advance, leftBearing;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &leftBearing);

        Character character = {
            texture,
            width, height,
            xoff, yoff,
            (unsigned int)(advance * scale)
        };
        m_characters[c] = character;

        stbtt_FreeBitmap(bitmap, nullptr);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    std::cout << "Police chargee: " << fontPath << std::endl;
    return true;
}

float TextRenderer::getTextWidth(const std::string& text, float scale) {
    float width = 0.0f;
    for (char c : text) {
        if (m_characters.find(c) != m_characters.end()) {
            Character ch = m_characters[c];
            width += ch.advance * scale;
        }
    }
    return width;
}

float TextRenderer::getTextHeight(const std::string& text, float scale) {
    if (text.empty()) return m_fontSize * scale;

    float maxBearingY = 0.0f;  // Point le plus haut
    float minY = 0.0f;          // Point le plus bas

    for (char c : text) {
        if (m_characters.find(c) != m_characters.end()) {
            Character ch = m_characters[c];

            // Point le plus haut du caract�re
            float top = ch.bearingY * scale;
            maxBearingY = std::max(maxBearingY, top);

            // Point le plus bas du caract�re
            float bottom = (ch.bearingY - ch.sizeY) * scale;
            minY = std::min(minY, bottom);
        }
    }

    // Hauteur totale = distance entre le point le plus haut et le plus bas
    return maxBearingY - minY;
}
void TextRenderer::renderText(const std::string& text, float x, float y,
    float scale, float r, float g, float b) {

    glUseProgram(m_shaderProgram);

    // Matrice de projection orthographique
    float projection[16] = {
        2.0f / m_screenWidth, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / m_screenHeight, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };

    GLint projLoc = glGetUniformLocation(m_shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

    GLint colorLoc = glGetUniformLocation(m_shaderProgram, "textColor");
    glUniform3f(colorLoc, r, g, b);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_VAO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float currentX = x;
    for (char c : text) {
        if (m_characters.find(c) == m_characters.end()) continue;
        Character ch = m_characters[c];

        // Position correcte avec stb_truetype
        float xpos = currentX + ch.bearingX * scale;
        float ypos = y + ch.bearingY * scale;  // AJOUT, pas soustraction

        float w = ch.sizeX * scale;
        float h = ch.sizeY * scale;

        float vertices[6][4] = {
            { xpos,     ypos,     0.0f, 0.0f },
            { xpos + w, ypos,     1.0f, 0.0f },
            { xpos,     ypos + h, 0.0f, 1.0f },
            { xpos + w, ypos,     1.0f, 0.0f },
            { xpos + w, ypos + h, 1.0f, 1.0f },
            { xpos,     ypos + h, 0.0f, 1.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        currentX += ch.advance * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}