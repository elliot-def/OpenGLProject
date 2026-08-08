#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <limits>

#include "Vertex.h"   // Structure d'un sommet (position, couleur, UV)

enum class VertexAttribute {
    POSITION = 1 << 0,  // 0001
    NORMAL   = 1 << 1,  // 0010
    COLOR    = 1 << 2,  // 0100
    TEXCOORD = 1 << 3,  // 1000
    SKINNING = 1 << 4   // 10000 — boneIDs + weights (attribs 4 et 5)
};

/**
 * @class Mesh
 * @brief Represente un maillage 3D pour OpenGL
 *
 * Un maillage contient :
 * - Des sommets (Vertex) : positions, couleurs, coordonnees de texture
 * - Des indices pour former les triangles
 * - Une texture associee
 *
 * La classe gere aussi les objets GPU suivants :
 * - VAO (Vertex Array Object)
 * - VBO (Vertex Buffer Object)
 * - EBO (Element Buffer Object)
 */
class Mesh {
public:
    /**
     * @brief Constructeur par defaut
     *
     * Initialise le pointeur de texture a nullptr.
     * Les IDs VAO, VBO, EBO seront generes dans load().
     */
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, unsigned int attributesMask = 0b0101, const std::vector<unsigned int>& textureIDs = {});

    /**
     * @brief Destructeur
     *
     * Appelle destroy() pour liberer correctement les ressources GPU.
     */
    ~Mesh();

    /**
     * @brief Charge les donnees du maillage dans la memoire GPU
     *
     * @param vertices Vecteur de sommets (positions, couleurs, UV)
     * @param indices Vecteur d'indices pour former les triangles
     * @param texture Pointeur vers la texture associee
     */
    /**
     * @brief Dessine le maillage a l'ecran
     *
     * Bind le VAO et la texture, puis appelle glDrawElements.
     */
    void draw() const;

    // Définit un sous-ensemble d'indices (triangles) à dessiner via
    // drawCulled() — ex: ne garder que les bras+jambes d'un personnage en
    // 1ère personne. Uniquement CPU ici : l'EBO dédié est créé paresseusement
    // au premier drawCulled() dans le contexte GL courant.
    void setCulledIndices(const std::vector<unsigned int>& culledIndices);

    // Dessine uniquement le sous-ensemble d'indices défini par
    // setCulledIndices(). Sans sous-ensemble, se comporte comme draw().
    // Restaure l'EBO principal après le dessin pour ne pas perturber draw().
    void drawCulled() const;

    // Nombre d'indices du sous-ensemble (0 si aucun).
    size_t getCulledIndexCount() const { return m_culledIndices.size(); }

    // Recrée les objets GPU (VAO, VBO, EBO) dans le contexte GL courant.
    // Nécessaire après un chargement sur un contexte partagé (thread) :
    // les VAO ne sont pas partagés entre contextes OpenGL.
    void reloadGPUResources();

	std::vector<Vertex> getVertices() const { return m_vertices; }
    std::vector<Vertex>& getWritableVertices() { return m_vertices; }
    std::vector<unsigned int> getIndices() const { return m_indices; }
    const std::vector<unsigned int>& getTextureIDs() const { return m_textureIDs; }

    glm::vec3 getLocalAABBMin() const { return m_aabbMin; }
    glm::vec3 getLocalAABBMax() const { return m_aabbMax; }

private:
    unsigned int m_attributeMask = 0;  // sauvegardé pour reloadGPUResources()
    unsigned int m_vao = 0;      // Vertex Array Object
    unsigned int m_vbo = 0;      // Vertex Buffer Object
    unsigned int m_ebo = 0;      // Element Buffer Object
    int m_indexCount = 0;        // Nombre d'indices

    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
    std::vector<unsigned int> m_textureIDs;

    // Sous-ensemble d'indices (voir setCulledIndices/drawCulled). L'EBO dédié
    // est créé paresseusement au premier drawCulled() (contexte GL courant).
    // m_culledIndicesSet distingue "jamais défini" (drawCulled = draw()) de
    // "défini mais vide" (drawCulled = ne rien dessiner : mesh entièrement
    // masqué, ex: torse/tête d'un personnage en vue 1P).
    std::vector<unsigned int> m_culledIndices;
    bool m_culledIndicesSet = false;
    mutable unsigned int m_culledEbo = 0;
    mutable int m_culledIndexCount = 0;

    void setupMesh(unsigned int attributesMask);

    glm::vec3 m_aabbMin{ std::numeric_limits<float>::max() };
    glm::vec3 m_aabbMax{ -std::numeric_limits<float>::max() };
};