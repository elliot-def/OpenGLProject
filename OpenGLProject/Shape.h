#pragma once

#include <glad/glad.h>                  // ou <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "Outlineable.h"

class Shader;
class Mesh;

class Shape : public Outlineable {
public:
    Shape(Shader* shader, float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f);
    virtual ~Shape();
    virtual void setupBuffers() = 0;
	virtual void draw() = 0;

	bool getIsVisible() { return m_isVisible; }
	bool isPointInside(double px, double py) {
		return (px >= m_position.x - m_size.x / 2 && px <= m_position.x + m_size.x / 2 &&
				py >= m_position.y - m_size.y / 2 && py <= m_position.y + m_size.y / 2);
	}
    glm::vec3 getPosition() { return m_position; }
	glm::vec2 getSize() { return m_size; }

    void setPosition(float x, float y);
    void setSize(float width, float height);
    void setColor(float r, float g, float b);
    void setRotation(float angle);
    void setIsVisible(bool visible) { m_isVisible = visible; }

    // ── Outline (silhouette) ────────────────────────────────────────────────
    // L'outline passe est un rendu du même mesh légèrement agrandi (épaisseur
    // = (1 + m_outlineThickness)), avec un shader plat couleur (uOutlineColor).
    // Le shader est fourni via setOutlineShader() après la construction
    // (typiquement depuis Game::initialize avec `m_shaderManager->getShader("outline")`).
    void setOutlineShader(Shader* s) { m_outlineShader = s; }
    Shader* getOutlineShader() const { return m_outlineShader; }


protected:
    Mesh* m_mesh;
    Shader* m_shader;
    Shader* m_outlineShader = nullptr;  // Optionnel — si null, outline impossible
    glm::vec3 m_position;
    glm::vec3 m_color;
    glm::vec2 m_size;
    float m_rotation;

    bool m_isVisible = true;

    // ── Cache des uniform locations (hot path 2D) ────────────────────────
    // Même pattern que LightManager/Spotlight : les locations sont résolues
    // UNE SEULE FOIS par shape (au premier draw()), puis le hot path ne
    // manipule que des GLint — plus aucun hash/compare de string par frame.
    // Valide car le shader d'un Shape est fixé à la construction et son
    // programme GL est déjà linké.
    struct UniformLocations {
        int model       = -1;
        int transform   = -1;
        int projection  = -1;
        int uProjection = -1;
        int color       = -1;
        int opacity     = -1;
        int image       = -1;
        int radius      = -1;
        int resolution  = -1;
    };
    void ensureUniformLocations();
    UniformLocations m_uniformLocations;
    bool m_uniformLocationsResolved = false;
};

