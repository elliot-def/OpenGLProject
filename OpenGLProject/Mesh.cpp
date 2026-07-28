#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstddef>     // offsetof (sur probe struct, voir setupMesh)
#include <type_traits> // std::is_standard_layout (probe struct)

#include "Mesh.h"
#include "SkinningData.h" // MAX_BONE_INFLUENCE (pour l'attrib bone IDs)


Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, unsigned int attributesMask, const std::vector<unsigned int>& textureIDs) 
    : m_vertices(vertices), m_indices(indices), m_textureIDs(textureIDs) {
    // Rien � faire ici : les IDs seront initialis�s dans load()
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

    m_vao = m_vbo = m_ebo = 0; // R�initialisation des IDs
    m_indexCount = 0;
}

void Mesh::setupMesh(unsigned int attributesMask = 0b0101) {
    m_indexCount = static_cast<GLsizei>(m_indices.size()); // Nombre d'indices

    // Cr�ation des objets OpenGL
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

    // D�finition du layout m�moire pour chaque attribut
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

    // Bone attribs (4 int boneIDs + 5 float weights). Configures UNE SEULE
    // fois dans setupMesh() au lieu d'etre re-injectes dans draw() a chaque
    // frame (~6 appels GL epargnes par draw()). Tous les Vertex portent des
    // zero en skin (voir Vertex.h), donc les mesh non rigges restent en
    // identite cote shader skinned (wsum==0 -> transformation passee outre).
    //
    // NB : Vertex n'est PAS standard-layout (il a des constructeurs), donc
    // offsetof(Vertex, m_boneIDs) est UB et refuse par MSVC. On utilise une
    // probe struct standard-layout dediee qui reflete l'arrangement de la fin
    // de Vertex. Les static_assert cassent la compile si quelqu'un modifie
    // Vertex et casse le stride -- detection immediate du layout drift.
    struct SkinLayoutProbe {
        char  pre[11 * sizeof(float)];       // 3 pos + 3 normal + 3 color + 2 uv
        int   boneIDs[MAX_BONE_INFLUENCE];   // m_boneIDs
        float weights[MAX_BONE_INFLUENCE];   // m_weights
    };
    static_assert(std::is_standard_layout<SkinLayoutProbe>::value,
                  "SkinLayoutProbe doit etre standard-layout pour offsetof");
    static_assert(sizeof(SkinLayoutProbe) == sizeof(Vertex),
                  "Vertex layout drift: skin stride != sizeof(Vertex). "
                  "Reordonnancer m_boneIDs/m_weights dans Vertex.h, ou "
                  "revoir SkinLayoutProbe.");

    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, MAX_BONE_INFLUENCE, GL_INT, sizeof(Vertex),
                           (void*)offsetof(SkinLayoutProbe, boneIDs));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(SkinLayoutProbe, weights));

    glBindVertexArray(0); // D�bind pour �viter les erreurs plus tard
}

void Mesh::draw() const {
    // On s'assure d'avoir au moins la diffuse
    if (m_textureIDs.size() >= 2) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[0]); // Diffuse toujours au slot 0

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[1]); // Sp�culaire (vraie ou fallback noire) au slot 1
    }
    else if (!m_textureIDs.empty()) {
        // S�curit� si un vieux mesh n'a qu'une seule texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[0]);
    }

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    // Bone attribs (4, 5) configures une seule fois dans setupMesh() et
    // captures dans le VAO state : glBindVertexArray(m_vao) ci-dessus les
    // reactive automatiquement sans avoir a les re-pusher a chaque draw.
    // ~6 appels GL epargnes par draw call * nombre de mesh par frame.

    glBindVertexArray(0);
}