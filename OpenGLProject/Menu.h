#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <map>

#include "constants.h"
#include "Shape.h"
#include "RangeInput.h"
#include "CheckboxInput.h"
#include "SelectInput.h"

class InputManager;     // Déclaration anticipée
class SoundManager;     // Déclaration anticipée
class ShaderManager;    // Déclaration anticipée
class TextRenderer;     // Déclaration anticipée
class TextureManager;   // Déclaration anticipée
class Shader;           // Déclaration anticipée
class Sound;           // Déclaration anticipée
class Game;             // Déclaration anticipée

// Structure pour un élément de menu
struct MenuText {
    std::string text;
    float x, y, width, height;
    bool isHovered;
    std::function<void()> callback;

    MenuText(const std::string& t, float px, float py, float w, float h, std::function<void()> cb)
        : text(t), x(px-w/2), y(py-h/2), width(w), height(h), isHovered(false), callback(cb) {
    }

    bool contains(double px, double py) const {
        return (px >= x && px <= x + width && py >= y && py <= y + height);
    }
};

// Structure pour un élément de menu
struct MenuShape {
    Shape* shape;
    bool isHovered;
    std::function<void()> callback;

    MenuShape(Shape* s, std::function<void()> cb)
        : shape(s), isHovered(false), callback(cb) {
    }

    bool contains(double px, double py) const {
        return shape->isPointInside(px, py);
    }
};

// Wrapper d'un RangeInput avec son libellé (le texte est dessiné par Menu::draw via TextRenderer)
struct MenuRange {
    std::string label;
    RangeInput* input;

    MenuRange(const std::string& l, RangeInput* i) : label(l), input(i) {}
    ~MenuRange() { delete input; }
};

// Wrapper d'un CheckboxInput avec son libellé
struct MenuCheckbox {
    std::string label;
    CheckboxInput* input;

    MenuCheckbox(const std::string& l, CheckboxInput* i) : label(l), input(i) {}
    ~MenuCheckbox() { delete input; }
};

// Wrapper d'un SelectInput avec son libellé
struct MenuSelect {
    std::string label;
    SelectInput* input;

    MenuSelect(const std::string& l, SelectInput* i) : label(l), input(i) {}
    ~MenuSelect() { delete input; }
};

// Classe Menu
class Menu {
protected:
    std::vector<std::unique_ptr<TextRenderer>>* m_textRenderers;
	ShaderManager* m_shaderManager;
	SoundManager* m_soundManager;
	CursorManager* m_cursorManager;
    Game* m_game;
    std::vector<MenuText> m_items;
    std::map<int, MenuShape*> m_shapes;
    std::vector<MenuRange*> m_ranges;
    std::vector<MenuCheckbox*> m_checkboxes;
    std::vector<MenuSelect*> m_selects;
    std::string m_title;
    float m_titleX, m_titleY, m_titleWidth, m_titleHeight;
    bool m_drawBackground;

    void drawTextCentered(const std::string& text, float centerX, float centerY, int textRendererIndex = 0, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float scale = 0.5f);
    void drawTextRightAligned(const std::string& text, float centerX, float centerY, int textRendererIndex = 0, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float scale = 0.5f);
    void drawTextLeftAligned(const std::string& text, float centerX, float centerY, int textRendererIndex = 0, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float scale = 0.5f);

public:
    Menu(Game* game, SoundManager* soundManager, std::vector<std::unique_ptr<TextRenderer>>* textRenderers = nullptr, ShaderManager* shaderManager = nullptr, CursorManager* cursorManager = nullptr, const std::string& t = "", bool bg = true)
        : m_game(game), m_soundManager(soundManager), m_textRenderers(textRenderers), m_shaderManager(shaderManager), m_cursorManager(cursorManager), m_title(t), m_titleX(Constants::MENU_TITLE_X), m_titleY(Constants::MENU_TITLE_Y), m_titleWidth(Constants::MENU_TITLE_W), m_titleHeight(Constants::MENU_TITLE_H), m_drawBackground(bg) {
    }

    ~Menu() {
        clear();
    }

    void addItem(const std::string& text, float x = Constants::WINDOW_WIDTH / 2, float y = Constants::WINDOW_HEIGHT / 2, float width = 100, float height = 30, std::function<void()> callback = {}) {
        m_items.emplace_back(text, x, y, width, height, callback);
    }

    void addShape(int id, Shape* shape, std::function<void()> callback = {}) {
        m_shapes.emplace(id, new MenuShape(shape, callback));
    }

    // label         : texte affiche a gauche du slider (rendu par Menu::draw)
    // x, y          : centre du slider
    // width, height : dimensions de la piste
    // Implementation dans Menu.cpp (ShaderManager n'y est que forward-declare ici)
    void addRange(const std::string& label, float x, float y, float width, float height,
        float minValue = 0.0f, float maxValue = 1.0f, float defaultValue = 0.5f,
        std::function<void(float)> onValueChanged = {});

    // label   : texte affiche a gauche de la case
    // x, y    : centre de la case
    // size    : cote de la case (carree)
    void addCheckbox(const std::string& label, float x, float y, float size = 32.0f,
        bool defaultValue = false, std::function<void(bool)> onValueChanged = {});

    // label         : texte affiche a gauche du select
    // x, y          : centre de la case fermee
    // width, height : dimensions d'une rangee (case fermee ou option)
    void addSelect(const std::string& label, float x, float y, float width, float height,
        std::vector<std::string> options, int defaultIndex = 0,
        std::function<void(int)> onValueChanged = {});

    void clear() {
        m_items.clear();
        for (auto& pair : m_shapes) {
            delete pair.second;
        }
        m_shapes.clear();

        for (auto* range : m_ranges) {
            delete range;
        }
        m_ranges.clear();

        for (auto* checkbox : m_checkboxes) {
            delete checkbox;
        }
        m_checkboxes.clear();

        for (auto* select : m_selects) {
            delete select;
        }
        m_selects.clear();
    }

    std::vector<MenuText> getItems() { return m_items; }

    void setSelectedItem(int index) { 
        for (auto& item : m_items) {
            item.isHovered = false;
		}
        m_items[index].isHovered = true;
    }

    void updateHover(double mouseX, double mouseY) {
        for (auto& item : m_items) {
            item.isHovered = false;
        }
        for (auto& item : m_items) {
            item.isHovered = item.contains(mouseX, mouseY);

        }
        for (auto& shape : m_shapes) {
            shape.second->isHovered = shape.second->contains(mouseX, mouseY);
        }
    }

    // A appeler chaque frame (independamment du clic) avec la position souris et l'etat du bouton gauche.
    // Necessaire pour que les sliders (RangeInput) puissent etre glisses (drag).
    void updateDrag(double mouseX, double mouseY, bool mousePressed) {
        for (auto* range : m_ranges) {
            range->input->update(mouseX, mouseY, mousePressed);
        }
    }

    bool handleClick(double mouseX, double mouseY);

    virtual void update() {};

    void draw();
};
