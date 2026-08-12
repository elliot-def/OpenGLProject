#pragma once

#include <memory>
#include <glm/glm.hpp>

class Cube;
class Shader;
class Texture;

class LightSource {
public:
    LightSource(glm::vec3 center, Shader* shader,
        glm::vec3 ambient = glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3 diffuse = glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f),
        float constant = 1.0f, float linear = 1.0f, float quadratic = 1.0f, glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f));

    ~LightSource();

    void update();

    glm::vec3 getLightColor();
    glm::vec3 getAmbient();
    glm::vec3 getDiffuse();
    glm::vec3 getSpecular();
    glm::vec3 getPosition();
    float getConstant();
    float getLinear();
    float getQuadratic();

    // Accès au Cube (version smart pointer)
    std::unique_ptr<Cube>& getCube();

private:
    std::unique_ptr<Cube> m_cube;  // Propriété du cube
    glm::vec3 m_lightColor, m_ambient, m_diffuse, m_specular;
    float m_constant, m_linear, m_quadratic;
};
