#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "Outlineable.h"

class Shader;
class Texture;
class Renderer;

// ─────────────────────────────────────────────────────────────────────────────
// Cube : descripteur d'instance pour le rendu INSTANCIE (CubeRenderer).
//
// Le Cube ne possede plus de geometrie propre : un cube unitaire (edge=1,
// centre origine) est partage par toutes les instances. La matrice modele
// translate(center) · spin · scale(edge) est portee par instance au GPU.
// Le rendu (glDrawElementsInstanced) est regroupe par shader dans
// CubeRenderer, supprimant le draw call + set d'uniforms par cube.
//
// Collision : le CollisionManager transforme l'AABB LOCALE du cube unitaire
// (±0.5) par getModelMatrix() → AABB world identique a l'ancien code.
// ─────────────────────────────────────────────────────────────────────────────
class Cube : public Outlineable {
public:
    // center : centre world ; edge : arete ; shader : severallights ou lightsource.
    // textures : materiau (vide pour un cube de lumiere). renderer : requis pour le spin.
    Cube(glm::vec3 center, float edge, Shader* shader,
         std::vector<Texture*> textures = {}, Renderer* renderer = nullptr);

    ~Cube() = default;

    void update();
    void setSpin(float speedDegPerSec, const glm::vec3& axis);

    Shader* getShader() const { return m_shader; }
    const std::vector<Texture*>& getTextures() const { return m_textures; }
    glm::vec3 getCenter() const { return m_center; }
    glm::mat4 getModelMatrix() const { return m_modelMatrix; }

    // Outline (silhouette) : conserve pour compatibilite (non utilise par le
    // rendu instancie).
    void setOutlineShader(Shader* s) { m_outlineShader = s; }
    Shader* getOutlineShader() const { return m_outlineShader; }

private:
    void rebuildModelMatrix();

    glm::vec3 m_center;
    float     m_edge;
    glm::mat4 m_modelMatrix = glm::mat4(1.0f);

    Shader* m_shader;
    Shader* m_outlineShader = nullptr;
    std::vector<Texture*> m_textures;
    Renderer* m_renderer;

    // Spin sur place (rotation continue autour du centre).
    float     m_spinSpeedDeg = 0.0f;
    glm::vec3 m_spinAxis     = glm::vec3(0.0f);
    float     m_spinAngle    = 0.0f;
};
