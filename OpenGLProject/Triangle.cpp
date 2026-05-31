#include "Triangle.h"
#include "Shader.h"
#include "Mesh.h"
#include "Vertex.h"

Triangle::Triangle(Shader* shader, float x, float y, float width, float height, glm::vec3 color)
    : Shape(shader, x, y, width, height) {
    setColor(color.r, color.g, color.b);
    setupBuffers();
}

Triangle::~Triangle() = default;


void Triangle::draw() {
    m_shader->use();

    // Creer la transformation complète avec votre classe
    Transformation trans;
    trans.translate(m_position)                                  // 1. Position
        .rotate(glm::vec3(0.0f, 0.0f, 1.0f), m_rotation)         // 2. Rotation
        .scale(glm::vec3(m_size.x, m_size.y, 1.0f));             // 3. Taille

    // Envoyer au shader
    m_shader->setTransformation("transform", &trans);
    m_shader->setupMatrices2D();
    m_shader->setVec3("color", m_color);
    if(m_shader->getName() == "shape/roundedTriangle") {
        m_shader->setFloat("radius", 1.0f);
        m_shader->setVec2("resolution", glm::vec2(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT));
    }

    m_mesh->draw();
}


void Triangle::setupBuffers() {
    float expand;

    if (m_shader->getName() == "shape/roundedTriangle") {
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