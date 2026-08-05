#pragma once

#include <glm/glm.hpp>

class Shader;
class Mesh;
class Model;

// ─────────────────────────────────────────────────────────────────────────────
// Outlineable : mixin d'etat pour l'outline (silhouette haute couleur)
//
// Permet a n'importe quelle entite (Shape, Cube, Entity) d'avoir un outline
// activable par-instance sans dupliquer les setters/getters dans chaque
// hierarchie. Pattern : heritage multiple `class Foo : public Outlineable`.
//
// Les fonctions du namespace Outline centralisent le rendu d'outline
// (implementations dans OutlinePass.cpp) pour eviter la duplication dans
// ~6 draw() differents.
// ─────────────────────────────────────────────────────────────────────────────

struct Outlineable {
    bool        m_outlineEnabled   = false;
    glm::vec3   m_outlineColor     = glm::vec3(1.0f);
    float       m_outlineThickness = 0.05f;

    void setOutlineEnabled(bool enabled)             { m_outlineEnabled = enabled; }
    void setOutlineColor(const glm::vec3& color)     { m_outlineColor   = color; }
    void setOutlineThickness(float thickness)       { m_outlineThickness = glm::max(0.0f, thickness); }

    bool              isOutlineEnabled() const { return m_outlineEnabled; }
    const glm::vec3&  getOutlineColor()  const { return m_outlineColor; }
    float             getOutlineThickness() const { return m_outlineThickness; }
};

namespace Outline {

// Outline 2D pour les formes utilisant SharedQuad (Rectangle, Image, MaskImage).
void draw2D(Shader* outlineShader,
            const glm::vec3& outlineColor, float outlineThickness,
            const glm::vec3& position, const glm::vec2& size, float rotation,
            const glm::mat4& projection);

// Outline 3D pour les objets avec Mesh (Cube, Triangle).
void draw3DMesh(Shader* outlineShader, Shader* mainShader,
                const glm::vec3& outlineColor, float outlineThickness,
                const glm::mat4& modelMatrix, Mesh* mesh);

// Outline 3D pour ModelEntity (Model::draw).
void draw3DModel(Shader* outlineShader, Shader* mainShader,
                 const glm::vec3& outlineColor, float outlineThickness,
                 const glm::mat4& modelMatrix, Model& model);

} // namespace Outline
