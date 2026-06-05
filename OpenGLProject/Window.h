#pragma once

struct GLFWwindow; // Déclaration anticipée pour éviter d'inclure GLFW ici

class SoundManager; // Déclaration anticipée pour éviter d'inclure SoundManager ici
/**
 * @class Window
 * @brief Gère la création et l'utilisation d'une fenêtre OpenGL via GLFW
 *
 * La classe encapsule :
 * - la largeur et la hauteur de la fenêtre
 * - le titre de la fenêtre
 * - le pointeur vers GLFWwindow
 *
 * Elle fournit des fonctions pour :
 * - Mettre à jour la fenêtre (swap buffers et gestion des événements)
 * - Récupérer les informations de la fenêtre
 * - Initialiser et fermer correctement GLFW
 */
class Window {
public:
    /**
     * @brief Constructeur
     * @param soundManager Pointeur vers le gestionnaire de son
     *
     * Crée la fenêtre et initialise GLFW
     */
    Window(SoundManager* soundManager);

    /**
     * @brief Destructeur
     *
     * Détruit la fenêtre et libère les ressources GLFW
     */
    ~Window();

    /**
     * @brief Met à jour la fenêtre
     *
     * Swap les buffers pour afficher le rendu et récupère les événements
     */
    void update() const;
    void setCursorCaptured(bool shouldCapture);
    void setWindowIcon(const char* iconPath);
    void setCustomCursor(const char* cursorPath);
    
    // Getters
    GLFWwindow* getGLFWwindow() const; // Retourne le pointeur GLFW
    bool getShouldClose() const;       // Indique si la fenêtre doit se fermer
    int getWidth() const;              // Largeur de la fenêtre
    int getHeight() const;             // Hauteur de la fenêtre

private:
    int m_width;            // Largeur
    int m_height;           // Hauteur
    const char* m_title;    // Titre
    GLFWwindow* m_window;   // Pointeur vers la fenêtre GLFW
    SoundManager* m_soundManager; // Pointeur vers le gestionnaire de son

    void pollEvents() const;  // Récupère les événements (clavier, souris)
    void swapBuffers() const; // Échange les buffers pour le rendu
    bool init();              // Initialise GLFW et crée la fenêtre
};

