#pragma once

// Classe Renderer : gère le rendu et le timing des frames
class Renderer {
public:
    // Constructeur : initialise les variables
    Renderer();

    // Destructeur : nettoyage si nécessaire
    ~Renderer();

    // à appeler une fois par frame
    // -> Sert à calculer le deltaTime et éventuellement les FPS
    void handleFrameTiming();

    // Lance le rendu d'une frame
    // -> Généralement fait un glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    void clear();

    // Retourne le temps écoulé entre deux frames (en secondes)
    // -> Utile pour rendre le déplacement des objets indépendant du FPS
    float getDeltaTime() const;

    // Active ou désactive la limitation de FPS
    void setCapFPS(bool enabled);

    // Fixe la valeur de FPS cible (par exemple 60)
    void setTargetFPS(int fps);

private:
    double m_lastTime;   // Temps de la dernière frame (en secondes)
    float m_deltaTime;   // Temps écoulé entre deux frames (deltaTime)

    int m_frameCount;    // Nombre de frames depuis le dernier calcul
    double m_fpsTimer;   // Timer pour mesurer les FPS

    bool m_capFPS;       // Si true -> limite le FPS à m_targetFPS
    int m_targetFPS;     // FPS cible (ex: 60, 120…)
};
