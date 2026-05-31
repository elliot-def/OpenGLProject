#pragma once

#include "Shape.h"

class MaskShape : public Shape {
public:
    MaskShape(Shader* shader, float x, float y, float width, float height);
    ~MaskShape() = default;

    void setTexture(unsigned int textureID);
    void setOpacity(float opacity);
    void setupBuffers() override;
    void draw() override;

private:
    unsigned int m_textureID = 0;
    float m_opacity = 1.0f;
};