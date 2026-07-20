#pragma once

#include "Shape.h"


class Shader;

class Rectangle : public Shape {
private:
    void setupBuffers() override;

public:
    Rectangle(Shader* shader, float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f));

    ~Rectangle();

    void draw() override;
};