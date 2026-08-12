#include "LightSource.h"

#include "Vertex.h"   // Structure d'un sommet (position, couleur, texture…)
#include "Mesh.h"     // Classe pour gerer les buffers OpenGL et dessiner
#include "Shader.h"   // Classe pour les shaders OpenGL
#include "Texture.h"  // Classe pour les textures
#include "Transformation.h" // Classe pour position, rotation et scale
#include "Cube.h"

LightSource::LightSource(glm::vec3 center, Shader* shader, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular,
	float constant, float linear, float quadratic, glm::vec3 lightColor) {
	m_cube = std::make_unique<Cube>(center, 0.5f, shader);
	m_lightColor = lightColor;
	m_constant = constant;
	m_linear = linear;
	m_quadratic = quadratic;
	m_ambient = ambient;
	m_diffuse = diffuse;
	m_specular = specular;
}

LightSource::~LightSource() {
	
}

void LightSource::update() {
	// Pour l'instant, la source de lumière ne fait rien de spécial
}

std::unique_ptr<Cube>& LightSource::getCube() { return m_cube; }
glm::vec3 LightSource::getLightColor() { return m_lightColor; }
glm::vec3 LightSource::getAmbient() { return m_ambient; }
glm::vec3 LightSource::getDiffuse() { return m_diffuse; }
glm::vec3 LightSource::getSpecular() { return m_specular; }
glm::vec3 LightSource::getPosition() { return m_cube->getCenter(); }
float LightSource::getConstant() { return m_constant; }
float LightSource::getLinear() { return m_linear; }
float LightSource::getQuadratic() { return m_quadratic; }