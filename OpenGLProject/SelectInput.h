#pragma once

#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

class Shader;
class Rectangle;
class CursorManager;

// Classe SelectInput : liste deroulante (dropdown)
// Etat ferme : affiche uniquement la valeur selectionnee (m_box)
// Etat ouvert : affiche en plus une rangee de Rectangle, une par option (m_optionBoxes)
//
// Le rendu du texte (valeur selectionnee + libelles des options) n'est pas gere ici :
// utilise getOptionLabel(i) / getSelectedLabel() avec ton TextRenderer, aux positions
// retournees par getPosition() / getOptionPosition(i).
class SelectInput {
public:
    // shader   : shader utilise pour dessiner les rectangles
    // x, y     : position du centre de la case fermee
    // width, height : dimensions d'une rangee (case fermee ou option)
    // options  : libelles affiches
    // defaultIndex : index selectionne au depart
    // onValueChanged : callback appele avec l'index choisi
    SelectInput(Shader* shader, float x, float y, float width, float height,
        std::vector<std::string> options, int defaultIndex = 0,
        std::function<void(int)> onValueChanged = nullptr);

    ~SelectInput();

    // A appeler sur le clic (relachement du bouton) recu par le menu
    // Gere l'ouverture/fermeture et la selection d'une option
    void handleClick(double mouseX, double mouseY);

    void draw();

    bool isOpen() const { return m_isOpen; }
    int getSelectedIndex() const { return m_selectedIndex; }
    const std::string& getSelectedLabel() const { return m_options[m_selectedIndex]; }
    const std::string& getOptionLabel(size_t i) const { return m_options[i]; }
    size_t getOptionCount() const { return m_options.size(); }

    glm::vec2 getPosition() const { return m_position; }
    glm::vec2 getOptionPosition(size_t i) const;

private:
    bool isPointInsideBox(double px, double py, glm::vec2 center) const;

    Shader* m_shader;
    CursorManager* m_cursorManager;
    class Rectangle* m_box;                       // Case affichant la valeur selectionnee
    std::vector<class Rectangle*> m_optionBoxes;  // Une rangee par option, visible seulement si m_isOpen

    std::vector<std::string> m_options;
    int m_selectedIndex;

    glm::vec2 m_position;
    glm::vec2 m_size;

    bool m_isOpen = false;

    std::function<void(int)> m_onValueChanged;
};
