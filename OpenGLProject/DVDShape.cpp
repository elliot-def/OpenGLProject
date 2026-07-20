#include "DVDShape.h"
#include "Shader.h"

#include "MaskImage.h"
#include "Renderer.h"
#include <cstdlib>
#include <ctime>

const std::vector<glm::vec3> DVDShape::s_colors = {
    { 1.0f, 0.2f, 0.2f },  // Rouge
    { 0.2f, 1.0f, 0.3f },  // Vert
    { 0.2f, 0.4f, 1.0f },  // Bleu
    { 1.0f, 0.85f, 0.1f }, // Jaune
    { 1.0f, 0.2f, 0.9f },  // Rose
    { 0.1f, 0.9f, 0.9f },  // Cyan
    { 1.0f, 0.5f, 0.1f },  // Orange
};

DVDShape::DVDShape(Shader* shader, Renderer* renderer, float startX, float startY, float width, float height, float vx, float vy)
    : MaskImage(shader, "./res/textures/menu/dvd_logo.png", startX, startY, width, height), m_renderer(renderer), m_vx(vx), m_vy(vy), m_colorIndex(0) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    m_color = s_colors[0];
    setIsVisible(false);
    setupBuffers();
}

void DVDShape::setupBuffers() {
	MaskImage::setupBuffers();
}

void DVDShape::update(float screenWidth, float screenHeight) {
    bool bounced = false;

    m_position.x += m_vx * m_renderer->getDeltaTime();
    m_position.y += m_vy * m_renderer->getDeltaTime();

    // Rebond bord gauche / droit
    if (m_position.x <= 0.0f) {
        m_position.x = 0.0f;
        m_vx = std::abs(m_vx);
        bounced = true;
    }
    else if (m_position.x + m_size.x >= screenWidth) {
        m_position.x = screenWidth - m_size.x;
        m_vx = -std::abs(m_vx);
        bounced = true;
    }

    // Rebond bord haut / bas
    if (m_position.y <= 0.0f) {
        m_position.y = 0.0f;
        m_vy = std::abs(m_vy);
        bounced = true;
    }
    else if (m_position.y + m_size.y >= screenHeight) {
        m_position.y = screenHeight - m_size.y;
        m_vy = -std::abs(m_vy);
        bounced = true;
    }

    if (bounced) pickNewColor();
}

void DVDShape::pickNewColor() {
    int next;
    do {
        next = std::rand() % static_cast<int>(s_colors.size());
    } while (next == m_colorIndex);
    m_colorIndex = next;
    m_color = s_colors[m_colorIndex];
}

void DVDShape::draw() {
    if (!m_isVisible) return;
    MaskImage::draw(m_color);
}