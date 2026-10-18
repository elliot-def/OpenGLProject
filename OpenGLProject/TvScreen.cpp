#include "TvScreen.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"
#include "ImageLoader.h"
#include "SharedQuad.h"
#include "constants/resource.h"

// Même palette que DVDShape (easter egg du menu principal) : le logo change de
// couleur à chaque rebond.
static const std::vector<glm::vec3> s_dvdColors = {
    { 1.0f, 0.2f, 0.2f },  // Rouge
    { 0.2f, 1.0f, 0.3f },  // Vert
    { 0.2f, 0.4f, 1.0f },  // Bleu
    { 1.0f, 0.85f, 0.1f }, // Jaune
    { 1.0f, 0.2f, 0.9f },  // Rose
    { 0.1f, 0.9f, 0.9f },  // Cyan
    { 1.0f, 0.5f, 0.1f },  // Orange
};

// Ratio du logo DVD (dvd_logo.png, affiché 330×195 px dans le menu).
static constexpr float LOGO_ASPECT = 330.0f / 195.0f;

TvScreen::TvScreen(Shader* maskShader, const glm::mat4& model, float width, float height)
    : m_shader(maskShader), m_model(model), m_width(width), m_height(height) {
    m_center = glm::vec3(model[3]);

    // Taille du logo : ~35% de la largeur de l'écran, proportions du menu.
    m_logoSize = glm::vec2(m_width * 0.35f, m_width * 0.35f / LOGO_ASPECT);

    // Vitesses de rebond : mêmes proportions relatives que DVDShape dans le
    // menu (200×180 px/s pour un logo de 330×195 px).
    m_velocity = glm::vec2(m_logoSize.x * (200.0f / 330.0f),
                           m_logoSize.y * (180.0f / 195.0f));

    // Graine aléatoire une seule fois par processus (même convention que
    // DVDShape dans le menu principal).
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    m_color = s_dvdColors[0];

    // Quad unitaire partagé (init idempotent) + textures.
    SharedQuad::init();

    // Texture blanche 1×1 : le fond d'écran est un quad teinté en noir par le
    // shader "image/mask" (FragColor = vec4(color, alpha)).
    const unsigned char white[4] = { 255, 255, 255, 255 };
    glGenTextures(1, &m_whiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Masque du logo DVD (même texture que le menu principal).
    // flipVertically=true : l'écran est en repère monde (Y vers le haut),
    // contrairement à l'UI 2D du menu (ortho Y vers le bas) — sans le flip,
    // le logo s'affiche à l'envers sur la télé.
    ImageLoader loader(Constants::Resource::TEXTURE_DVD_LOGO,
                       /*flipVertically=*/true, /*generateMipmaps=*/true,
                       GL_CLAMP_TO_EDGE, /*requestedChannels=*/0);
    m_logoTexture = loader.releaseTextureID();
}

TvScreen::~TvScreen() {
    if (m_whiteTexture) glDeleteTextures(1, &m_whiteTexture);
    if (m_logoTexture)  glDeleteTextures(1, &m_logoTexture);
}

void TvScreen::update(float deltaTime, const glm::vec3& cameraPos) {
    // Optimisation : l'animation du logo n'a lieu qu'à proximité de la télé.
    m_active = glm::length(cameraPos - m_center) <= MAX_DRAW_DISTANCE;
    if (!m_active) return;

    // Rebond dans le repère local de l'écran (comme DVDShape dans le menu).
    m_logoPos += m_velocity * deltaTime;

    bool bounced = false;
    const glm::vec2 half = 0.5f * (glm::vec2(m_width, m_height) - m_logoSize);

    if (m_logoPos.x <= -half.x) {
        m_logoPos.x = -half.x;
        m_velocity.x = std::abs(m_velocity.x);
        bounced = true;
    } else if (m_logoPos.x >= half.x) {
        m_logoPos.x = half.x;
        m_velocity.x = -std::abs(m_velocity.x);
        bounced = true;
    }

    if (m_logoPos.y <= -half.y) {
        m_logoPos.y = -half.y;
        m_velocity.y = std::abs(m_velocity.y);
        bounced = true;
    } else if (m_logoPos.y >= half.y) {
        m_logoPos.y = half.y;
        m_velocity.y = -std::abs(m_velocity.y);
        bounced = true;
    }

    if (bounced) pickNewColor();
}

void TvScreen::pickNewColor() {
    int next;
    do {
        next = std::rand() % static_cast<int>(s_dvdColors.size());
    } while (next == m_colorIndex);
    m_colorIndex = next;
    m_color = s_dvdColors[m_colorIndex];
}

void TvScreen::draw(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_shader) return;

    m_shader->use();
    m_shader->setMat4("projection", projection);
    m_shader->setFloat("opacity", 1.0f);
    m_shader->setInt("image", 0);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Fond d'écran : quad plein noir (texture blanche teintée). Toujours
    // dessiné : 1 quad, la télé reste visible de loin.
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader->setVec3("color", glm::vec3(0.02f, 0.02f, 0.02f));
    m_shader->setMat4("model", view * m_model *
        glm::scale(glm::mat4(1.0f), glm::vec3(m_width, m_height, 1.0f)));
    SharedQuad::draw();

    // Easter egg DVD : seulement à portée (cf. update), décalé devant le fond.
    if (m_active) {
        glBindTexture(GL_TEXTURE_2D, m_logoTexture);
        m_shader->setVec3("color", m_color);
        m_shader->setMat4("model", view * m_model *
            glm::translate(glm::mat4(1.0f), glm::vec3(m_logoPos, 0.003f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(m_logoSize, 1.0f)));
        SharedQuad::draw();
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}
