#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ShapeRenderer.h"

#include "ShaderManager.h"
#include "Rectangle.h"

ShapeRenderer::ShapeRenderer(ShaderManager* shaderManager) : m_shaderManager(shaderManager) {}

void ShapeRenderer::drawRectangle(float x, float y, float width, float height) {
    Rectangle(m_shaderManager->getShader("rectangle"), x, y, width, height).draw();
}