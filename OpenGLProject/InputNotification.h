#pragma once

#include <chrono>
#include <string>

class TextRenderer;

// Source d'entree active du joueur (clavier/souris ou manette).
// Sert a afficher la notification de bascule dynamique.
enum class InputSource {
    KEYBOARD_MOUSE,
    CONTROLLER
};

// Classe InputNotification : petite notification a l'ecran (en haut au
// centre de la fenetre GLFW) annoncant le passage clavier/souris <-> manette.
// L'icone est un glyphe de la police kenney (res/fonts/kenney) dessine par
// un TextRenderer dedie, le libelle par le TextRenderer de texte standard.
class InputNotification {
public:
    // Duree d'affichage de la notification (secondes)
    static constexpr float NOTIFICATION_DURATION = 2.5f;

    // Affiche la notification pour la source donnee (repart le minuteur)
    void show(InputSource source);

    // True si la notification est encore affichee (minuteur non expire)
    bool isVisible() const;

    // Dessine l'icone kenney + le libelle au centre-haut de l'ecran.
    //  - controllerIcons : TextRenderer charge avec les icones manette kenney
    //  - keyboardIcons   : TextRenderer charge avec les icones clavier/souris kenney
    //  - textRenderer    : TextRenderer du texte standard (ex: Amarna)
    // A appeler entre beginFrame() et flush() des TextRenderers.
    void draw(TextRenderer* controllerIcons, TextRenderer* keyboardIcons,
              TextRenderer* textRenderer);

private:
    InputSource m_source = InputSource::KEYBOARD_MOUSE;
    std::chrono::steady_clock::time_point m_shownAt{};
    bool m_visible = false;

    // Constantes de rendu
    static constexpr float NOTIFICATION_Y = 140.0f;  // distance depuis le haut
    static constexpr float ICON_SCALE = 1.3f;        // icones kenney (~35px a 48px) -> ~45px
    static constexpr float TEXT_SCALE = 0.5f;        // texte Amarna (96px) -> 48px
    static constexpr float GAP = 14.0f;              // espace icone / texte
};
