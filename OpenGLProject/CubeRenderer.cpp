#include "CubeRenderer.h"

#include "Mesh.h"
#include "Shader.h"
#include "ShaderType.h"
#include "Texture.h"
#include "Camera.h"
#include "LightManager.h"
#include "Vertex.h"

#include <glad/glad.h>

namespace {

// Cube unitaire : edge=1, centré à l'origine (24 sommets pos+normal+UV,
// 36 indices). Position/taille/rotation sont portées par la matrice modèle
// par instance (et non "bakées" dans les sommets, contrairement à l'ancien Cube).
void buildUnitCube(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) {
    const float h = 0.5f;
    vertices = {
        // Face avant (Z+)
        Vertex(-h, -h,  h, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f),
        Vertex( h, -h,  h, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f),
        Vertex( h,  h,  h, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
        Vertex(-h,  h,  h, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f),
        // Face arrière (Z-)
        Vertex(-h, -h, -h, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
        Vertex( h, -h, -h, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex( h,  h, -h, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f),
        Vertex(-h,  h, -h, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f),
        // Face gauche (X-)
        Vertex(-h, -h, -h, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f),
        Vertex(-h, -h,  h, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f),
        Vertex(-h,  h,  h, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
        Vertex(-h,  h, -h, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
        // Face droite (X+)
        Vertex( h, -h, -h, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f),
        Vertex( h, -h,  h, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f),
        Vertex( h,  h,  h, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
        Vertex( h,  h, -h, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
        // Face bas (Y-)
        Vertex(-h, -h, -h, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f),
        Vertex( h, -h, -h, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f),
        Vertex( h, -h,  h, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f),
        Vertex(-h, -h,  h, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f),
        // Face haut (Y+)
        Vertex(-h,  h, -h, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
        Vertex( h,  h, -h, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        Vertex( h,  h,  h, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f),
        Vertex(-h,  h,  h, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f),
    };
    indices = {
        0, 1, 2,   2, 3, 0,
        5, 4, 7,   7, 6, 5,
        8, 9, 10,  10, 11, 8,
        13, 12, 15, 15, 14, 13,
        17, 16, 19, 19, 18, 17,
        20, 21, 22, 22, 23, 20,
    };
}

} // namespace

CubeRenderer::CubeRenderer() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    buildUnitCube(vertices, indices);

    m_unitCube = std::make_unique<Mesh>(
        vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);

    // Attributs par-instance : mat4 model (locations 4-7) + vec3 color (loc 8).
    m_unitCube->setupInstanceAttributes(sizeof(CubeInstance));
}

CubeRenderer::~CubeRenderer() = default;

void CubeRenderer::clear() {
    m_batches.clear();
}

void CubeRenderer::submit(Shader* shader, const std::vector<Texture*>& textures,
                          const glm::mat4& model, const glm::vec3& color) {
    Batch& batch = m_batches[shader];
    if (batch.instances.empty() && !textures.empty()) {
        batch.textures = textures; // matériau du lot (identique entre instances)
    }
    CubeInstance inst;
    inst.model = model;
    inst.color = glm::vec4(color, 1.0f);
    batch.instances.push_back(inst);
}

void CubeRenderer::draw(Camera* camera, LightManager* lightManager) {
    for (auto& [shader, batch] : m_batches) {
        if (batch.instances.empty()) continue;

        shader->use();
        shader->setMat4("view", camera->getViewMatrix());
        shader->setMat4("projection", shader->getProjection());

        if (shader->getType() == ShaderType::SeveralLights) {
            shader->setVec3("viewPos", camera->getPosition());
            for (Texture* texture : batch.textures) {
                if (texture && texture->hasSpecular()) {
                    texture->applyToShader(shader);
                }
            }
            if (lightManager) lightManager->applyToShader(shader);
        }

        m_unitCube->uploadInstanceData(batch.instances.data(),
                                       batch.instances.size(),
                                       sizeof(CubeInstance));
        m_unitCube->drawInstanced(batch.instances.size());
    }
}
