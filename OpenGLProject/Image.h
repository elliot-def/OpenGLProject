#pragma once

#include "Shape.h"
#include <string>

class Shader;

class Image : public Shape {
public:
    // Chargement depuis un fichier
    Image(Shader* shader, const std::string& imagePath,
        float x = 0.0f, float y = 0.0f,
        float width = 100.0f, float height = 100.0f,
        float opacity = 1.0f);

    // Depuis un ID OpenGL déjà existant (optionnel, pour interop TextureManager)
    Image(Shader* shader, unsigned int textureID,
        float x = 0.0f, float y = 0.0f,
        float width = 100.0f, float height = 100.0f,
        float opacity = 1.0f);

    ~Image();

    void draw() override;
    void drawGradient(glm::vec3 color);
    void setOpacity(float opacity) { m_opacity = opacity; }
    float getOpacity() const { return m_opacity; }

private:
    void setupBuffers() override;
    void loadTexture(const std::string& imagePath);

    unsigned int m_textureID = 0;
    bool m_ownsTexture = false; // true = on a créé la texture, on doit la détruire
    float m_opacity;
};