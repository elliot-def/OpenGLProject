#pragma once
#include <glm/glm.hpp>

/**
 * @class Vertex
 * @brief Représente un sommet 3D avec position, couleur et coordonnées de texture.
 *
 * Chaque vertex contient :
 * - m_x, m_y, m_z : position dans l'espace 3D
 * - m_r, m_g, m_b : couleur (RGB)
 * - m_s, m_t : coordonnées de texture (UV)
 */
class Vertex
{
public:

    /**
    * @brief Constructeur avec vecteurs (position, normal, couleur, UV).
    * Permet d'utiliser des initializers imbriqués comme dans Image.cpp:
    *   Vertex v = {{x,y,z}, {nx,ny,nz}, {r,g,b}, {s,t}};
    */
    Vertex(const glm::vec3& pos, const glm::vec3& normal, const glm::vec3& color, const glm::vec2& tex)
        : m_x(pos.x), m_y(pos.y), m_z(pos.z),
          m_nx(normal.x), m_ny(normal.y), m_nz(normal.z),
          m_r(color.x), m_g(color.y), m_b(color.z),
          m_s(tex.x), m_t(tex.y) {
    }

    /**
     * @brief Constructeur position seulement.
     */
    Vertex(float x, float y, float z)
        : m_x(x), m_y(y), m_z(z),
          m_nx(0.0f), m_ny(0.0f), m_nz(0.0f),
          m_r(0.0f), m_g(0.0f), m_b(0.0f),
          m_s(0.0f), m_t(0.0f) {
    }
    
    /**
     * @brief Constructeur avec position et couleur.
     */
    Vertex(float x, float y, float z, float r, float g, float b)
        : m_x(x), m_y(y), m_z(z),
          m_nx(0.0f), m_ny(0.0f), m_nz(0.0f),
          m_r(r), m_g(g), m_b(b),
          m_s(0.0f), m_t(0.0f) {
    }

    /**
    * @brief Constructeur complet avec normal et UV.
    */
    Vertex(float x, float y, float z, float nx, float ny, float nz, float s, float t)
        : m_x(x), m_y(y), m_z(z),
          m_nx(nx), m_ny(ny), m_nz(nz),
          m_r(0.0f), m_g(0.0f), m_b(0.0f),
          m_s(s), m_t(t) {
    }

    /**
     * @brief Destructeur par défaut.
     */
    ~Vertex() = default;

    // Getters
    float getX() const { return m_x; }
    float getY() const { return m_y; }
    float getZ() const { return m_z; }
    float getNX() const { return m_nx; }
    float getNY() const { return m_ny; }
    float getNZ() const { return m_nz; }
    float getR() const { return m_r; }
    float getG() const { return m_g; }
    float getB() const { return m_b; }
    float getS() const { return m_s; }
    float getT() const { return m_t; }

	glm::vec3 getPositions() const { return glm::vec3(m_x, m_y, m_z); }
	glm::vec3 getNormals() const { return glm::vec3(m_nx, m_ny, m_nz); }
	glm::vec3 getColor() const { return glm::vec3(m_r, m_g, m_b); }
	glm::vec2 getTexCoords() const { return glm::vec2(m_s, m_t); }

private:
    float m_x, m_y, m_z;        ///< Coordonnées du sommet
    float m_nx, m_ny, m_nz;     ///< Normal du sommet (selon la surface à laquelle il appartient)
    float m_r, m_g, m_b;        ///< Couleur RGB
    float m_s, m_t;             ///< Coordonnées de texture UV
};
