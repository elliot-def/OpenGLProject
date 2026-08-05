#include "Cube.h"

#include <cmath>
#include "Vertex.h"   // Structure d'un sommet (position, couleur, texture�)
#include "Mesh.h"     // Classe pour gerer les buffers OpenGL et dessiner
#include "Shader.h"   // Classe pour les shaders OpenGL
#include "ShaderType.h" // Enum class pour identifier le rôle du shader sans string compare
#include "Texture.h"  // Classe pour les textures
#include "Transformation.h" // Classe pour position, rotation et scale
#include "LightSource.h"
#include "Player.h"
#include "LightManager.h"
#include "LightSource.h"
#include "Renderer.h"

#include <glad/glad.h>  // GL_TRUE/GL_FALSE/glDepthMask - requise par l'outline pass

Cube::Cube(glm::vec3 center, float edge, Shader* shader, Player* player)
    : m_center(center), m_edge(edge), m_shader(shader), m_player(player), m_lightManager(nullptr), m_lightSource(nullptr), m_renderer(nullptr) {

}

// Constructeur du cube
Cube::Cube(glm::vec3 center, float edge, Shader* shader, LightSource* lightSource, Player* player)
    : Cube(center, edge, shader, player) {
    m_lightSource = lightSource;

    // Coordonn�es du centre du cube
    float x = center[0];
    float y = center[1];
    float z = center[2];

    // Moiti� de la taille du cube (sert � placer les sommets autour du centre)
    float halfEdge = m_edge / 2.0f;

    // Cr�ation des composants n�cessaires
    m_transformation = std::make_unique<Transformation>(); // Permet de d�placer/faire tourner/agrandir l�objet

    // D�finition des sommets du cube
    // Chaque face a 4 sommets, et comme un cube a 6 faces -> 24 sommets en tout
    // Chaque sommet a : position (x,y,z), normale (ici mise � 0 pour l�instant), coordonn�es UV
    const std::vector<Vertex> vertices = {
        // Face avant (Z+)
        Vertex(x - halfEdge, y - halfEdge, z + halfEdge, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f),
        Vertex(x + halfEdge, y - halfEdge, z + halfEdge, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f),
        Vertex(x + halfEdge, y + halfEdge, z + halfEdge, 0.0f,  0.0f, 1.0f, 1.0f, 1.0f),
        Vertex(x - halfEdge, y + halfEdge, z + halfEdge, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f),

        // Face arri�re (Z-)
        Vertex(x - halfEdge, y - halfEdge, z - halfEdge, 0.0f,  0.0f, -1.0f, 1.0f, 0.0f),
        Vertex(x + halfEdge, y - halfEdge, z - halfEdge, 0.0f,  0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(x + halfEdge, y + halfEdge, z - halfEdge, 0.0f,  0.0f, -1.0f, 0.0f, 1.0f),
        Vertex(x - halfEdge, y + halfEdge, z - halfEdge, 0.0f,  0.0f, -1.0f, 1.0f, 1.0f),

        // Face gauche (X-)
        Vertex(x - halfEdge, y - halfEdge, z - halfEdge, -1.0f,  0.0f, 0.0f, 0.0f, 0.0f),
        Vertex(x - halfEdge, y - halfEdge, z + halfEdge, -1.0f,  0.0f, 0.0f, 1.0f, 0.0f),
        Vertex(x - halfEdge, y + halfEdge, z + halfEdge, -1.0f,  0.0f, 0.0f, 1.0f, 1.0f),
        Vertex(x - halfEdge, y + halfEdge, z - halfEdge, -1.0f,  0.0f, 0.0f, 0.0f, 1.0f),

        // Face droite (X+)
        Vertex(x + halfEdge, y - halfEdge, z - halfEdge, 1.0f,  0.0f, 0.0f, 1.0f, 0.0f),
        Vertex(x + halfEdge, y - halfEdge, z + halfEdge, 1.0f,  0.0f, 0.0f, 0.0f, 0.0f),
        Vertex(x + halfEdge, y + halfEdge, z + halfEdge, 1.0f,  0.0f, 0.0f, 0.0f, 1.0f),
        Vertex(x + halfEdge, y + halfEdge, z - halfEdge, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f),

        // Face du bas (Y-)
        Vertex(x - halfEdge, y - halfEdge, z - halfEdge, 0.0f,  -1.0f, 0.0f, 1.0f, 0.0f),
        Vertex(x + halfEdge, y - halfEdge, z - halfEdge, 0.0f,  -1.0f, 0.0f, 1.0f, 1.0f),
        Vertex(x + halfEdge, y - halfEdge, z + halfEdge, 0.0f,  -1.0f, 0.0f, 0.0f, 1.0f),
        Vertex(x - halfEdge, y - halfEdge, z + halfEdge, 0.0f,  -1.0f, 0.0f, 0.0f, 0.0f),

        // Face du haut (Y+)
        Vertex(x - halfEdge, y + halfEdge, z - halfEdge, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f),
        Vertex(x + halfEdge, y + halfEdge, z - halfEdge, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f),
        Vertex(x + halfEdge, y + halfEdge, z + halfEdge, 0.0f,  1.0f, 0.0f, 1.0f, 1.0f),
        Vertex(x - halfEdge, y + halfEdge, z + halfEdge, 0.0f,  1.0f, 0.0f, 1.0f, 0.0f)
    };

    // Indices : disent dans quel ordre relier les sommets pour former les triangles
    // Chaque face du cube = 2 triangles = 6 indices
    const std::vector<unsigned int> indices = {
        0, 1, 2,   2, 3, 0,     // Face avant (Z+)
        5, 4, 7,   7, 6, 5,     // Face arri�re (Z-)
        8, 9, 10,  10, 11, 8,   // Face gauche (X-)
        13, 12, 15, 15, 14, 13, // Face droite (X+)
        17, 16, 19, 19, 18, 17, // Face du bas (Y-)
        20, 21, 22, 22, 23, 20  // Face du haut (Y+)
    };

    m_mesh = std::make_unique<Mesh>(vertices, indices, (unsigned int)VertexAttribute::POSITION | (unsigned int)VertexAttribute::COLOR);
}

// Constructeur du cube
Cube::Cube(glm::vec3 center, float edge, Shader* shader, std::vector<Texture*> textures, Renderer* renderer, LightManager* lightManager, Player* player)
	: Cube(center, edge, shader, player) {
    m_textures = textures;
    m_lightManager = lightManager;
    m_renderer = renderer;
    m_textures = textures;

    // Coordonn�es du centre du cube
    float x = center[0];
    float y = center[1];
    float z = center[2];

    // Moiti� de la taille du cube (sert � placer les sommets autour du centre)
    float halfEdge = m_edge / 2.0f;

    // Cr�ation des composants n�cessaires
    m_transformation = std::make_unique<Transformation>(); // Permet de d�placer/faire tourner/agrandir l�objet

    // D�finition des sommets du cube
    // Chaque face a 4 sommets, et comme un cube a 6 faces -> 24 sommets en tout
    // Chaque sommet a : position (x,y,z), normale (ici mise � 0 pour l�instant), coordonn�es UV
    const std::vector<Vertex> vertices = {
        // Face avant (Z+)
        Vertex(x - halfEdge, y - halfEdge, z + halfEdge, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f),
        Vertex(x + halfEdge, y - halfEdge, z + halfEdge, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f),
        Vertex(x + halfEdge, y + halfEdge, z + halfEdge, 0.0f,  0.0f, 1.0f, 1.0f, 1.0f),
        Vertex(x - halfEdge, y + halfEdge, z + halfEdge, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f),

        // Face arri�re (Z-)
        Vertex(x - halfEdge, y - halfEdge, z - halfEdge, 0.0f,  0.0f, -1.0f, 1.0f, 0.0f),
        Vertex(x + halfEdge, y - halfEdge, z - halfEdge, 0.0f,  0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(x + halfEdge, y + halfEdge, z - halfEdge, 0.0f,  0.0f, -1.0f, 0.0f, 1.0f),
        Vertex(x - halfEdge, y + halfEdge, z - halfEdge, 0.0f,  0.0f, -1.0f, 1.0f, 1.0f),

        // Face gauche (X-)
        Vertex(x - halfEdge, y - halfEdge, z - halfEdge, -1.0f,  0.0f, 0.0f, 0.0f, 0.0f),
        Vertex(x - halfEdge, y - halfEdge, z + halfEdge, -1.0f,  0.0f, 0.0f, 1.0f, 0.0f),
        Vertex(x - halfEdge, y + halfEdge, z + halfEdge, -1.0f,  0.0f, 0.0f, 1.0f, 1.0f),
        Vertex(x - halfEdge, y + halfEdge, z - halfEdge, -1.0f,  0.0f, 0.0f, 0.0f, 1.0f),

        // Face droite (X+)
        Vertex(x + halfEdge, y - halfEdge, z - halfEdge, 1.0f,  0.0f, 0.0f, 1.0f, 0.0f),
        Vertex(x + halfEdge, y - halfEdge, z + halfEdge, 1.0f,  0.0f, 0.0f, 0.0f, 0.0f),
        Vertex(x + halfEdge, y + halfEdge, z + halfEdge, 1.0f,  0.0f, 0.0f, 0.0f, 1.0f),
        Vertex(x + halfEdge, y + halfEdge, z - halfEdge, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f),

        // Face du bas (Y-)
        Vertex(x - halfEdge, y - halfEdge, z - halfEdge, 0.0f,  -1.0f, 0.0f, 1.0f, 0.0f),
        Vertex(x + halfEdge, y - halfEdge, z - halfEdge, 0.0f,  -1.0f, 0.0f, 1.0f, 1.0f),
        Vertex(x + halfEdge, y - halfEdge, z + halfEdge, 0.0f,  -1.0f, 0.0f, 0.0f, 1.0f),
        Vertex(x - halfEdge, y - halfEdge, z + halfEdge, 0.0f,  -1.0f, 0.0f, 0.0f, 0.0f),

        // Face du haut (Y+)
        Vertex(x - halfEdge, y + halfEdge, z - halfEdge, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f),
        Vertex(x + halfEdge, y + halfEdge, z - halfEdge, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f),
        Vertex(x + halfEdge, y + halfEdge, z + halfEdge, 0.0f,  1.0f, 0.0f, 1.0f, 1.0f),
        Vertex(x - halfEdge, y + halfEdge, z + halfEdge, 0.0f,  1.0f, 0.0f, 1.0f, 0.0f)
    };

    // Indices : disent dans quel ordre relier les sommets pour former les triangles
    // Chaque face du cube = 2 triangles = 6 indices
    const std::vector<unsigned int> indices = {
        0, 1, 2,   2, 3, 0,     // Face avant (Z+)
        5, 4, 7,   7, 6, 5,     // Face arri�re (Z-)
        8, 9, 10,  10, 11, 8,   // Face gauche (X-)
        13, 12, 15, 15, 14, 13, // Face droite (X+)
        17, 16, 19, 19, 18, 17, // Face du bas (Y-)
        20, 21, 22, 22, 23, 20  // Face du haut (Y+)
    };

    m_mesh = std::make_unique<Mesh>(vertices, indices, (unsigned int)VertexAttribute::POSITION | (unsigned int)VertexAttribute::NORMAL | (unsigned int)VertexAttribute::TEXCOORD);
}

// ~Cube() est =default dans le header : unique_ptr gere la liberation automatique

inline std::vector<Texture*> Cube::getTextures() const {
    return m_textures;
}

// Prepare le cube pour etre affiche (envoie les donnees au GPU)
void Cube::update() {
    if (m_spinSpeedDeg != 0.0f && m_renderer) {
        m_spinAngle = std::fmod(m_spinAngle + m_spinSpeedDeg * m_renderer->getDeltaTime(), 360.0f);
        m_transformation->spinAroundSelf(m_center, m_spinAxis, m_spinAngle);
    }
}

// Dessine le cube a l'ecran
void Cube::draw() {
    // OUTLINE PASS
    if (m_outlineEnabled && m_outlineShader) {
        m_outlineShader->use();
        glm::mat4 outline_model = glm::scale(
            m_transformation->getMatrix(),
            glm::vec3(1.0f + m_outlineThickness)
        );
        m_outlineShader->setMat4("uModel", outline_model);
        m_outlineShader->setMat4("uView", m_shader->getCamera()->getViewMatrix());
        m_outlineShader->setMat4("uProjection", m_shader->getProjection());
        m_outlineShader->setVec3("uOutlineColor", m_outlineColor);

        glDepthMask(GL_FALSE);
        m_mesh->draw();
        glDepthMask(GL_TRUE);
    }

    m_shader->setModel(m_transformation->getMatrix());
    m_shader->use();
    m_shader->setupMatrices();

    if (m_shader->getType() == ShaderType::LightSource) {
        drawLightSourceShader();
    } else if (m_shader->getType() == ShaderType::SeveralLights) {
        drawSeveralLightShader();
    } else {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            std::cout << "Shader name not found in Cube draw : " << m_shader->getName() << std::endl;
        }
    }
    m_mesh->draw();
}

void Cube::drawLightSourceShader() {
    if (!m_lightSource) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            std::cout << "[Cube] drawLightSourceShader : light source non definie." << std::endl;
        }
        return;
    }
    m_shader->setVec3("lightColor", m_lightSource->getLightColor());
}

void Cube::drawSeveralLightShader() {
    if (!m_player) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            std::cout << "[Cube] drawSeveralLightShader : aucun joueur." << std::endl;
        }
        return;
    }

    m_shader->setVec3("viewPos", m_shader->getCamera()->getPosition());

    for (Texture* texture : m_textures) {
        if (!texture->hasSpecular()) {
            static bool s_warnedSpec = false;
            if (!s_warnedSpec) {
                s_warnedSpec = true;
                std::cout << "[Cube] drawSeveralLightShader : texture sans specular ignoree." << std::endl;
            }
            continue;
        }
        texture->applyToShader(m_shader);
    }

    m_lightManager->applyToShader(m_shader);
}