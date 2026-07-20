#include "OptionsMenu.h"
#include "Game.h"
#include "constants.h"
#include "CursorManager.h"
#include "File.h"
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

void OptionsMenu::createOptions(bool isMuted, float volume) {
    addCheckbox(m_cursorManager, "Son", Constants::WINDOW_WIDTH / 2, 700, 50, !isMuted, [this](bool isChecked) {
        m_soundManager->setMute(!isChecked);
        });

    // Note : Pense à adapter la valeur par défaut (ici récupérée via le SoundManager si possible, sinon 1.0f)
    addRange(m_cursorManager, "Volume", Constants::WINDOW_WIDTH / 2, 800, 300, 25, 0, 2, volume, [this](float volume) {
        m_soundManager->setMasterVolume(volume);
        });

    addItem("Retour", Constants::WINDOW_WIDTH / 2, 900, 200, 50, [this]() {
        m_game->changeState(m_previousState == STATE_PLAYING ? STATE_PAUSED : STATE_MENU);
        exportJSON();
        });
}

void OptionsMenu::loadJSON() {
    File optionFile(Constants::JSON_OPTION_PATH);

    // On vérifie directement si le fichier existe
    if (!optionFile.exists()) {
        std::cout << "[Options] Aucun fichier de sauvegarde trouvé. Utilisation des valeurs par défaut.\n";
        createOptions(false, 1.0f);
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

        createOptions(isMuted, volume);

        // Application des paramètres au SoundManager
        if (m_soundManager) {
            m_soundManager->setMute(isMuted);
            m_soundManager->setMasterVolume(volume);
        }

        std::cout << "[Options] Paramètres chargés avec succès.\n";
    }
    catch (const json::parse_error& e) {
        createOptions(false, 1.0f);
        std::cerr << "[Options] Erreur lors de la lecture du JSON : " << e.what() << "\n";
    }
}

void OptionsMenu::exportJSON() {
    File optionFile(Constants::JSON_OPTION_PATH);

    try {
        json j;

        // On récupère les valeurs actuelles du soundManager pour les sauvegarder
        if (m_soundManager) {
            j["muted"] = m_soundManager->isMuted();
            j["volume"] = m_soundManager->getMasterVolume();
        }
        else {
            throw std::runtime_error("SoundManager non initialisé, impossible d'exporter les paramètres.");
        }

        // On génère la string JSON et on l'écrit d'un coup avec writeText()
        if (!optionFile.writeText(j.dump(4))) {
            throw std::runtime_error("Impossible d'écrire dans le fichier d'options.");
        }

        std::cout << "[Options] Paramètres exportés avec succès.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Options] Erreur lors de l'export JSON : " << e.what() << "\n";
    }
}