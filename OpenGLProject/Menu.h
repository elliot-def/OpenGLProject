#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <glm/glm.hpp>
#include <map>

#include "constants/window.h"
#include "constants/menu.h"

#include "Shape.h"
#include "Rectangle.h"
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
class Menu;             // Déclaration anticipée (utilisée par MenuInput)

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
    std::unique_ptr<Shape> shape;
    bool isHovered;
    std::function<void()> callback;

    MenuShape(Shape* s, std::function<void()> cb)
        : shape(s), isHovered(false), callback(cb) {
    }

    bool contains(double px, double py) const {
        return shape->isPointInside(px, py);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// MenuInput : interface commune des widgets d'entrée (slider, case, liste).
// Les trois familles (m_ranges/m_checkboxes/m_selects) sont remplacées par un
// seul std::vector<std::unique_ptr<MenuInput>> : draw(), handleClick() et la
// navigation manette n'appliquent plus la même logique trois fois.
// ─────────────────────────────────────────────────────────────────────────────
class MenuInput {
public:
    virtual ~MenuInput() = default;

    // Dessine le widget ET ses libellés/valeurs (texte via Menu::drawText*).
    virtual void draw(Menu& menu) = 0;

    // Gère le clic souris. Retourne true si le clic a été consommé (le menu
    // arrête alors la propagation et joue éventuellement le son de clic).
    virtual bool handleClick(double mouseX, double mouseY) = 0;

    // Test de survol du widget (sans effet de bord).
    virtual bool isPointInside(double mouseX, double mouseY) const = 0;

    // Centre du widget : sert au tri vertical de la navigation manette.
    virtual glm::vec2 getPosition() const = 0;

    // Focus manette (surlignage).
    virtual void setFocused(bool focused) = 0;

    // Hooks optionnels (no-op par défaut) :
    virtual void updateHover(double /*mouseX*/, double /*mouseY*/) {}
    virtual void updateDrag(double /*mouseX*/, double /*mouseY*/, bool /*pressed*/) {}
    virtual void activate() {}                    // Bouton A manette
    virtual void adjust(int /*direction*/) {}     // Gauche/droite manette
    virtual bool isOpen() const { return false; } // Liste ouverte : capture clic prioritaire
    virtual bool playsInteractionSound() const { return true; } // false pour le slider
};

// Wrapper d'un RangeInput (slider) avec son libellé
class MenuRange : public MenuInput {
public:
    std::string label;
    RangeInput* input;

    MenuRange(const std::string& l, RangeInput* i) : label(l), input(i) {}
    ~MenuRange() override { delete input; }

    void draw(Menu& menu) override;
    bool handleClick(double mouseX, double mouseY) override;
    bool isPointInside(double mouseX, double mouseY) const override;
    glm::vec2 getPosition() const override;
    void setFocused(bool focused) override;
    void updateDrag(double mouseX, double mouseY, bool pressed) override;
    void activate() override;
    void adjust(int direction) override;
    bool playsInteractionSound() const override { return false; }
};

// Wrapper d'un CheckboxInput (case à cocher) avec son libellé
class MenuCheckbox : public MenuInput {
public:
    std::string label;
    CheckboxInput* input;

    MenuCheckbox(const std::string& l, CheckboxInput* i) : label(l), input(i) {}
    ~MenuCheckbox() override { delete input; }

    void draw(Menu& menu) override;
    bool handleClick(double mouseX, double mouseY) override;
    bool isPointInside(double mouseX, double mouseY) const override;
    glm::vec2 getPosition() const override;
    void setFocused(bool focused) override;
    void activate() override;
};

// Wrapper d'un SelectInput (liste déroulante) avec son libellé
class MenuSelect : public MenuInput {
public:
    std::string label;
    SelectInput* input;

    MenuSelect(const std::string& l, SelectInput* i) : label(l), input(i) {}
    ~MenuSelect() override { delete input; }

    void draw(Menu& menu) override;
    bool handleClick(double mouseX, double mouseY) override;
    bool isPointInside(double mouseX, double mouseY) const override;
    glm::vec2 getPosition() const override;
    void setFocused(bool focused) override;
    void updateHover(double mouseX, double mouseY) override;
    void activate() override;
    void adjust(int direction) override;
    bool isOpen() const override;
};

// Classe Menu
class Menu {
protected:
    // ── Navigation manette (discrète) ──────────────────────────────────────
    // Le stick gauche déplace la sélection parmi les éléments actionnables
    // (items, cases à cocher, listes, sliders), A valide, gauche/droite ajuste.
    // m_focusTargets est reconstruit à la demande (m_focusDirty) et trié du
    // haut vers le bas selon centerY.
    enum class FocusType { Item, Input };
    struct FocusTarget {
        FocusType type;
        int index;
        float centerY; // tri vertical (haut -> bas)
    };
    std::vector<FocusTarget> m_focusTargets;
    int m_focusIndex = 0;      // index dans m_focusTargets
    bool m_focusDirty = true;  // liste à reconstruire (après clear/add)

    void rebuildFocusTargets();
    void applyFocusHighlight();

    std::vector<std::unique_ptr<TextRenderer>>* m_textRenderers;
	ShaderManager* m_shaderManager;
	SoundManager* m_soundManager;
	CursorManager* m_cursorManager;
    Game* m_game;
    std::vector<MenuText> m_items;
    std::map<int, std::unique_ptr<MenuShape>> m_shapes;
    // Shapes de premier plan (déjà possédés par m_shapes) : dessinés APRÈS
    // le flush du texte pour passer devant lui.
    std::vector<Shape*> m_overlayShapes;
    std::vector<std::unique_ptr<MenuInput>> m_inputs;
    std::string m_title;
    float m_titleX, m_titleY, m_titleWidth, m_titleHeight;
    bool m_drawBackground;
    // `class Rectangle` (type specifier élabore) : dans les TU qui incluent
    // windows.h AVANT Menu.h (ex: Game.cpp via win_compat.h -> config.h),
    // la fonction GDI wingdi.h `Rectangle(HDC,...)` est déclarée en premier et
    // HIDE le nom de la classe au niveau global. Sans le specifier élabore,
    // `std::unique_ptr<Rectangle>` résout vers la FONCTION -> C2923.
    std::unique_ptr<class Rectangle> m_background; // Rectangle du fond, créé une fois


public:
    // Helpers de rendu texte (utilisés aussi par les sous-classes MenuInput
    // pour dessiner leurs libellés/valeurs).
    void drawTextCentered(const std::string& text, float centerX, float centerY, int textRendererIndex = 0, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float scale = 0.5f);
    void drawTextRightAligned(const std::string& text, float centerX, float centerY, int textRendererIndex = 0, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float scale = 0.5f);
    void drawTextLeftAligned(const std::string& text, float centerX, float centerY, int textRendererIndex = 0, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float scale = 0.5f);
    Menu(Game* game, SoundManager* soundManager, std::vector<std::unique_ptr<TextRenderer>>* textRenderers = nullptr, ShaderManager* shaderManager = nullptr, CursorManager* cursorManager = nullptr, const std::string& t = "", bool bg = true)
        : m_game(game), m_soundManager(soundManager), m_textRenderers(textRenderers), m_shaderManager(shaderManager), m_cursorManager(cursorManager), m_title(t), m_titleX(Constants::Menu::MENU_TITLE_X), m_titleY(Constants::Menu::MENU_TITLE_Y), m_titleWidth(Constants::Menu::MENU_TITLE_W), m_titleHeight(Constants::Menu::MENU_TITLE_H), m_drawBackground(bg) {
    }

    ~Menu();

    void addItem(const std::string& text, float x = Constants::Window::WINDOW_WIDTH / 2, float y = Constants::Window::WINDOW_HEIGHT / 2, float width = 100, float height = 30, std::function<void()> callback = {}) {
        m_items.emplace_back(text, x, y, width, height, callback);
        m_focusDirty = true;
    }

    void addShape(int id, Shape* shape, std::function<void()> callback = {}) {
        m_shapes.emplace(id, std::make_unique<MenuShape>(shape, callback));
    }

    // Enregistre un shape possédé par m_shapes comme shape de PREMIER PLAN :
    // il sera dessiné APRÈS le flush du texte (cf. drawOverlays), donc devant
    // lui. Ex. : Easter egg DVD du MainMenu.
    void addOverlayShape(Shape* shape) {
        m_overlayShapes.push_back(shape);
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
        m_shapes.clear();
        m_overlayShapes.clear(); // pointeurs obsolètes : les shapes viennent d'être détruits
        m_inputs.clear();
        m_focusDirty = true;
    }

    // ── Navigation manette (discrète) ──────────────────────────────────────
    // Déplace la sélection d'un cran (direction : -1 = haut, +1 = bas, boucle).
    void navigate(int direction);
    // Valide la sélection (équivalent manette du clic) : callback d'item,
    // bascule de checkbox, ouverture/fermeture de liste, etc.
    void activateSelected();
    // Ajuste la sélection (direction : -1 = gauche, +1 = droite) : slider ou
    // option de liste.
    void adjustSelected(int direction);
    // Joue le son de clic partage des menus (bouton A, clic souris, bouton
    // B/Retour). Rendu public pour MenuManager::goBack() (retour manette).
    void playClickSound();
    // Repositionne la sélection sur la première ligne (entrée dans un menu).
    void resetFocus();
    // Position manette courante (index dans m_focusTargets) : MenuManager la
    // sauvegarde avant un changement de menu pour la restaurer au retour.
    int getFocusIndex() const { return m_focusIndex; }
    // Restaure une position manette sauvegardee (bornee a la liste courante ;
    // 0 si liste vide). Reconstruit la liste des cibles si besoin.
    void setFocusIndex(int index);

    const std::vector<MenuText>& getItems() const { return m_items; }

    // Met a jour le texte d'un item (ex: affichage de la touche associee apres un rebinding)
    void setItemText(size_t index, const std::string& text) {
        if (index < m_items.size()) m_items[index].text = text;
    }

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
        // La souris reprend la main : on retire aussi le focus manette des
        // widgets (cases, listes, sliders) pour eviter un double surlignage.
        for (auto& w : m_inputs) if (w) w->setFocused(false);
        // Survol d'option dans les listes ouvertes (surbrillance du dropdown)
        for (auto& w : m_inputs) if (w) w->updateHover(mouseX, mouseY);
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
        for (auto& w : m_inputs) {
            if (w) w->updateDrag(mouseX, mouseY, mousePressed);
        }
    }

    bool handleClick(double mouseX, double mouseY);

    virtual void update() {};

    void draw();

    // Dessine les shapes de premier plan (par-dessus le texte). À appeler
    // APRÈS le flush des TextRenderers (cf. Game::run / MenuManager::drawOverlays).
    void drawOverlays();

    // Lazy-init du Rectangle de fond (évite recréation par frame)
    void ensureBackground();
};
