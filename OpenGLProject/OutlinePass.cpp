#include "Outlineable.h"
#include "Shader.h"
#include "Mesh.h"
#include "Model.h"
#include "SharedQuad.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Outline {

void draw2D(Shader* outlineShader,
            const glm::vec3& outlineColor, float outlineThickness,
            const glm::vec3& position, const glm::vec2& size, float rotation,
            const glm::mat4& projection) {
    outlineShader->use();
    glm::vec2 outlineSize = size * (1.0f + outlineThickness);
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::translate(model, glm::vec3(outlineSize.x * 0.5f, outlineSize.y * 0.5f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::translate(model, glm::vec3(-outlineSize.x * 0.5f, -outlineSize.y * 0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(outlineSize.x, outlineSize.y, 1.0f));
    outlineShader->setMat4("uModel", model);
    outlineShader->setMat4("uView", glm::mat4(1.0f));
    outlineShader->setMat4("uProjection", projection);
    outlineShader->setVec3("uOutlineColor", outlineColor);

    glDepthMask(GL_FALSE);
    SharedQuad::draw();
    glDepthMask(GL_TRUE);
}

void draw3DMesh(Shader* outlineShader, Shader* mainShader,
                const glm::vec3& outlineColor, float outlineThickness,
                const glm::mat4& modelMatrix, Mesh* mesh) {
    outlineShader->use();
    glm::mat4 outlineModel = glm::scale(modelMatrix, glm::vec3(1.0f + outlineThickness));
    outlineShader->setMat4("uModel", outlineModel);
    outlineShader->setMat4("uView", mainShader->getCamera()->getViewMatrix());
    outlineShader->setMat4("uProjection", mainShader->getProjection());
    outlineShader->setVec3("uOutlineColor", outlineColor);

    glDepthMask(GL_FALSE);
    mesh->draw();
    glDepthMask(GL_TRUE);
}

void draw3DModel(Shader* outlineShader, Shader* mainShader,
                 const glm::vec3& outlineColor, float outlineThickness,
                 const glm::mat4& modelMatrix, Model& model) {
    outlineShader->use();
    glm::mat4 outlineModel = glm::scale(modelMatrix, glm::vec3(1.0f + outlineThickness));
    outlineShader->setMat4("uModel", outlineModel);
    outlineShader->setMat4("uView", mainShader->getCamera()->getViewMatrix());
    outlineShader->setMat4("uProjection", mainShader->getProjection());
    outlineShader->setVec3("uOutlineColor", outlineColor);

    glDepthMask(GL_FALSE);
    model.draw(*outlineShader);
    glDepthMask(GL_TRUE);
}

} // namespace Outline
