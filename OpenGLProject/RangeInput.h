#pragma once

#include <functional>
#include <glm/glm.hpp>

class Shader;
class Rectangle;
class CursorManager;

// Classe RangeInput : slider horizontal permettant de choisir une valeur entre min et max
// Compose 3 Rectangle : la piste (track), le remplissage (fill) et le curseur (handle)
class RangeInput {
public:
    // shader        : shader utilise pour dessiner les rectangles
    // x, y          : position du centre du slider
    // width, height : dimensions totales de la piste
    // minValue/maxValue/defaultValue : bornes et valeur initiale
    // onValueChanged : callback appele a chaque changement de valeur (drag)
    RangeInput(Shader* shader, float x, float y, float width, float height,
        float minValue = 0.0f, float maxValue = 1.0f, float defaultValue = 0.5f,
        std::function<void(float)> onValueChanged = nullptr);

    ~RangeInput();

    // A appeler chaque frame avec la position souris et son etat (bouton gauche enfonce)
    // Gere le debut/fin du drag et met a jour la valeur si on est en train de glisser
    void update(double mouseX, double mouseY, bool mousePressed);

    void draw();

    float getValue() const { return m_value; }
    void setValue(float value);

    bool isPointInside(double px, double py) const;

    glm::vec2 getPosition() const { return m_position; }
    glm::vec2 getSize() const { return m_size; }

private:
	CursorManager* m_cursorManager;

    void updateHandlePosition();
    void setValueFromMouseX(double mouseX);

    class Rectangle* m_track;   // Barre de fond (grise)
    class Rectangle* m_fill;    // Portion remplie a gauche du curseur (progression)
    class Rectangle* m_handle;  // Curseur deplacable

    glm::vec2 m_position; // Centre du slider
    glm::vec2 m_size;     // Taille totale (piste)
    float m_handleWidth;

    float m_minValue;
    float m_maxValue;
    float m_value;

    bool m_isDragging = false;

    std::function<void(float)> m_onValueChanged;
};
