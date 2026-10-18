#pragma once

#include <glm/glm.hpp>

class Shader;

// ─────────────────────────────────────────────────────────────────────────────
// TvScreen : écran de télévision posé sur un meuble (placement fourni par la
// carte, clé JSON "tvs" → MapLoader::getScreens()). L'écran affiche l'easter
// egg du menu principal : le logo DVD qui rebondit et change de couleur à
// chaque rebond (même comportement que DVDShape, mais en 3D sur l'écran).
//
// Optimisation : le logo n'est ni animé ni dessiné que quand la caméra est
// assez proche (MAX_DRAW_DISTANCE) ; au-delà seul le fond d'écran (1 quad)
// est rendu. Le fond est un quad plein noir (texture blanche 1×1 + couleur du
// shader "image/mask"), le logo un second quad juste devant (décalage z minime
// pour éviter le z-fighting) avec blending (le masque du logo gère l'alpha).
//
// Rendu en repère monde : le shader "image/mask" n'a que "model" et
// "projection", on y met donc model = view · monde, projection = perspective
// de la caméra (pas de passage par l'ortho des Shape 2D).
// ─────────────────────────────────────────────────────────────────────────────
class TvScreen {
public:
    // model : repère local de l'écran (quad centré sur l'origine, normale +Z),
    // width/height en mètres. maskShader : shader "image/mask" (ShaderManager).
    TvScreen(Shader* maskShader, const glm::mat4& model, float width, float height);
    ~TvScreen();

    // Anime le logo DVD si la caméra est à portée (sinon no-op : optimisation).
    void update(float deltaTime, const glm::vec3& cameraPos);

    // Dessine le fond d'écran (toujours) + le logo DVD (seulement à portée).
    void draw(const glm::mat4& view, const glm::mat4& projection);

private:
    void pickNewColor();

    // Portée de l'easter egg (en mètres) : au-delà, ni animation ni logo.
    static constexpr float MAX_DRAW_DISTANCE = 5.0f;

    Shader*     m_shader = nullptr;
    glm::mat4   m_model{ 1.0f };       // placement monde (translate · rotate)
    glm::vec3   m_center{ 0.0f };      // centre monde (= translation de m_model)
    float       m_width  = 1.0f;       // écran, en mètres
    float       m_height = 0.6f;

    // Logo DVD dans le repère local de l'écran (centre du logo, en mètres,
    // origine = centre de l'écran) + vitesse (m/s) — mêmes proportions de
    // rebond que DVDShape dans le menu principal.
    glm::vec2   m_logoSize{ 0.42f, 0.248f };   // 330×195 px, comme le menu
    glm::vec2   m_logoPos{ 0.0f };
    glm::vec2   m_velocity{ 0.25f, 0.23f };
    glm::vec3   m_color{ 1.0f, 1.0f, 1.0f };
    int         m_colorIndex = 0;
    bool        m_active = false;      // caméra à portée (mis à jour par update)

    unsigned int m_whiteTexture = 0;   // texture blanche 1×1 (fond d'écran)
    unsigned int m_logoTexture  = 0;   // masque du logo DVD (dvd_logo.png)
};
