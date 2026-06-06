#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Mesh.h"


Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, unsigned int attributesMask, const std::vector<unsigned int>& textureIDs) 
    : m_vertices(vertices), m_indices(indices), m_textureIDs(textureIDs) {
    // Rien à faire ici : les IDs seront initialisés dans load()
    setupMesh(attributesMask);
}

Mesh::~Mesh() {
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);

    m_vao = m_vbo = m_ebo = 0; // Réinitialisation des IDs
    m_indexCount = 0;
}

void Mesh::setupMesh(unsigned int attributesMask = 0b0101) {
    m_indexCount = static_cast<GLsizei>(m_indices.size()); // Nombre d'indices

    // Création des objets OpenGL
    glGenVertexArrays(1, &m_vao); // VAO
    glGenBuffers(1, &m_vbo);      // VBO
    glGenBuffers(1, &m_ebo);      // EBO

    glBindVertexArray(m_vao); // Bind du VAO

    // Chargement des sommets dans le VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STATIC_DRAW);

    // Chargement des indices dans le EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);

    // Définition du layout mémoire pour chaque attribut
    // Position (x, y, z)
    // Position (layout = 0) : offset = 0
    // Position (toujours actif)
    if (attributesMask & (unsigned int)VertexAttribute::POSITION) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    }

    // Normal (optionnel)
    if (attributesMask & (unsigned int)VertexAttribute::NORMAL) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)(3 * sizeof(float)));
    }

    // Color (optionnel)
    if (attributesMask & (unsigned int)VertexAttribute::COLOR) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)(6 * sizeof(float)));
    }

    // TexCoord (optionnel)
    if (attributesMask & (unsigned int)VertexAttribute::TEXCOORD) {
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)(9 * sizeof(float)));
    }

    glBindVertexArray(0); // Débind pour éviter les erreurs plus tard
}

void Mesh::draw() const {
    // On s'assure d'avoir au moins la diffuse
    if (m_textureIDs.size() >= 2) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[0]); // Diffuse toujours au slot 0

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[1]); // Spéculaire (vraie ou fallback noire) au slot 1
    }
    else if (!m_textureIDs.empty()) {
        // Sécurité si un vieux mesh n'a qu'une seule texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[0]);
    }

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}