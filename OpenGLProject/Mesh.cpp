#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstddef>
#include <type_traits>

#include "Mesh.h"
#include "SkinningData.h"


Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, unsigned int attributesMask, const std::vector<unsigned int>& textureIDs) 
    : m_vertices(vertices), m_indices(indices), m_textureIDs(textureIDs), m_attributeMask(attributesMask) {
    setupMesh(attributesMask);

    for (const auto& v : m_vertices) {
        glm::vec3 p = v.getPositions();
        m_aabbMin = glm::min(m_aabbMin, p);
        m_aabbMax = glm::max(m_aabbMax, p);
    }
}

Mesh::~Mesh() {
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);

    m_vao = m_vbo = m_ebo = 0;
    m_indexCount = 0;
}

// ---------------------------------------------------------------------------
// Recrée les VAO/VBO/EBO dans le contexte GL courant.
// Appelé après un chargement sur un contexte partagé (thread), car les VAO
// ne sont pas partagés entre contextes OpenGL (contrairement aux VBO/EBO/textures).
// Les anciens handles du contexte partagé seront nettoyés à sa destruction.
// ---------------------------------------------------------------------------

void Mesh::reloadGPUResources() {
    setupMesh(m_attributeMask);
}

void Mesh::setupMesh(unsigned int attributesMask = 0b0101) {
    m_indexCount = static_cast<GLsizei>(m_indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);

    if (attributesMask & (unsigned int)VertexAttribute::POSITION) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    }

    if (attributesMask & (unsigned int)VertexAttribute::NORMAL) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)(3 * sizeof(float)));
    }

    if (attributesMask & (unsigned int)VertexAttribute::COLOR) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)(6 * sizeof(float)));
    }

    if (attributesMask & (unsigned int)VertexAttribute::TEXCOORD) {
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)(9 * sizeof(float)));
    }

    if (attributesMask & (unsigned int)VertexAttribute::SKINNING) {
        struct SkinLayoutProbe {
            char  pre[11 * sizeof(float)];
            int   boneIDs[MAX_BONE_INFLUENCE];
            float weights[MAX_BONE_INFLUENCE];
        };
        static_assert(std::is_standard_layout<SkinLayoutProbe>::value,
                      "SkinLayoutProbe doit etre standard-layout pour offsetof");
        static_assert(sizeof(SkinLayoutProbe) == sizeof(Vertex),
                      "Vertex layout drift: skin stride != sizeof(Vertex)");

        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, MAX_BONE_INFLUENCE, GL_INT, sizeof(Vertex),
                               (void*)offsetof(SkinLayoutProbe, boneIDs));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(SkinLayoutProbe, weights));
    }

    glBindVertexArray(0);
}

void Mesh::draw() const {
    if (m_textureIDs.size() >= 2) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[0]);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[1]);
    }
    else if (!m_textureIDs.empty()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[0]);
    }

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}
