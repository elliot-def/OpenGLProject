#include "Log.h"
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

// Decode un caractere UTF-8 et avance l'iterateur jusqu'au caractere suivant.
// Retourne le point de code (les octets invalides sont ignores).
static unsigned int decodeUtf8(const char*& it, const char* end) {
    const unsigned char c = static_cast<unsigned char>(*it);
    if (c < 0x80) { ++it; return c; }

    unsigned int codepoint = 0;
    int extra = 0;
    if ((c & 0xE0) == 0xC0)      { codepoint = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { codepoint = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { codepoint = c & 0x07; extra = 3; }
    else                         { ++it; return c; } // octet invalide

    ++it;
    for (int i = 0; i < extra && it != end; ++i) {
        codepoint = (codepoint << 6) | (static_cast<unsigned char>(*it) & 0x3F);
        ++it;
    }
    return codepoint;
}

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
    // Une nouvelle police de base (ASCII 0-127) remplace tout l'atlas
    m_fontSize = fontSize;
    m_fontRanges.clear();
    m_extraCharacters.clear();
    m_characters.fill(Character{});
    m_fontRanges.push_back({ fontPath, fontSize, 0, 128 });
    return rebuildAtlas();
}

bool TextRenderer::loadFontRange(const std::string& fontPath, float fontSize,
                                 unsigned int firstCodepoint, unsigned int numChars) {
    // Ajoute une plage de points de code (ex: icones kenney U+E000..U+E0F2)
    // aux plages deja chargees : l'atlas entier est re-paquete ensemble.
    m_fontSize = fontSize;
    m_fontRanges.push_back({ fontPath, fontSize, firstCodepoint, numChars });
    return rebuildAtlas();
}

bool TextRenderer::rebuildAtlas() {
    if (m_fontRanges.empty()) return false;

    // Atlas unique pour toutes les plages de glyphes (stbtt_Pack)
    m_atlasData.assign(ATLAS_WIDTH * ATLAS_HEIGHT, 0);

    stbtt_pack_context packCtx;
    if (!stbtt_PackBegin(&packCtx, m_atlasData.data(), ATLAS_WIDTH, ATLAS_HEIGHT,
                         ATLAS_WIDTH, ATLAS_PADDING, nullptr)) {
        logErr() << "Erreur: echec stbtt_PackBegin (atlas trop petit ?)" << std::endl;
        return false;
    }

    // Les buffers de polices doivent rester vivants pendant le paquetage
    std::vector<std::vector<unsigned char>> fontBuffers;
    fontBuffers.reserve(m_fontRanges.size());

    for (const auto& range : m_fontRanges) {
        std::ifstream file(range.fontPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            logErr() << "Erreur: Impossible d'ouvrir " << range.fontPath << std::endl;
            stbtt_PackEnd(&packCtx);
            return false;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> buffer(size);
        if (!file.read((char*)buffer.data(), size)) {
            stbtt_PackEnd(&packCtx);
            return false;
        }

        std::vector<stbtt_packedchar> chardata(range.numChars);
        if (!stbtt_PackFontRange(&packCtx, buffer.data(), 0, range.fontSize,
                                 range.firstCodepoint, range.numChars, chardata.data())) {
            logErr() << "Erreur: echec stbtt_PackFontRange pour " << range.fontPath
                      << " (atlas trop petit ?)" << std::endl;
            stbtt_PackEnd(&packCtx);
            return false;
        }

        // Construire la table des caracteres (UV + metriques depuis stbtt_packedchar)
        for (unsigned int i = 0; i < range.numChars; ++i) {
            const stbtt_packedchar& pc = chardata[i];
            const unsigned int codepoint = range.firstCodepoint + i;

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

            if (codepoint < m_characters.size()) {
                m_characters[codepoint] = character;
            }
            else {
                m_extraCharacters[codepoint] = character;
            }
        }

        fontBuffers.push_back(std::move(buffer));
    }
    stbtt_PackEnd(&packCtx);

    // Upload de l'atlas (format GL_RED, comme les anciennes textures de glyphes)
    if (m_atlasTexture) glDeleteTextures(1, &m_atlasTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &m_atlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                 GL_RED, GL_UNSIGNED_BYTE, m_atlasData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    logOut() << "Police chargee: " << m_fontRanges.size() << " plage(s) de glyphes"
              << " (atlas " << ATLAS_WIDTH << "x" << ATLAS_HEIGHT << ")" << std::endl;
    return true;
}

float TextRenderer::getTextWidth(const std::string& text, float scale) {
    float width = 0.0f;
    const char* it = text.data();
    const char* end = it + text.size();
    while (it < end) {
        // Decodage UTF-8 : les glyphes hors ASCII (icones kenney en zone
        // privee) sont mesures via m_extraCharacters, le reste est ignore.
        const unsigned int codepoint = decodeUtf8(it, end);
        const Character* ch = getCharacter(codepoint);
        if (!ch) continue;
        width += ch->advance * scale;
    }
    return width;
}

float TextRenderer::getTextHeight(const std::string& text, float scale) {
    if (text.empty()) return m_fontSize * scale;

    float maxBearingY = 0.0f;  // Point le plus haut
    float minY = 0.0f;          // Point le plus bas

    const char* it = text.data();
    const char* end = it + text.size();
    while (it < end) {
        const unsigned int codepoint = decodeUtf8(it, end);
        const Character* ch = getCharacter(codepoint);
        if (!ch) continue;

        // Point le plus haut du caractere
        float top = ch->bearingY * scale;
        maxBearingY = std::max(maxBearingY, top);

        // Point le plus bas du caractere
        float bottom = (ch->bearingY - ch->sizeY) * scale;
        minY = std::min(minY, bottom);
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
    const char* it = text.data();
    const char* end = it + text.size();
    while (it < end) {
        // Decodage UTF-8 : supporte les glyphes hors ASCII (icones kenney
        // U+E000..U+E0FF encodees en 3 octets UTF-8)
        const unsigned int codepoint = decodeUtf8(it, end);
        const Character* ch = getCharacter(codepoint);
        if (!ch) continue;

        // Position correcte avec stb_truetype (meme convention que l'ancien code)
        float xpos = currentX + ch->bearingX * scale;
        float ypos = y + ch->bearingY * scale;

        float w = ch->sizeX * scale;
        float h = ch->sizeY * scale;

        const float quad[6][7] = {
            { xpos,     ypos,     ch->u0, ch->v0, r, g, b },
            { xpos + w, ypos,     ch->u1, ch->v0, r, g, b },
            { xpos,     ypos + h, ch->u0, ch->v1, r, g, b },
            { xpos + w, ypos,     ch->u1, ch->v0, r, g, b },
            { xpos + w, ypos + h, ch->u1, ch->v1, r, g, b },
            { xpos,     ypos + h, ch->u0, ch->v1, r, g, b }
        };
        m_batchVertices.insert(m_batchVertices.end(), &quad[0][0], &quad[0][0] + 6 * 7);
        m_batchVertexCount += 6;

        currentX += ch->advance * scale;
    }
}
