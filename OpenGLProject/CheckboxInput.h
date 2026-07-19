#pragma once

#include <functional>
#include <glm/glm.hpp>

class Shader;
class Rectangle;
class CursorManager;

// Classe CheckboxInput : case a cocher simple (booleen)
// Compose 2 Rectangle : le cadre exterieur (box) et la coche interieure (check)
class CheckboxInput {
public:
    // shader        : shader utilise pour dessiner les rectangles
    // x, y          : position du centre de la case
    // size          : taille (largeur = hauteur) de la case
    // defaultValue  : etat initial (coche ou non)
    // onValueChanged : callback appele a chaque bascule (clic)
    CheckboxInput(CursorManager* cursorManager, Shader* shader, float x, float y, float size = 32.0f,
        bool defaultValue = false,
        std::function<void(bool)> onValueChanged = nullptr);

    ~CheckboxInput();

    // A appeler sur le clic (relachement du bouton) recu par le menu, ex :
    // if (checkbox->isPointInside(mouseX, mouseY)) checkbox->toggle();
    void toggle();
    void setValue(bool value);
    bool getValue() const { return m_value; }

    bool isPointInside(double px, double py) const;

    glm::vec2 getPosition() const { return m_position; }
    float getSize() const { return m_size; }

    void draw();

private:
    CursorManager* m_cursorManager;
    class Rectangle* m_box;   // Cadre exterieur
    class Rectangle* m_check; // Coche interieure (visible seulement si m_value == true)

    glm::vec2 m_position;
    float m_size;

    bool m_value;

    std::function<void(bool)> m_onValueChanged;
};
