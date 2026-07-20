// SharedQuad.h
#pragma once
#include <glad/glad.h>

class SharedQuad {
public:
    static void init() {
        if (s_initialized) return;

        // Positions + UV d'un quad unitaire (-0.5,-0.5,0) -> (0.5,0.5)
        float vertices[] = {
            // pos.x, pos.y, pos.z,  u,    v
            -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
            0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
            0.5f, 0.5f, 0.0f,   1.0f, 1.0f,
            -0.5f, 0.5f, 0.0f,   0.0f, 1.0f,
        };
        unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

        glGenVertexArrays(1, &s_vao);
        glGenBuffers(1, &s_vbo);
        glGenBuffers(1, &s_ebo);

        glBindVertexArray(s_vao);

        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // layout(location = 0) = position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // layout(location = 1) = UV
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        s_initialized = true;
    }

    static void draw() {
        glBindVertexArray(s_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    static void destroy() {
        if (!s_initialized) return;
        glDeleteVertexArrays(1, &s_vao);
        glDeleteBuffers(1, &s_vbo);
        glDeleteBuffers(1, &s_ebo);
        s_initialized = false;
    }

private:
    static inline unsigned int s_vao = 0, s_vbo = 0, s_ebo = 0;
    static inline bool s_initialized = false;
};