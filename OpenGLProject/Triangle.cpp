#include "Triangle.h"
#include "Shader.h"
#include "Mesh.h"
#include "Vertex.h"

#include "constants/window.h"

Triangle::Triangle(Shader* shader, float x, float y, float width, float height, glm::vec3 color)
    : Shape(shader, x, y, width, height) {
    setColor(color.r, color.g, color.b);
    setupBuffers();
}

Triangle::~Triangle() = default;


void Triangle::draw() {
    // ── OUTLINE PASS ─────────────────────────────────────────────────────────────
    if (m_outlineEnabled && m_outlineShader) {
        // Triangle utilise une transformation 2D (position/size/rotation)
        Transformation trans;
        trans.translate(m_position)
            .rotate(glm::vec3(0.0f, 0.0f, 1.0f), m_rotation)
            .scale(glm::vec3(m_size.x, m_size.y, 1.0f));
        Outline::draw3DMesh(m_outlineShader, m_shader,
                            m_outlineColor, m_outlineThickness,
                            trans.getMatrix(), m_mesh);
    }

    m_shader->use();

    // Creer la transformation complète avec votre classe
    Transformation trans;
    trans.translate(m_position)                                  // 1. Position
        .rotate(glm::vec3(0.0f, 0.0f, 1.0f), m_rotation)         // 2. Rotation
        .scale(glm::vec3(m_size.x, m_size.y, 1.0f));             // 3. Taille

    // Envoyer au shader — locations cachees (resolues une fois) : plus
    // aucun hash/compare de string par frame (pattern LightManager/Spotlight).
    ensureUniformLocations();
    m_shader->setMat4(m_uniformLocations.transform, trans.getMatrix());
    m_shader->setMat4(m_uniformLocations.projection, m_shader->getProjection2D());
    m_shader->setMat4(m_uniformLocations.uProjection, m_shader->getProjection2D());
    m_shader->setVec3(m_uniformLocations.color, m_color);
    if (m_shader->getType() == ShaderType::RoundedTriangle) {
        m_shader->setFloat(m_uniformLocations.radius, 1.0f);
        m_shader->setVec2(m_uniformLocations.resolution, glm::vec2(Constants::Window::WINDOW_WIDTH, Constants::Window::WINDOW_HEIGHT));
    }

    m_mesh->draw();
}


void Triangle::setupBuffers() {
    float expand;

    if (m_shader->getType() == ShaderType::RoundedTriangle) {
		expand = 1.3f;  // triangle agrandi pour compenser les arrondis, ajuster cette valeur selon le rayon utilisé dans le shader
    }
    else {
        expand = 1.0f;  // triangle classique
	}

    auto vertices = {
        // Positions
        Vertex(-0.5f * expand, -0.5f * expand, 0.0f, m_color.r, m_color.g, m_color.b),
        Vertex( 0.5f * expand, -0.5f * expand, 0.0f, m_color.r, m_color.g, m_color.b),
        Vertex( 0.0f,           0.5f * expand, 0.0f, m_color.r, m_color.g, m_color.b),
    };
    
    std::vector<unsigned int> indices = {
        0, 1, 2
    };

    m_mesh = new Mesh(vertices, indices, 0b0101);
}