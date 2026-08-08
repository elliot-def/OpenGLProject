#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ShapeRenderer.h"

#include "ShaderManager.h"
#include "Rectangle.h"

ShapeRenderer::ShapeRenderer(ShaderManager* shaderManager) : m_shaderManager(shaderManager) {}

void ShapeRenderer::drawRectangle(float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f) {
	Rectangle(m_shaderManager->getShader("rectangle"), x, y, width, height).draw();
    m_shaderManager->getShader("rectangle")->use();

    // Créer la matrice de transformation
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    transform = glm::rotate(transform, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::scale(transform, glm::vec3(size.x, size.y, 1.0f));

    // Envoyer les uniforms
    GLint transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, &transform[0][0]);

    GLint colorLoc = glGetUniformLocation(shaderProgram, "color");
    glUniform3fv(colorLoc, 1, &color[0]);

    // Dessiner
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}