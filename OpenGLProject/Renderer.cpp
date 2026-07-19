#include "Renderer.h"
#include "constants.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <thread>
#include <chrono>

Renderer::Renderer()
    : m_lastTime(glfwGetTime()), m_deltaTime(0.0f),
    m_frameCount(0), m_fpsTimer(0.0),
    m_capFPS(Constants::DEFAULT_IS_FPS_CAPPING),
    m_targetFPS(Constants::DEFAULT_FPS_CAPPING)
{
    // Rien a faire de plus : on initialise juste les variables
}

Renderer::~Renderer() {}

void Renderer::handleFrameTiming() {
    double frameTime = 1.0 / m_targetFPS;
    double currentTime = glfwGetTime();
    m_deltaTime = static_cast<float>(currentTime - m_lastTime);

    if (m_capFPS) {
        double timeElapsed = glfwGetTime() - m_lastTime;

        // 1. Sommeil safe : on dort si on a plus de 2 millisecondes d'avance.
        // On garde une marge de 2ms (0.002s) car l'OS peut se réveiller en retard.
        while (timeElapsed < (frameTime - 0.002)) {
            // On dort par petites tranches de 1ms pour ne pas rater notre cible
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timeElapsed = glfwGetTime() - m_lastTime;
        }

        // 2. Micro-boucle active : précision maximale sur les dernières fractions de ms.
        while ((glfwGetTime() - m_lastTime) < frameTime) {
            // On peut optionnellement ajouter un yield() pour être sympa avec l'OS
            std::this_thread::yield();
        }

        currentTime = glfwGetTime();
        m_deltaTime = static_cast<float>(currentTime - m_lastTime);
    }

    m_lastTime = currentTime;

    // Comptage des FPS
    m_frameCount++;
    m_fpsTimer += m_deltaTime;

    if (m_fpsTimer >= 1.0) {
        std::cout << "FPS: " << m_frameCount << std::endl;
        m_frameCount = 0;
        m_fpsTimer = 0.0;
    }
}

void Renderer::clear() {
    glClearColor(0.0f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

float Renderer::getDeltaTime() const {
    return m_deltaTime;
}

void Renderer::setCapFPS(bool enabled) {
    m_capFPS = enabled;
}

void Renderer::setTargetFPS(int fps) {
    if (fps > 0) {
        m_targetFPS = fps;
    }
}
