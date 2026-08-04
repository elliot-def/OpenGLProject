#pragma once

class TextRenderer;

// ---------------------------------------------------------------------------
// LoadingScreen : écran de chargement autonome (shader inline, zéro dépendance)
//
// Barre de progression par étapes discrètes avec fondu fluide entre chaque
// palier. Les points d'étape sous la barre indiquent la progression.
//
//     auto screen = LoadingScreen();
//     screen.setStep(3, 7);     // 3ème étape sur 7
//     screen.draw(deltaTime);   // la barre glide vers 3/7
//
// Optionnel : injecter un TextRenderer via setTextRenderer() pour afficher
// le nom de l'étape au-dessus de la barre avec drawLabel().
// ---------------------------------------------------------------------------

class LoadingScreen {
public:
    LoadingScreen();
    ~LoadingScreen();

    // Avance l'animation (lerp + glow) puis dessine l'écran.
    // dt  : delta time en secondes.
    // alpha : opacité (1.0 = normal, 0.0 = fondu complet).
    void draw(float dt, float alpha = 1.0f);

    // Définit l'étape courante. La barre glide vers current/total.
    // (0, 0) = pas d'indicateur (utilisé pendant STEAM_WAIT).
    void setStep(int current, int total);

    // Nom de l'étape (1-indexé). Retourne nullptr si l'index est invalide.
    static const char* getStepName(int index);
    static constexpr int STEP_COUNT = 7;

    // Injecte un TextRenderer pour l'affichage du nom d'étape.
    // Si non appelé, drawLabel() est un no-op.
    void setTextRenderer(TextRenderer* tr) { m_textRenderer = tr; }

    // Dessine le nom de l'étape courante centré au-dessus de la barre.
    // À appeler après draw(). No-op si setTextRenderer() n'a pas été appelé.
    void drawLabel();

private:
    unsigned int m_shader      = 0;
    unsigned int m_vao         = 0;
    int          m_timeLoc     = -1;
    int          m_alphaLoc    = -1;
    int          m_progressLoc = -1;
    int          m_stepLoc     = -1;
    int          m_stepCntLoc  = -1;

    float m_time            = 0.0f;
    float m_targetProgress  = 0.0f;   // 0..1, objectif
    float m_displayProgress = 0.0f;   // 0..1, valeur affichée (lerp)
    int   m_step            = 0;
    int   m_stepCount       = 0;

    static constexpr float LERP_SPEED = 3.0f;  // vitesse du glide entre étapes

    TextRenderer* m_textRenderer = nullptr;    // optionnel, pour drawLabel()
};
