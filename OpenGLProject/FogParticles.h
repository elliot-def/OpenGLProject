#pragma once

#include <random>
#include <vector>
#include <glm/glm.hpp>

class Shader;

// ─────────────────────────────────────────────────────────────────────────────
// FogParticles : brume volumetrique en particules billboard.
//
// ~300 sprites autour du joueur (rayon kRadius, hauteur kMaxHeight), chacun
// avec sa taille, son opacite et son gris. Texture procedurale : atlas 2x2 de
// "bouffees" faites de cercles blancs/gris de tailles et de transparences
// differentes. Chaque particule monte lentement, apparait en fondu, puis
// disparait en fondu avant de renaitre ailleurs. Dessin trie de l'arriere vers
// l'avant, blending alpha, ecriture de profondeur desactivee.
// ─────────────────────────────────────────────────────────────────────────────
class FogParticles {
public:
    explicit FogParticles(Shader* shader);
    ~FogParticles();
    FogParticles(const FogParticles&) = delete;
    FogParticles& operator=(const FogParticles&) = delete;

    void update(float dt, const glm::vec3& cameraPos);
    void draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

private:
    struct Particle {
        glm::vec3 pos{ 0.0f };
        glm::vec3 vel{ 0.0f };     // montee lente + derive horizontale
        float size = 2.0f;         // cote du billboard (m)
        float alpha = 0.1f;        // opacite max
        float grey = 0.9f;         // teinte (0.7 gris .. 1 blanc)
        float life = 0.0f;         // temps ecoule
        float maxLife = 10.0f;
        int   cell = 0;            // cellule de l'atlas (0..3)
    };

    void respawn(Particle& p, const glm::vec3& cam, bool randomLife);
    void createTexture();
    float rnd01() { return (m_rng() >> 8) / 16777216.0f; }
    float rnd(float a, float b) { return a + (b - a) * rnd01(); }

    static constexpr int   kCount     = 300;
    static constexpr float kRadius    = 24.0f;   // rayon du volume autour du joueur (m)
    static constexpr float kMaxHeight = 3.5f;    // hauteur d'apparition (m)

    Shader* m_shader = nullptr;
    std::vector<Particle> m_particles;
    std::mt19937 m_rng{ 20260101u };
    bool m_initialized = false;

    unsigned int m_vao = 0, m_vbo = 0, m_ebo = 0, m_texture = 0;
    std::vector<float> m_vertices;   // 4 sommets x 9 floats par particule
};
