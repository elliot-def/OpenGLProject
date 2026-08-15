#pragma once
#include <glad/glad.h>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>

class ShaderManager; // forward declaration

struct Character {
    float u0, v0, u1, v1;   // UV du glyphe dans l'atlas
    int sizeX, sizeY;       // taille du bitmap du glyphe (px)
    int bearingX, bearingY; // decalage d'origine (px)
    unsigned int advance;   // avance horizontale (px)
};

class TextRenderer {
private:
    ShaderManager* m_shaderManager;
    // Table fixe des 128 premiers caracteres ASCII (0-127), indexee par
    // (unsigned char) : acces O(1) par caractere dans le hot path du texte
    // (l'ancien std::map faisait O(log n) + pointer-chase par glyphe).
    std::array<Character, 128> m_characters{};
    // Glyphes hors ASCII (points de code >= 128) : icones kenney en zone
    // privee (U+E000..U+E0FF). Remplis par loadFontRange().
    std::unordered_map<unsigned int, Character> m_extraCharacters;
    unsigned int m_VAO, m_VBO;
    unsigned int m_shaderProgram;
    unsigned int m_atlasTexture = 0; // une seule texture pour toute la police
    int m_projLoc = -1;              // location de "projection", cachee une seule fois
    float m_fontSize;
    int m_screenWidth, m_screenHeight;

    // Copie CPU de l'atlas : conservee pour pouvoir ajouter des plages de
    // glyphes (loadFontRange) en re-paquetant tout puis re-uploadant.
    std::vector<unsigned char> m_atlasData;

    // Plages de glyphes chargees (une police + une plage contigue chacune)
    struct LoadedFontRange {
        std::string fontPath;
        float fontSize;
        unsigned int firstCodepoint;
        unsigned int numChars;
    };
    std::vector<LoadedFontRange> m_fontRanges;

    // Lookup d'un caractere : ASCII dans le tableau fixe, sinon dans la map
    // des glyphes supplementaires. Retourne nullptr si inconnu.
    const Character* getCharacter(unsigned int codepoint) const {
        if (codepoint < m_characters.size()) return &m_characters[codepoint];
        auto it = m_extraCharacters.find(codepoint);
        return it != m_extraCharacters.end() ? &it->second : nullptr;
    }

    // Regenere l'atlas a partir de toutes les plages chargees (m_fontRanges)
    // puis re-upload la texture. Appelee par loadFont() / loadFontRange().
    bool rebuildAtlas();

    // Batch de la frame : 7 floats par sommet (x, y, u, v, r, g, b).
    // renderText() n'empile que des quads ici ; flush() envoie tout d'un coup.
    std::vector<float> m_batchVertices;
    GLsizei m_batchVertexCount = 0;

public:
    TextRenderer(ShaderManager* shaderManager);
    ~TextRenderer();

    void setScreenSize(int width, int height);
    bool loadFont(const std::string& fontPath, float fontSize);

    // Ajoute une plage de points de code au meme atlas (ex: icones kenney
    // U+E000..U+E042). Peut etre appele plusieurs fois, sur une ou plusieurs
    // polices : tout est re-paquete ensemble. Les glyphes >= 128 sont
    // accessibles via renderText() avec leur encodage UTF-8.
    bool loadFontRange(const std::string& fontPath, float fontSize,
                       unsigned int firstCodepoint, unsigned int numChars);

    // Debut de frame : vide le batch de glyphes. A appeler avant toute
    // renderText() de la frame (une fois par frame).
    void beginFrame();

    // Dessine TOUS les glyphes accumules depuis beginFrame() en UN SEUL
    // draw call (1 glBindTexture + 1 glBufferData + 1 glDrawArrays).
    // A appeler une fois en fin de frame, apres tout le texte.
    void flush();

    // Ajoute le texte au batch de la frame courante (aucun appel GL ici).
    void renderText(const std::string& text, float x, float y, float scale,
        float r, float g, float b);
    float getTextWidth(const std::string& text, float scale);
    float getTextHeight(const std::string& text, float scale);
};
