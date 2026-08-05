#include "TextRenderer.h"
#include "ShaderManager.h"

#include "constants/window.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>
#include <fstream>
#include <vector>
#include <iostream>

#include <glad/glad.h>  // GL_FALSE / glUniformMatrix4fv / glDrawArrays / etc.

// Taille de l'atlas de glyphes : une seule texture pour toute la police
// (jusqu'a Gnocchi a 282px). Bordure de 2px pour eviter le bleeding du
// filtrage LINEAIRE entre glyphes voisins.
static constexpr int ATLAS_WIDTH = 4096;
static constexpr int ATLAS_HEIGHT = 2048;
static constexpr int ATLAS_PADDING = 2;

TextRenderer::TextRenderer(ShaderManager* shaderManager)
    : m_shaderManager(shaderManager), m_fontSize(48.0f), m_VAO(0), m_VBO(0), m_shaderProgram(0),
    m_screenWidth(Constants::Window::WINDOW_WIDTH), m_screenHeight(Constants::Window::WINDOW_HEIGHT) {

    // Creer le shader
    m_shaderProgram = m_shaderManager->getShader("text")->getID();

    // Locations cachees UNE SEULE FOIS ici : plus de glGetUniformLocation a
    // chaque frame dans le hot path (opti rendu texte).
    m_projLoc = glGetUniformLocation(m_shaderProgram, "projection");

    // Configuration du VAO/VBO : attribut 0 = <pos2, uv2>, attribut 1 = <color3>
    // (7 floats par sommet — la couleur par sommet permet le batching de toute
    // la frame en un seul draw call malgre des couleurs de texte differentes).
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 7, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(4 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

TextRenderer::~TextRenderer() {
    if (m_atlasTexture)
        glDeleteTextures(1, &m_atlasTexture);
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

    // ── Atlas unique pour les 128 premiers caracteres (stbtt_Pack) ─────────
    // Avant : 128 textures + 1 draw call + 1 glBufferSubData par glyphe.
    // Apres : 1 texture, et 1 seul draw call pour toute la frame.
    std::vector<unsigned char> atlasData(ATLAS_WIDTH * ATLAS_HEIGHT, 0);

    stbtt_pack_context packCtx;
    if (!stbtt_PackBegin(&packCtx, atlasData.data(), ATLAS_WIDTH, ATLAS_HEIGHT,
                         ATLAS_WIDTH, ATLAS_PADDING, nullptr)) {
        std::cerr << "Erreur: echec stbtt_PackBegin pour " << fontPath << std::endl;
        return false;
    }

    stbtt_packedchar chardata[128];
    if (!stbtt_PackFontRange(&packCtx, buffer.data(), 0, fontSize, 0, 128, chardata)) {
        std::cerr << "Erreur: echec stbtt_PackFontRange pour " << fontPath
                  << " (atlas trop petit ?)" << std::endl;
        stbtt_PackEnd(&packCtx);
        return false;
    }
    stbtt_PackEnd(&packCtx);

    // Upload de l'atlas (format GL_RED, comme les anciennes textures de glyphes)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &m_atlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlasData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // Construire la table des caracteres (UV + metriques depuis stbtt_packedchar)
    m_characters.clear();
    for (unsigned char c = 0; c < 128; c++) {
        const stbtt_packedchar& pc = chardata[c];
        Character character;
        // Inset d'un demi-texel : evite le bleeding du filtre LINEAIRE
        // entre deux glyphes voisins de l'atlas.
        character.u0 = (pc.x0 + 0.5f) / ATLAS_WIDTH;
        character.v0 = (pc.y0 + 0.5f) / ATLAS_HEIGHT;
        character.u1 = (pc.x1 - 0.5f) / ATLAS_WIDTH;
        character.v1 = (pc.y1 - 0.5f) / ATLAS_HEIGHT;
        character.sizeX = pc.x1 - pc.x0;
        character.sizeY = pc.y1 - pc.y0;
        character.bearingX = static_cast<int>(pc.xoff);
        character.bearingY = static_cast<int>(pc.yoff);
        character.advance = static_cast<unsigned int>(pc.xadvance);
        m_characters[c] = character;
    }

    std::cout << "Police chargee: " << fontPath
              << " (atlas " << ATLAS_WIDTH << "x" << ATLAS_HEIGHT << ")" << std::endl;
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

            // Point le plus haut du caractere
            float top = ch.bearingY * scale;
            maxBearingY = std::max(maxBearingY, top);

            // Point le plus bas du caractere
            float bottom = (ch.bearingY - ch.sizeY) * scale;
            minY = std::min(minY, bottom);
        }
    }

    // Hauteur totale = distance entre le point le plus haut et le plus bas
    return maxBearingY - minY;
}

void TextRenderer::beginFrame() {
    m_batchVertices.clear();
    m_batchVertexCount = 0;
}

void TextRenderer::flush() {
    if (m_batchVertexCount == 0) return;

    // Matrice de projection orthographique (constante pour une taille d'ecran)
    float projection[16] = {
        2.0f / m_screenWidth, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / m_screenHeight, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };

    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_projLoc, 1, GL_FALSE, projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);

    // Le texte est un overlay : on sauvegarde puis restaure les etats GL
    // autour du draw (blend + depth) pour ne ni dependre ni impacter le rendu
    // qui suit (meme comportement que l'ancien HUD qui les manipulait autour
    // du texte).
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    // Orphaning : glBufferData avec la taille exacte du batch evite les stalls
    // (le driver peut recycler l'ancien buffer sans attendre le GPU).
    glBufferData(GL_ARRAY_BUFFER, m_batchVertices.size() * sizeof(float),
                 m_batchVertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, m_batchVertexCount);

    if (depthEnabled) glEnable(GL_DEPTH_TEST);
    if (!blendEnabled) glDisable(GL_BLEND);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    m_batchVertices.clear();
    m_batchVertexCount = 0;
}

void TextRenderer::renderText(const std::string& text, float x, float y,
    float scale, float r, float g, float b) {

    // Aucun appel GL ici : on empile les quads dans le batch de la frame.
    // Le flush() de fin de frame enverra tout d'un coup (1 draw call).
    m_batchVertices.reserve(m_batchVertices.size() + text.size() * 6 * 7);

    float currentX = x;
    for (char c : text) {
        auto it = m_characters.find(c);
        if (it == m_characters.end()) continue;
        const Character& ch = it->second;

        // Position correcte avec stb_truetype (meme convention que l'ancien code)
        float xpos = currentX + ch.bearingX * scale;
        float ypos = y + ch.bearingY * scale;

        float w = ch.sizeX * scale;
        float h = ch.sizeY * scale;

        const float quad[6][7] = {
            { xpos,     ypos,     ch.u0, ch.v0, r, g, b },
            { xpos + w, ypos,     ch.u1, ch.v0, r, g, b },
            { xpos,     ypos + h, ch.u0, ch.v1, r, g, b },
            { xpos + w, ypos,     ch.u1, ch.v0, r, g, b },
            { xpos + w, ypos + h, ch.u1, ch.v1, r, g, b },
            { xpos,     ypos + h, ch.u0, ch.v1, r, g, b }
        };
        m_batchVertices.insert(m_batchVertices.end(), &quad[0][0], &quad[0][0] + 6 * 7);
        m_batchVertexCount += 6;

        currentX += ch.advance * scale;
    }
}
