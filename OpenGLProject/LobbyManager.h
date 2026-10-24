#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;
class SteamManager;
class ShaderManager;
class TextRenderer;
class Rectangle;

// Options de partie choisies par l'hote dans le lobby (indices dans les listes
// LobbyManager::optionLabels).
struct MatchSettings {
    int difficulty = 1;   // 0 Facile, 1 Normal, 2 Difficile
    int map = 2;          // 0 Village (jour), 1 Village (nuit), 2 Batiment (defaut)
    int mode = 0;         // 0 Chasse (fropy actif), 1 Exploration (sans fropy)
    unsigned seed = 0;    // graine de la partie (tiree par l'hote, publiee dans le lobby) : tirage des taches

    // Effets de la difficulte sur fropy.
    float creatureSpeedMul() const { return difficulty == 0 ? 0.8f : (difficulty == 2 ? 1.3f : 1.0f); }
    float creatureSenseMul() const { return difficulty == 0 ? 0.75f : (difficulty == 2 ? 1.35f : 1.0f); }
    bool  creatureEnabled() const { return mode == 0; }
    // Difficulte : nombre de vies et de taches a accomplir (+ vitesse/sens de fropy ci-dessus).
    int   lives() const { return difficulty == 0 ? 5 : (difficulty == 2 ? 1 : 3); }
    int   tasks() const { return difficulty == 0 ? 3 : (difficulty == 2 ? 8 : 5); }
    const char* mapPath() const;
};

// ---------------------------------------------------------------------------
// LobbyManager : lobby d'avant-partie (carte res/maps/lobby.json, une maison
// geante). Les joueurs y apparaissent en arrivant ; l'HOTE (proprietaire du
// lobby Steam, ou le joueur seul en solo) interagit avec les boutons de la
// salle (touche E) : chacun ouvre un panneau HUD pour choisir la difficulte,
// la carte ou le mode de jeu, le dernier lance la partie.
//
// Synchronisation : les options sont ecrites dans les donnees du lobby Steam
// (SetLobbyData "opt_*") par l'hote et relues chaque frame par les autres
// joueurs, qui les voient a droite du HUD. "opt_start" est un compteur : son
// increment declenche le chargement de la carte choisie chez tout le monde (y
// compris un joueur qui rejoint une partie deja lancee).
// ---------------------------------------------------------------------------
class LobbyManager {
public:
    // Charge une carte avec les options de partie (Scene::setMatchParams + loadMap).
    using LoadMapCallback = std::function<void(const char* mapPath, const MatchSettings& settings)>;

    LobbyManager(SteamManager* steam, GLFWwindow* window, ShaderManager* shaderManager,
                 std::vector<std::unique_ptr<TextRenderer>>* textRenderers, LoadMapCallback loadMap);
    ~LobbyManager();

    // Chemin de la carte du lobby.
    static const char* lobbyMapPath();

    // La Scene est sur la carte du lobby (le resume des options s'affiche).
    void setOnLobbyMap(bool onLobbyMap) { m_onLobbyMap = onLobbyMap; }
    bool isOnLobbyMap() const { return m_onLobbyMap; }

    bool isHost() const;
    bool isPanelOpen() const { return m_panelOption >= 0; }

    // Fropy a touche le joueur local : une vie en moins ; plus aucune -> fin de partie
    // puis retour au lobby.
    void onPlayerHit();
    int getLives() const { return m_lives; }
    // Toutes les taches de la partie sont accomplies : victoire, puis retour au lobby.
    void onVictory();

    // Bouton de la salle actionne (nom JSON : difficulty / map / mode / start).
    void onButton(const std::string& name);

    // Chaque frame en jeu : clavier du panneau, synchro des options Steam,
    // detection du lancement.
    void update(float dt);

    // HUD : resume des options (droite), panneau de choix, invite "[E] ..." du
    // bouton vise (aimedLabel, vide = aucun), message temporaire.
    void draw(const std::string& aimedLabel);

private:
    static const std::vector<std::string>& optionLabels(int option);
    static const char* optionTitle(int option);
    int& optionValue(int option);

    void publish();                 // hote -> donnees du lobby Steam
    void readFromLobby();           // donnees du lobby Steam -> m_settings
    void startMatch();
    void notify(const std::string& text) { m_notice = text; m_noticeTimer = 3.0f; }

    SteamManager* m_steam = nullptr;
    GLFWwindow* m_window = nullptr;
    ShaderManager* m_shaderManager = nullptr;
    std::vector<std::unique_ptr<TextRenderer>>* m_textRenderers = nullptr;
    LoadMapCallback m_loadMap;

    MatchSettings m_settings;
    bool m_onLobbyMap = true;
    uint64_t m_lobbyID = 0;         // lobby Steam courant (detection d'un changement)
    int m_startSeen = 0;            // dernier compteur "opt_start" applique

    // Panneau de choix (hote) : option ouverte (-1 = ferme) et ligne surlignee.
    int m_panelOption = -1;
    int m_panelCursor = 0;
    bool m_upPrev = false, m_downPrev = false, m_enterPrev = false, m_escPrev = false;
    std::unique_ptr<class Rectangle> m_panelBg;   // `class` : cf. Menu.h (conflit avec Rectangle() de wingdi.h)

    int m_lives = 0;                // vies restantes (partie en cours)
    float m_gameOverTimer = 0.0f;   // > 0 : fin de partie, retour au lobby a 0

    std::string m_notice;
    float m_noticeTimer = 0.0f;
};
