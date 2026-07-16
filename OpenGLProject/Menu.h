#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <map>

#include "constants.h"
#include "Shape.h"

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

// Classe Menu
class Menu {
protected:
    std::vector<std::unique_ptr<TextRenderer>>* m_textRenderers;
	ShaderManager* m_shaderManager;
	SoundManager* m_soundManager;
    Game* m_game;
    std::vector<MenuText> m_items;
    std::map<int, MenuShape*> m_shapes;
    std::string m_title;
    float m_titleX, m_titleY, m_titleWidth, m_titleHeight;
    bool m_drawBackground;

    void drawTextCentered(const std::string& text, float centerX, float centerY, int textRendererIndex = 0, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float scale = 0.5f);

public:
    Menu(Game* game, SoundManager* soundManager, std::vector<std::unique_ptr<TextRenderer>>* textRenderers = nullptr, ShaderManager* shaderManager = nullptr, const std::string& t = "", bool bg = true)
        : m_game(game), m_soundManager(soundManager), m_textRenderers(textRenderers), m_shaderManager(shaderManager), m_title(t), m_titleX(Constants::MENU_TITLE_X), m_titleY(Constants::MENU_TITLE_Y), m_titleWidth(Constants::MENU_TITLE_W), m_titleHeight(Constants::MENU_TITLE_H), m_drawBackground(bg) {
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

    void clear() {
        m_items.clear();
        for (auto& pair : m_shapes) {
            delete pair.second;
        }
        m_shapes.clear();
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

    bool handleClick(double mouseX, double mouseY);

    virtual void update() {};

    void draw();
};