#include "OptionsMenu.h"
#include "Game.h"
#include "MenuManager.h"
#include "CursorManager.h"
#include "File.h"

#include "constants/window.h"
#include "constants/file.h"

#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

OptionsMenu::OptionsMenu(Game* game, SoundManager* soundManager, GameState& previousState, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager, CursorManager* cursorManager)
    : Menu(game, soundManager, textRenderers, shaderManager, cursorManager, "Options", false), m_previousState(previousState)
{
    // On charge les options enregistrées avant de créer les éléments visuels
    loadJSON();
}

OptionsMenu::~OptionsMenu() {
    // Nettoyage des ressources si nécessaire
}

void OptionsMenu::createOptions(bool isMuted, float volume, float musicVolume) {
    // Sous-menu de reconfiguration des touches clavier/souris (res/keys.json)
    // Contient aussi le slider de sensibilite de la souris.
    addItem("Clavier & Souris", Constants::Window::WINDOW_WIDTH / 2, 620, 200, 50, [this]() {
        m_game->getMenuManager()->showKeyBindings();
        });

    // Sous-menu de reconfiguration des boutons de manette + sensibilite camera
    addItem("Manette", Constants::Window::WINDOW_WIDTH / 2, 690, 200, 50, [this]() {
        m_game->getMenuManager()->showControllerBindings();
        });

    addCheckbox("Son", Constants::Window::WINDOW_WIDTH / 2, 760, 40, !isMuted, [this](bool isChecked) {
        m_soundManager->setMute(!isChecked);
        });

    addRange("Volume", Constants::Window::WINDOW_WIDTH / 2, 820, 300, 25, 0, 2, volume, [this](float volume) {
        m_soundManager->setMasterVolume(volume);
        });

    addRange("Volume musique", Constants::Window::WINDOW_WIDTH / 2, 880, 300, 25, 0, 2, musicVolume, [this](float musicVolume) {
        m_soundManager->setMusicVolume(musicVolume);
        });

    addItem("Retour", Constants::Window::WINDOW_WIDTH / 2, 950, 200, 50, [this]() {
        // Retour vers le menu precedent : on restaure sa selection manette.
        m_game->changeState(m_previousState == STATE_PLAYING ? STATE_PAUSED : STATE_MENU, true);
        exportJSON();
        });
}

void OptionsMenu::loadJSON() {
    File optionFile(Constants::File::JSON_OPTION_PATH);

    // On vérifie directement si le fichier existe
    if (!optionFile.exists()) {
        std::cout << "[Options] Aucun fichier de sauvegarde trouve. Utilisation des valeurs par defaut.\n";
        createOptions(false, 1.0f, 0.5f);
        return;
    }

    try {
        // On lit tout le contenu du fichier d'un coup grâce à readAll()
        std::string content = optionFile.readAll();

        // On parse la string JSON reçue
        json j = json::parse(content);

        // Extraction des données avec valeurs de secours (fallback) si la clé est absente
        bool isMuted = j.value("muted", false);
        float volume = j.value("volume", 1.0f);
        float musicVolume = j.value("musicVolume", 0.5f);

        createOptions(isMuted, volume, musicVolume);

        // Application des parametres au SoundManager
        if (m_soundManager) {
            m_soundManager->setMute(isMuted);
            m_soundManager->setMasterVolume(volume);
            m_soundManager->setMusicVolume(musicVolume);
        }
        // Les bindings et les sensibilites sont charges par l'InputManager
        // depuis res/keys.json (voir InputManager::loadKeyBindings).

        std::cout << "[Options] Parametres charges avec succes.\n";
    }
    catch (const json::parse_error& e) {
        createOptions(false, 1.0f, 0.5f);
        std::cerr << "[Options] Erreur lors de la lecture du JSON : " << e.what() << "\n";
    }
}

void OptionsMenu::exportJSON() {
    File optionFile(Constants::File::JSON_OPTION_PATH);

    try {
        json j;

        // On récupère les valeurs actuelles du soundManager pour les sauvegarder
        if (m_soundManager) {
            j["muted"] = m_soundManager->isMuted();
            j["volume"] = m_soundManager->getMasterVolume();
            j["musicVolume"] = m_soundManager->getMusicVolume();
        }
        else {
            throw std::runtime_error("SoundManager non initialise, impossible d'exporter les parametres.");
        }

        // On génère la string JSON et on l'écrit d'un coup avec writeText()
        if (!optionFile.writeText(j.dump(4))) {
            throw std::runtime_error("Impossible d'ecrire dans le fichier d'options.");
        }

        std::cout << "[Options] Parametres exportes avec succes.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Options] Erreur lors de l'export JSON : " << e.what() << "\n";
    }
}
