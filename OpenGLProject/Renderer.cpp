#include "Renderer.h"

#include "constants/renderer.h"
#include "Log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>

Renderer::Renderer()
    : m_lastTime(glfwGetTime()), m_deltaTime(0.0f),
    m_frameCount(0), m_fpsTimer(0.0),
    m_capFPS(Constants::Renderer::DEFAULT_IS_FPS_CAPPING),
    m_targetFPS(Constants::Renderer::DEFAULT_FPS_CAPPING)
{
    // Rien a faire de plus : on initialise juste les variables
}

Renderer::~Renderer() {}

void Renderer::handleFrameTiming() {
    double frameTime = 1.0 / m_targetFPS;
    double currentTime = glfwGetTime();
    m_deltaTime = static_cast<float>(currentTime - m_lastTime);

    if (m_capFPS) {
        // Throttling calme : un SEUL sommeil calibré jusqu'à ~2ms avant la
        // cible, puis une courte micro-boucle de précision (≤2ms).
        // L'ancienne boucle dormait par tranches de 1ms en réinterrogeant
        // glfwGetTime à chaque réveil (plusieurs dizaines de wakeups/frame) ;
        // avec le vsync actif (glfwSwapInterval), le swap bloque déjà sur le
        // vblank, ce chemin ne sert que pour le cap logiciel optionnel.
        const double targetTime = m_lastTime + frameTime;
        const double remaining = targetTime - glfwGetTime();
        if (remaining > 0.002) {
            std::this_thread::sleep_for(std::chrono::duration<double>(remaining - 0.002));
        }
        while ((glfwGetTime() - m_lastTime) < frameTime) {
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
        LOG_INFO("FPS: %d", m_frameCount);
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
