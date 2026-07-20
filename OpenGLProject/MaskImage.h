#pragma once

#include "Image.h"

class MaskImage : public Image {
public:
    MaskImage(Shader* shader, const std::string& texturePath, float x, float y, float width, float height);
    ~MaskImage() = default;

    void draw(glm::vec3 color);
    void draw() override { draw(glm::vec3(1.0f, 1.0f, 1.0f)); }

    glm::vec3 getCurrentColor() const { return m_color; }

protected:
	glm::vec3 m_color;
};