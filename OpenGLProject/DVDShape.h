#pragma once
#include "MaskImage.h"
#include <vector>
#include <glm/glm.hpp>


class Renderer;

class DVDShape : public MaskImage {
public:
    DVDShape(Shader* shader, Renderer* renderer, float startX = 100.0f, float startY = 100.0f, float width = 120.0f, float height = 44.0f, float vx = 2.5f, float vy = 2.0f);

    void setupBuffers() override;
    void draw() override;
    void update(float screenWidth, float screenHeight);
private:
    void pickNewColor();

    Renderer* m_renderer;
    float m_vx, m_vy;
    int m_colorIndex;

    static const std::vector<glm::vec3> s_colors;
};