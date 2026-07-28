#pragma once

#include <glm/glm.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Outlineable : mixin d'état pour l'outline (silhouette haute couleur)
//
// Permet à n'importe quelle entité (Shape, Cube, Entity) d'avoir un outline
// activable par-instance sans dupliquer les setters/getters dans chaque
// hiérarchie. Pattern : héritage multiple `class Foo : public Outlineable`.
//
// Trois états : activé, couleur, épaisseur. L'épaisseur est un multiplicateur
// scalaire relatif (1.0 = pas d'expansion ; 1.05 = 5% plus grand que l'original).
// Le rendu lui-même délègue au draw() de l'entité qui appelle l'outline shader
// stocké séparément (m_outlineShader, voir Shape/Entity/Cube).
// ─────────────────────────────────────────────────────────────────────────────

struct Outlineable {
    bool        m_outlineEnabled   = false;
    glm::vec3   m_outlineColor     = glm::vec3(1.0f); // blanc par défaut
    float       m_outlineThickness = 0.05f;           // 5 % plus large que l'original

    void setOutlineEnabled(bool enabled)             { m_outlineEnabled = enabled; }
    void setOutlineColor(const glm::vec3& color)     { m_outlineColor   = color; }
    void setOutlineThickness(float thickness)       { m_outlineThickness = glm::max(0.0f, thickness); }

    bool              isOutlineEnabled() const { return m_outlineEnabled; }
    const glm::vec3&  getOutlineColor()  const { return m_outlineColor; }
    float             getOutlineThickness() const { return m_outlineThickness; }
};
