#include "FogParticles.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "Shader.h"

#include <glad/glad.h>

static constexpr int kAtlasCells = 2;     // atlas 2x2
static constexpr int kCellPx = 128;       // taille d'une cellule (px)

FogParticles::FogParticles(Shader* shader) : m_shader(shader) {
    m_particles.resize(kCount);
    m_vertices.resize(static_cast<size_t>(kCount) * 4 * 9);

    createTexture();

    // Indices statiques : 2 triangles par quad.
    std::vector<unsigned int> indices;
    indices.reserve(static_cast<size_t>(kCount) * 6);
    for (unsigned int i = 0; i < static_cast<unsigned int>(kCount); ++i) {
        const unsigned int b = i * 4;
        indices.insert(indices.end(), { b, b + 1, b + 2, b + 2, b + 3, b });
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    const GLsizei stride = 9 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                      // position
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));    // uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));    // couleur
    glBindVertexArray(0);
}

FogParticles::~FogParticles() {
    if (m_texture) glDeleteTextures(1, &m_texture);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

// Atlas procedural : chaque cellule = 7 a 11 cercles flous (rayon, opacite et gris
// differents) composites "over", fondus vers les bords pour eviter les coupures.
void FogParticles::createTexture() {
    const int W = kAtlasCells * kCellPx;
    std::vector<unsigned char> img(static_cast<size_t>(W) * W * 4, 0);
    std::mt19937 rng(777u);
    auto r01 = [&rng]() { return (rng() >> 8) / 16777216.0f; };

    for (int cell = 0; cell < kAtlasCells * kAtlasCells; ++cell) {
        const int ox = (cell % kAtlasCells) * kCellPx;
        const int oy = (cell / kAtlasCells) * kCellPx;
        struct Circle { float cx, cy, r, a, g; };
        std::vector<Circle> circles;
        const int n = 7 + static_cast<int>(r01() * 5.0f);
        for (int i = 0; i < n; ++i) {
            circles.push_back({ kCellPx * (0.25f + 0.5f * r01()), kCellPx * (0.25f + 0.5f * r01()),
                                kCellPx * (0.08f + 0.28f * r01()),       // rayons varies
                                0.18f + 0.55f * r01(),                   // opacites variees
                                0.72f + 0.28f * r01() });                // gris -> blanc
        }
        for (int y = 0; y < kCellPx; ++y) {
            for (int x = 0; x < kCellPx; ++x) {
                float outA = 0.0f, outG = 0.0f;
                for (const Circle& c : circles) {
                    const float d = std::hypot(x - c.cx, y - c.cy) / c.r;
                    if (d >= 1.0f) continue;
                    const float f = 1.0f - d;
                    const float a = c.a * f * f * (3.0f - 2.0f * f);   // bord doux
                    // composition "over"
                    outG = (c.g * a + outG * outA * (1.0f - a)) / std::max(a + outA * (1.0f - a), 1e-4f);
                    outA = a + outA * (1.0f - a);
                }
                // fondu vers les bords de la cellule
                const float ex = std::min(x, kCellPx - 1 - x) / (kCellPx * 0.5f);
                const float ey = std::min(y, kCellPx - 1 - y) / (kCellPx * 0.5f);
                const float edge = std::min(1.0f, std::min(ex, ey) * 2.2f);
                outA *= edge;
                unsigned char* px = &img[(static_cast<size_t>(oy + y) * W + (ox + x)) * 4];
                const unsigned char g = static_cast<unsigned char>(std::clamp(outG, 0.0f, 1.0f) * 255.0f);
                px[0] = px[1] = px[2] = g;
                px[3] = static_cast<unsigned char>(std::clamp(outA, 0.0f, 1.0f) * 255.0f);
            }
        }
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, W, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void FogParticles::respawn(Particle& p, const glm::vec3& cam, bool randomLife) {
    const float ang = rnd(0.0f, 6.2831853f);
    const float r = kRadius * std::sqrt(rnd01());
    p.pos = glm::vec3(cam.x + r * std::cos(ang), rnd(0.05f, kMaxHeight), cam.z + r * std::sin(ang));
    p.vel = glm::vec3(rnd(-0.12f, 0.12f), rnd(0.08f, 0.32f), rnd(-0.12f, 0.12f));   // monte lentement
    p.size = rnd(1.6f, 4.8f);
    p.alpha = rnd(0.05f, 0.20f);
    p.grey = rnd(0.72f, 1.0f);
    p.maxLife = rnd(7.0f, 15.0f);
    p.life = randomLife ? rnd(0.0f, p.maxLife) : 0.0f;
    p.cell = static_cast<int>(rnd01() * 4.0f) & 3;
}

void FogParticles::update(float dt, const glm::vec3& cameraPos) {
    if (!(dt > 0.0f)) dt = 0.0f;
    if (!m_initialized) {
        for (Particle& p : m_particles) respawn(p, cameraPos, true);
        m_initialized = true;
    }
    for (Particle& p : m_particles) {
        p.life += dt;
        p.pos += p.vel * dt;
        const float dx = p.pos.x - cameraPos.x, dz = p.pos.z - cameraPos.z;
        // fin de vie, ou sortie du volume autour du joueur : renait ailleurs
        if (p.life >= p.maxLife || dx * dx + dz * dz > (kRadius * 1.15f) * (kRadius * 1.15f)) {
            respawn(p, cameraPos, false);
        }
    }
}

void FogParticles::draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) {
    if (!m_shader || m_particles.empty()) return;

    const glm::vec3 right(view[0][0], view[1][0], view[2][0]);
    const glm::vec3 up(view[0][1], view[1][1], view[2][1]);

    // Tri arriere -> avant (blending alpha correct).
    std::vector<int> order(m_particles.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return glm::dot(m_particles[a].pos - cameraPos, m_particles[a].pos - cameraPos) >
               glm::dot(m_particles[b].pos - cameraPos, m_particles[b].pos - cameraPos);
    });

    size_t w = 0;
    for (int idx : order) {
        const Particle& p = m_particles[idx];
        const float t = p.life / p.maxLife;
        // apparition rapide (0..15 %), disparition longue (55..100 %)
        const float fadeIn = std::clamp(t / 0.15f, 0.0f, 1.0f);
        const float fadeOut = 1.0f - std::clamp((t - 0.55f) / 0.45f, 0.0f, 1.0f);
        const float a = p.alpha * fadeIn * fadeIn * fadeOut;

        const float u0 = (p.cell % kAtlasCells) / static_cast<float>(kAtlasCells);
        const float v0 = (p.cell / kAtlasCells) / static_cast<float>(kAtlasCells);
        const float du = 1.0f / kAtlasCells;
        const glm::vec3 r = right * (p.size * 0.5f), u = up * (p.size * 0.5f);
        const glm::vec3 corners[4] = { p.pos - r - u, p.pos + r - u, p.pos + r + u, p.pos - r + u };
        const float uvs[4][2] = { { u0, v0 }, { u0 + du, v0 }, { u0 + du, v0 + du }, { u0, v0 + du } };
        for (int k = 0; k < 4; ++k) {
            float* v = &m_vertices[w];
            v[0] = corners[k].x; v[1] = corners[k].y; v[2] = corners[k].z;
            v[3] = uvs[k][0];    v[4] = uvs[k][1];
            v[5] = p.grey; v[6] = p.grey; v[7] = p.grey; v[8] = a;
            w += 9;
        }
    }

    m_shader->use();
    m_shader->setMat4("view", view);
    m_shader->setMat4("projection", projection);
    m_shader->setVec3("viewPos", cameraPos);
    m_shader->setInt("uTex", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(w * sizeof(float)), m_vertices.data());
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_particles.size() * 6), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}
