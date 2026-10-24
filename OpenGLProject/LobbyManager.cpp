#include "LobbyManager.h"

#include <algorithm>
#include <cstdlib>
#include <random>

#include "Rectangle.h"
#include "ShaderManager.h"
#include "SteamManager.h"
#include "TextRenderer.h"
#include "Log.h"

#include "constants/window.h"
#include "constants/resource.h"
#include "constants/color.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace {
    // Index des options (ordre des boutons de la salle)
    constexpr int OPT_DIFFICULTY = 0;
    constexpr int OPT_MAP = 1;
    constexpr int OPT_MODE = 2;
    constexpr int OPT_COUNT = 3;

    const char* const kLobbyKeys[OPT_COUNT] = { "opt_difficulty", "opt_map", "opt_mode" };
    constexpr const char* kStartKey = "opt_start";
    constexpr const char* kSeedKey = "opt_seed";    // graine du tirage des taches (MatchSettings::seed)

    constexpr float kScale = 0.42f;        // texte du HUD
    constexpr float kTitleScale = 0.55f;
    constexpr float kLineH = 46.0f;
    constexpr float kMargin = 40.0f;
}

const char* MatchSettings::mapPath() const {
    return map == 1 ? Constants::Resource::MAP_VILLAGE_NIGHT : (map == 2 ? Constants::Resource::MAP_BATIMENT : Constants::Resource::MAP_VILLAGE_DAY);
}

const char* LobbyManager::lobbyMapPath() {
    return Constants::Resource::MAP_LOBBY;
}

LobbyManager::LobbyManager(SteamManager* steam, GLFWwindow* window, ShaderManager* shaderManager,
                           std::vector<std::unique_ptr<TextRenderer>>* textRenderers, LoadMapCallback loadMap)
    : m_steam(steam), m_window(window), m_shaderManager(shaderManager),
      m_textRenderers(textRenderers), m_loadMap(std::move(loadMap)) {
}

LobbyManager::~LobbyManager() = default;

const std::vector<std::string>& LobbyManager::optionLabels(int option) {
    static const std::vector<std::string> difficulty = { "Facile", "Normal", "Difficile" };
    static const std::vector<std::string> map = { "Village (jour)", "Village (nuit)", "Batiment (ecole x3)" };
    static const std::vector<std::string> mode = { "Chasse (fropy actif)", "Exploration (sans fropy)" };
    return option == OPT_DIFFICULTY ? difficulty : (option == OPT_MAP ? map : mode);
}

const char* LobbyManager::optionTitle(int option) {
    return option == OPT_DIFFICULTY ? "Difficulte" : (option == OPT_MAP ? "Carte" : "Mode de jeu");
}

int& LobbyManager::optionValue(int option) {
    return option == OPT_DIFFICULTY ? m_settings.difficulty
         : (option == OPT_MAP ? m_settings.map : m_settings.mode);
}

bool LobbyManager::isHost() const {
    // Solo (pas de lobby Steam) : le joueur local decide.
    if (!m_steam || !m_steam->isInLobby()) return true;
    return m_steam->isLobbyOwner();
}

// ── Steam ────────────────────────────────────────────────────────────────────

void LobbyManager::publish() {
    if (!m_steam || !m_steam->isInLobby() || !isHost()) return;
    for (int i = 0; i < OPT_COUNT; ++i) {
        m_steam->setLobbyData(kLobbyKeys[i], std::to_string(optionValue(i)).c_str());
    }
    m_steam->setLobbyData(kSeedKey, std::to_string(m_settings.seed).c_str());
    m_steam->setLobbyData(kStartKey, std::to_string(m_startSeen).c_str());
}

void LobbyManager::readFromLobby() {
    for (int i = 0; i < OPT_COUNT; ++i) {
        const std::string v = m_steam->getLobbyData(kLobbyKeys[i]);
        if (v.empty()) continue;
        const int n = static_cast<int>(optionLabels(i).size());
        optionValue(i) = std::clamp(std::atoi(v.c_str()), 0, n - 1);   // valeur distante bornee
    }
    m_settings.seed = static_cast<unsigned>(std::strtoul(m_steam->getLobbyData(kSeedKey).c_str(), nullptr, 10));
}

void LobbyManager::startMatch() {
    m_onLobbyMap = false;
    m_panelOption = -1;
    m_lives = m_settings.lives();
    m_gameOverTimer = 0.0f;
    LOG_INFO("[Lobby] Lancement : difficulte=%d carte=%d mode=%d", m_settings.difficulty, m_settings.map, m_settings.mode);
    if (m_loadMap) m_loadMap(m_settings.mapPath(), m_settings);
}

void LobbyManager::onPlayerHit() {
    if (m_onLobbyMap || m_lives <= 0) return;
    --m_lives;
    if (m_lives > 0) {
        notify("Fropy vous a attrape ! Vies restantes : " + std::to_string(m_lives));
    } else {
        notify("Fin de la partie : plus de vies");
        m_gameOverTimer = 4.0f;
    }
}

void LobbyManager::onVictory() {
    if (m_onLobbyMap || m_gameOverTimer > 0.0f) return;
    notify("Toutes les taches sont accomplies : victoire !");
    m_noticeTimer = 5.0f;
    m_gameOverTimer = 5.0f;   // retour a la salle du lobby
}

// ── Boutons de la salle ──────────────────────────────────────────────────────

void LobbyManager::onButton(const std::string& name) {
    if (!m_onLobbyMap) return;
    if (!isHost()) {
        notify("Seul l'hote peut choisir les options de la partie");
        return;
    }
    if (name == "start") {
        ++m_startSeen;
        m_settings.seed = std::random_device{}();   // tirage des taches, publie avec le lancement
        publish();       // les autres joueurs voient le compteur changer et chargent la carte
        startMatch();
        return;
    }
    const int option = name == "difficulty" ? OPT_DIFFICULTY : (name == "map" ? OPT_MAP : (name == "mode" ? OPT_MODE : -1));
    if (option < 0) return;
    m_panelOption = option;
    m_panelCursor = optionValue(option);
}

// ── Mise a jour ──────────────────────────────────────────────────────────────

void LobbyManager::update(float dt) {
    m_noticeTimer = std::max(0.0f, m_noticeTimer - dt);

    // Fin de partie : retour a la salle du lobby apres quelques secondes.
    if (m_gameOverTimer > 0.0f) {
        m_gameOverTimer -= dt;
        if (m_gameOverTimer <= 0.0f) {
            m_onLobbyMap = true;
            m_lives = 0;
            if (m_loadMap) m_loadMap(lobbyMapPath(), m_settings);
        }
    }

    // Synchro Steam
    if (m_steam && m_steam->isInLobby()) {
        const uint64_t lobby = m_steam->getCurrentLobbyID().ConvertToUint64();
        if (lobby != m_lobbyID) {          // nouveau lobby : on repart de zero
            m_lobbyID = lobby;
            m_startSeen = 0;
            if (isHost()) {
                publish();
            } else if (!m_onLobbyMap && m_steam->getLobbyData(kStartKey) == "0") {
                // Partie pas encore lancee chez l'hote : retour dans la salle du lobby.
                m_onLobbyMap = true;
                if (m_loadMap) m_loadMap(lobbyMapPath(), m_settings);
            }
        }
        if (!isHost()) {
            readFromLobby();
            const int start = std::atoi(m_steam->getLobbyData(kStartKey).c_str());
            if (start > m_startSeen) {     // l'hote a lance (ou partie deja en cours)
                m_startSeen = start;
                startMatch();
            }
        }
    } else {
        m_lobbyID = 0;
    }

    // Clavier du panneau (fronts montants pour la navigation, RELACHEMENT pour
    // Entree/Echap : sinon la touche encore enfoncee serait vue par le chat ou
    // le menu pause des la frame suivante).
    if (!isPanelOpen() || !m_window) return;
    const bool up = glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS;
    const bool down = glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS;
    const bool enter = glfwGetKey(m_window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_KP_ENTER) == GLFW_PRESS
                    || glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
    const bool esc = glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    const int n = static_cast<int>(optionLabels(m_panelOption).size());

    if (up && !m_upPrev) m_panelCursor = (m_panelCursor + n - 1) % n;
    if (down && !m_downPrev) m_panelCursor = (m_panelCursor + 1) % n;
    if (!enter && m_enterPrev) {           // valide
        optionValue(m_panelOption) = m_panelCursor;
        publish();
        notify(std::string(optionTitle(m_panelOption)) + " : " + optionLabels(m_panelOption)[m_panelCursor]);
        m_panelOption = -1;
    } else if (!esc && m_escPrev) {        // annule
        m_panelOption = -1;
    }
    m_upPrev = up; m_downPrev = down; m_enterPrev = enter; m_escPrev = esc;
}

// ── HUD ──────────────────────────────────────────────────────────────────────

void LobbyManager::draw(const std::string& aimedLabel) {
    if (!m_textRenderers || m_textRenderers->empty()) return;
    TextRenderer* tr = (*m_textRenderers)[0].get();
    const float W = static_cast<float>(Constants::Window::WINDOW_WIDTH);
    const float H = static_cast<float>(Constants::Window::WINDOW_HEIGHT);
    const glm::vec3 text = Constants::Color::LINEN;
    const glm::vec3 accent = Constants::Color::VANILLA_CUSTARD;

    auto drawRight = [&](const std::string& s, float y, float scale, const glm::vec3& c) {
        const float x = W - kMargin - tr->getTextWidth(s, scale);
        tr->renderText(s, x + 2.0f, y + 2.0f, scale, 0.0f, 0.0f, 0.0f);
        tr->renderText(s, x, y, scale, c.r, c.g, c.b);
    };
    auto drawCenter = [&](const std::string& s, float y, float scale, const glm::vec3& c) {
        const float x = (W - tr->getTextWidth(s, scale)) * 0.5f;
        tr->renderText(s, x + 2.0f, y + 2.0f, scale, 0.0f, 0.0f, 0.0f);
        tr->renderText(s, x, y, scale, c.r, c.g, c.b);
    };

    // Resume des options, a droite (tous les joueurs, dans la salle du lobby)
    if (m_onLobbyMap) {
        float y = 120.0f;
        drawRight("LOBBY", y, kTitleScale, accent);
        y += kLineH * 1.2f;
        std::string host = "Hote : vous";
        if (m_steam && m_steam->isInLobby() && !isHost()) {
            host = std::string("Hote : ") + m_steam->getPersonaName(m_steam->getLobbyOwner());
        }
        drawRight(host, y, kScale, text);
        y += kLineH;
        for (int i = 0; i < OPT_COUNT; ++i) {
            drawRight(std::string(optionTitle(i)) + " : " + optionLabels(i)[optionValue(i)], y, kScale, text);
            y += kLineH;
        }
        drawRight("Vies : " + std::to_string(m_settings.lives()) + "   Taches : " + std::to_string(m_settings.tasks()), y, kScale, text);
        y += kLineH;
        if (!isHost()) drawRight("En attente du lancement par l'hote...", y + 10.0f, kScale * 0.85f, Constants::Color::TROPICAL_TEAL);
    }

    // Partie en cours (fropy actif) : vies restantes, haut a gauche
    if (!m_onLobbyMap && m_settings.creatureEnabled()) {
        const std::string lives = "Vies : " + std::to_string(m_lives) + " / " + std::to_string(m_settings.lives());
        tr->renderText(lives, kMargin + 2.0f, 80.0f + 2.0f, kScale, 0.0f, 0.0f, 0.0f);
        tr->renderText(lives, kMargin, 80.0f, kScale, 0.95f, 0.45f, 0.4f);
    }   // taches : liste du QuestManager, sous les vies

    // Invite du bouton vise
    if (!aimedLabel.empty() && !isPanelOpen()) {
        drawCenter("[E] " + aimedLabel, H * 0.72f, kScale, accent);
    }

    // Message temporaire
    if (m_noticeTimer > 0.0f) {
        drawCenter(m_notice, H * 0.80f, kScale, Constants::Color::TROPICAL_TEAL);
    }

    // Panneau de choix (hote)
    if (isPanelOpen()) {
        const auto& labels = optionLabels(m_panelOption);
        const float panelW = 760.0f;
        const float panelH = 150.0f + labels.size() * kLineH;
        if (!m_panelBg && m_shaderManager) {
            m_panelBg = std::make_unique<class Rectangle>(m_shaderManager->getShader("shape"),
                W * 0.5f, H * 0.5f, panelW, panelH, Constants::Color::SHADOW_GREY);
        }
        if (m_panelBg) {
            m_panelBg->setSize(panelW, panelH);
            m_panelBg->setPosition(W * 0.5f, H * 0.5f);
            glDisable(GL_DEPTH_TEST);
            m_panelBg->draw();
            glEnable(GL_DEPTH_TEST);
        }
        float y = H * 0.5f - panelH * 0.5f + 60.0f;
        drawCenter(optionTitle(m_panelOption), y, kTitleScale, accent);
        y += kLineH * 1.3f;
        for (size_t i = 0; i < labels.size(); ++i) {
            const bool sel = static_cast<int>(i) == m_panelCursor;
            drawCenter(sel ? "> " + labels[i] + " <" : labels[i], y, kScale, sel ? accent : text);
            y += kLineH;
        }
        drawCenter("Haut/Bas : choisir   Entree : valider   Echap : fermer", y + 10.0f, kScale * 0.7f,
                   Constants::Color::TROPICAL_TEAL);
    }
}
