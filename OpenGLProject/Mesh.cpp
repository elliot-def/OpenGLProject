#include <glad/glad.h>
#include <GLFW/glfw3.h>

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
    if (m_culledEbo) glDeleteBuffers(1, &m_culledEbo);

    m_vao = m_vbo = m_ebo = m_culledEbo = 0;
    m_indexCount = 0;
    m_culledIndexCount = 0;
}

void Mesh::setCulledIndices(const std::vector<unsigned int>& culledIndices) {
    m_culledIndices = culledIndices;
    m_culledIndicesSet = true; // même vide : drawCulled() ne doit RIEN dessiner
    // L'EBO dédié sera (ré)créé au prochain drawCulled() dans le contexte courant.
    if (m_culledEbo) {
        glDeleteBuffers(1, &m_culledEbo);
        m_culledEbo = 0;
    }
    m_culledIndexCount = 0;
}

void Mesh::drawCulled() const {
    if (!m_culledIndicesSet) {
        // Jamais de sous-ensemble défini : comportement identique à draw()
        draw();
        return;
    }
    if (m_culledIndices.empty()) {
        // Sous-ensemble défini mais vide : mesh entièrement masqué (ex: torse/
        // tête en vue 1P). Ne rien dessiner — NE PAS retomber sur draw().
        return;
    }

    if (!m_culledEbo) {
        // Création paresseuse de l'EBO filtré (contexte GL courant)
        glGenBuffers(1, &m_culledEbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_culledEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     m_culledIndices.size() * sizeof(unsigned int),
                     m_culledIndices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        m_culledIndexCount = static_cast<int>(m_culledIndices.size());
    }

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
    // L'EBO fait partie de l'état du VAO : on bind le sous-ensemble, on dessine,
    // puis on remet l'EBO principal pour ne pas perturber les draw() ultérieurs.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_culledEbo);
    glDrawElements(GL_TRIANGLES, m_culledIndexCount, GL_UNSIGNED_INT, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

    glBindVertexArray(0);
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

void Mesh::setupMesh(unsigned int attributesMask) {
    m_indexCount = static_cast<GLsizei>(m_indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // ── Layout interleve COMPACT (opti 7) ─────────────────────────────────
    // Avant : on uploadait sizeof(Vertex) = 76 octets par sommet, meme pour un
    // Cube (POSITION|COLOR = 24 o) ou un mesh statique (POSITION|NORMAL|
    // TEXCOORD = 32 o). On ne copie plus que les attributs actifs du masque,
    // dans l'ordre canonique : position, normale, couleur, UV, boneIDs, poids.
    // Le stride et les offsets par attribut sont calcules une seule fois ici.
    unsigned int offsets[6] = { 0, 0, 0, 0, 0, 0 };
    unsigned int stride = 0;
    auto addAttr = [&](unsigned int location, unsigned int sizeBytes) {
        offsets[location] = stride;
        stride += sizeBytes;
    };
    if (attributesMask & (unsigned int)VertexAttribute::POSITION) addAttr(0, 3 * sizeof(float));
    if (attributesMask & (unsigned int)VertexAttribute::NORMAL)   addAttr(1, 3 * sizeof(float));
    if (attributesMask & (unsigned int)VertexAttribute::COLOR)    addAttr(2, 3 * sizeof(float));
    if (attributesMask & (unsigned int)VertexAttribute::TEXCOORD) addAttr(3, 2 * sizeof(float));
    if (attributesMask & (unsigned int)VertexAttribute::SKINNING) {
        addAttr(4, MAX_BONE_INFLUENCE * sizeof(int));
        addAttr(5, MAX_BONE_INFLUENCE * sizeof(float));
    }

    // Buffer compact : les champs bone/weights ne sont PAS envoyes quand
    // SKINNING n'est pas dans le masque (gain ~3x sur les meshes non-skinnes).
    std::vector<unsigned char> packed;
    packed.reserve(m_vertices.size() * stride);
    for (const Vertex& v : m_vertices) {
        if (attributesMask & (unsigned int)VertexAttribute::POSITION) {
            const float f[3] = { v.getX(), v.getY(), v.getZ() };
            packed.insert(packed.end(), reinterpret_cast<const unsigned char*>(f),
                          reinterpret_cast<const unsigned char*>(f) + sizeof(f));
        }
        if (attributesMask & (unsigned int)VertexAttribute::NORMAL) {
            const float f[3] = { v.getNX(), v.getNY(), v.getNZ() };
            packed.insert(packed.end(), reinterpret_cast<const unsigned char*>(f),
                          reinterpret_cast<const unsigned char*>(f) + sizeof(f));
        }
        if (attributesMask & (unsigned int)VertexAttribute::COLOR) {
            const float f[3] = { v.getR(), v.getG(), v.getB() };
            packed.insert(packed.end(), reinterpret_cast<const unsigned char*>(f),
                          reinterpret_cast<const unsigned char*>(f) + sizeof(f));
        }
        if (attributesMask & (unsigned int)VertexAttribute::TEXCOORD) {
            const float f[2] = { v.getS(), v.getT() };
            packed.insert(packed.end(), reinterpret_cast<const unsigned char*>(f),
                          reinterpret_cast<const unsigned char*>(f) + sizeof(f));
        }
        if (attributesMask & (unsigned int)VertexAttribute::SKINNING) {
            const int ids[MAX_BONE_INFLUENCE] = { v.m_boneIDs[0], v.m_boneIDs[1], v.m_boneIDs[2], v.m_boneIDs[3] };
            packed.insert(packed.end(), reinterpret_cast<const unsigned char*>(ids),
                          reinterpret_cast<const unsigned char*>(ids) + sizeof(ids));
            const float w[MAX_BONE_INFLUENCE] = { v.m_weights[0], v.m_weights[1], v.m_weights[2], v.m_weights[3] };
            packed.insert(packed.end(), reinterpret_cast<const unsigned char*>(w),
                          reinterpret_cast<const unsigned char*>(w) + sizeof(w));
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, packed.size(),
                 packed.empty() ? nullptr : packed.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);

    if (attributesMask & (unsigned int)VertexAttribute::POSITION) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)offsets[0]);
    }

    if (attributesMask & (unsigned int)VertexAttribute::NORMAL) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)offsets[1]);
    }

    if (attributesMask & (unsigned int)VertexAttribute::COLOR) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)offsets[2]);
    }

    if (attributesMask & (unsigned int)VertexAttribute::TEXCOORD) {
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)offsets[3]);
    }

    if (attributesMask & (unsigned int)VertexAttribute::SKINNING) {
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, MAX_BONE_INFLUENCE, GL_INT, stride, (void*)(size_t)offsets[4]);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)offsets[5]);
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
