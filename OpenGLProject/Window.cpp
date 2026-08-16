#include <iostream>
#include <cstdlib>

#include "Window.h"
#include "Log.h"
#include "SoundManager.h"
#include "constants/window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>

Window::Window() : m_width(Constants::Window::WINDOW_WIDTH), m_height(Constants::Window::WINDOW_HEIGHT), m_title(Constants::Window::WINDOW_TITLE), m_window(nullptr) {
    if (!init()) {
        logErr() << "Failed to initialize Window\n";
        std::exit(EXIT_FAILURE);
    }
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
}

bool Window::getShouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::swapBuffers() const {
    glfwSwapBuffers(m_window);
}

void Window::update() const {
    swapBuffers();
    pollEvents();
}

GLFWwindow* Window::getGLFWwindow() const {
    return m_window;
}

int Window::getWidth() const {
    return m_width;
}

int Window::getHeight() const {
    return m_height;
}

void Window::setCursorCaptured(bool shouldCapture) {
    //efface le bouton de la souris et permet de capturer la souris
    glfwSetInputMode(m_window, GLFW_CURSOR, shouldCapture ?  GLFW_CURSOR_DISABLED : GLFW_CURSOR_CAPTURED);
}

bool Window::init() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = nullptr;
    const GLFWvidmode* mode = nullptr;

    if (Constants::Window::IS_WINDOW_FULLSCREEN) {
        monitor = glfwGetPrimaryMonitor();
        mode = glfwGetVideoMode(monitor);
        if (!mode) {
            logErr() << "Failed to get video mode for fullscreen\n";
            return false;
        }

        // Fixer la résolution, le taux de rafraîchissement et les bits couleur
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

        m_width = mode->width;
        m_height = mode->height;
    }

    // Création de la fenêtre
    m_window = glfwCreateWindow(
        m_width, m_height, m_title,
        monitor, // nullptr pour fenêtre normale, sinon fullscreen
        nullptr
    );

    if (!m_window) {
        logErr() << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        logErr() << "Failed to initialize GLAD\n";
        return false;
    }

    // Définition de la zone de rendu
    glViewport(0, 0, m_width, m_height);

    // Vsync desactive : pas de cap FPS lie au taux de rafraichissement.
    // Pour limiter les FPS, utiliser le cap logiciel de Renderer
    // (DEFAULT_IS_FPS_CAPPING / DEFAULT_FPS_CAPPING dans constants/renderer.h).
    glfwSwapInterval(0);

    //efface le bouton de la souris et permet de capturer la souris
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);

    setWindowIcon("./res/textures/logo.jpeg");

    return true;
}

// Fonction pour charger et définir l'icône de la fenêtre
void Window::setWindowIcon(const char* iconPath) {
    // Charger l'image avec stb_image
    int width, height, channels;
    unsigned char* pixels = stbi_load(iconPath, &width, &height, &channels, 4); // Force RGBA

    if (!pixels) {
        logPrintf("Erreur: Impossible de charger l'icone %s\n", iconPath);
        return;
    }

    // Créer la structure GLFWimage
    GLFWimage icon;
    icon.width = width;
    icon.height = height;
    icon.pixels = pixels;

    // Définir l'icône de la fenêtre
    glfwSetWindowIcon(m_window, 1, &icon);

    // Libérer la mémoire de l'image
    stbi_image_free(pixels);

    logPrintf("Icone definie avec succes (%dx%d)\n", width, height);
}

// ---------------------------------------------------------------------------
// Contexte GL partage (thread de chargement)
// ---------------------------------------------------------------------------

GLFWwindow* Window::createSharedContext() const {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* w = glfwCreateWindow(1, 1, "loader", nullptr, m_window);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);  // restore default
    if (!w) {
        logPrintf("[Window] ERREUR: impossible de creer le contexte partage.\n");
    }
    return w;
}

void Window::destroySharedContext(GLFWwindow* w) const {
    if (w) glfwDestroyWindow(w);
}

void Window::setCustomCursor(const char* cursorPath) {
    int cursor_width, cursor_height, cursor_channels;
    unsigned char* cursor_pixels = stbi_load(cursorPath, &cursor_width, &cursor_height, &cursor_channels, 4);
    if (cursor_pixels) {
        // Construire une GLFWimage à partir des données chargées par stb_image
        GLFWimage cursor_image{};
        cursor_image.width = cursor_width;
        cursor_image.height = cursor_height;
        cursor_image.pixels = cursor_pixels;

        GLFWcursor* cursor = glfwCreateCursor(&cursor_image, 0, 0);
        if (cursor) {
            glfwSetCursor(m_window, cursor);
            // Note: vous pouvez conserver le pointeur `cursor` si vous voulez le détruire plus tard avec glfwDestroyCursor
        }
        else {
            logPrintf("Erreur: Impossible de creer le curseur GLFW\n");
        }

        // glfwCreateCursor copie les données, on peut donc libérer l'image chargée
        stbi_image_free(cursor_pixels);
    }
    else {
        logPrintf("Erreur: Impossible de charger l'image du curseur ./res/textures/menu/cursor.png\n");
    }
}