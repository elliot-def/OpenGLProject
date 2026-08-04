#pragma once

struct GLFWwindow; // D�claration anticip�e pour �viter d'inclure GLFW ici

class SoundManager; // D�claration anticip�e pour �viter d'inclure SoundManager ici
/**
 * @class Window
 * @brief G�re la cr�ation et l'utilisation d'une fen�tre OpenGL via GLFW
 *
 * La classe encapsule :
 * - la largeur et la hauteur de la fen�tre
 * - le titre de la fen�tre
 * - le pointeur vers GLFWwindow
 *
 * Elle fournit des fonctions pour :
 * - Mettre � jour la fen�tre (swap buffers et gestion des �v�nements)
 * - R�cup�rer les informations de la fen�tre
 * - Initialiser et fermer correctement GLFW
 */
class Window {
public:
    /**
     * @brief Constructeur
     * @param soundManager Pointeur vers le gestionnaire de son
     *
     * Cr�e la fen�tre et initialise GLFW
     */
    Window();

    /**
     * @brief Destructeur
     *
     * D�truit la fen�tre et lib�re les ressources GLFW
     */
    ~Window();

    /**
     * @brief Met � jour la fen�tre
     *
     * Swap les buffers pour afficher le rendu et r�cup�re les �v�nements
     */
    void update() const;
    void setCursorCaptured(bool shouldCapture);
    void setWindowIcon(const char* iconPath);
    void setCustomCursor(const char* cursorPath);

    // Contexte GL partagé pour le chargement en arrière-plan.
    // Crée une fenêtre invisible 1×1 partageant les ressources du contexte principal.
    GLFWwindow* createSharedContext() const;
    void        destroySharedContext(GLFWwindow* w) const;
    
    // Getters
    GLFWwindow* getGLFWwindow() const; // Retourne le pointeur GLFW
    bool getShouldClose() const;       // Indique si la fen�tre doit se fermer
    int getWidth() const;              // Largeur de la fen�tre
    int getHeight() const;             // Hauteur de la fen�tre

private:
    int m_width;            // Largeur
    int m_height;           // Hauteur
    const char* m_title;    // Titre
    GLFWwindow* m_window;   // Pointeur vers la fen�tre GLFW

    void pollEvents() const;  // R�cup�re les �v�nements (clavier, souris)
    void swapBuffers() const; // �change les buffers pour le rendu
    bool init();              // Initialise GLFW et cr�e la fen�tre
};

