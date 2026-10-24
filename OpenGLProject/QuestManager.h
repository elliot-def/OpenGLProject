#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "MiniGames.h"

struct GLFWwindow;
class Scene;
class ShaderManager;
class TextRenderer;
class Image;
class Rectangle;

// ─────────────────────────────────────────────────────────────────────────────
// QuestManager : taches de la partie (mini-jeux).
//
// Postes = boutons de la carte nommes "quest:<type>:<id>" (type : wires, simon,
// timing, wave ; cf. MiniGames.h). A chaque carte, `count` postes sont tires avec
// une graine commune (memes postes chez tous les joueurs du lobby) ; les autres
// sont masques. E sur un poste actif ouvre son mini-jeu (panneau plein ecran qui
// capture les commandes, fropy continue de chasser). Reussi : le poste disparait
// et la tache est cochee dans la liste du HUD. Toutes faites : onAllDone.
// Progression locale a chaque joueur.
// ─────────────────────────────────────────────────────────────────────────────
class QuestManager {
public:
    QuestManager(GLFWwindow* window, ShaderManager* shaderManager,
                 std::vector<std::unique_ptr<TextRenderer>>* textRenderers);
    ~QuestManager();

    // Nouvelle carte chargee dans `scene` : tirage de `count` postes (graine `seed`).
    void beginMap(Scene* scene, int count, unsigned seed);
    // Bouton de carte actionne : true si c'est un poste de quete (le mini-jeu s'ouvre).
    bool handleButton(const std::string& name);
    void close();                         // ferme le mini-jeu en cours (fropy attrape le joueur)
    bool isOpen() const { return m_game != nullptr; }

    void update(float dt);
    void draw();                          // liste des taches (HUD) + mini-jeu ouvert

    int completed() const;
    int total() const { return static_cast<int>(m_tasks.size()); }
    void setOnAllDone(std::function<void()> cb) { m_onAllDone = std::move(cb); }

private:
    struct Task {
        std::string button;   // nom du bouton de carte
        std::string label;
        QuestType type;
        bool done = false;
    };
    class Painter;

    QuestInput readInput();
    void drawHud(Painter& p);
    void drawGame(Painter& p);

    GLFWwindow* m_window = nullptr;
    ShaderManager* m_shaderManager = nullptr;
    std::vector<std::unique_ptr<TextRenderer>>* m_textRenderers = nullptr;
    Scene* m_scene = nullptr;

    std::vector<Task> m_tasks;
    int m_current = -1;                   // tache du mini-jeu ouvert
    std::unique_ptr<MiniGame> m_game;
    unsigned m_seed = 0;
    float m_time = 0.0f;                  // horloge des animations
    float m_successTimer = 0.0f;          // > 0 : "reussi" affiche avant fermeture
    float m_openFade = 0.0f;              // 0 -> 1 a l'ouverture
    float m_hudFlash = 0.0f;              // surbrillance de la tache tout juste cochee
    std::function<void()> m_onAllDone;

    // Etats precedents (fronts d'appui ; Echap agit au relachement, cf. LobbyManager)
    bool m_prev[6] = {};
    bool m_escPrev = false;

    // Visuels (res/textures/quests, generes par make_quest_ui.py)
    std::unique_ptr<class Rectangle> m_rect;   // `class` : conflit avec Rectangle() de wingdi.h
    std::unique_ptr<Image> m_dim, m_panel, m_hudPanel, m_done, m_todo;
    std::unique_ptr<Image> m_icons[4];
};
