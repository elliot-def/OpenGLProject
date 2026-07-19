#include "OptionsMenu.h"
#include "Game.h"
#include "constants.h"
#include "CursorManager.h"
#include <iostream>
#include <fstream> // Ajouté pour la gestion des fichiers (ifstream / ofstream)
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
    std::ifstream file(Constants::JSON_OPTION_PATH);
    if (!file.is_open()) {
        std::cout << "[Options] Aucun fichier de sauvegarde trouvé. Utilisation des valeurs par défaut.\n";
        createOptions(false, 1.0f);
        return;
    }

    try {
        json j;
        file >> j;

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

    file.close();
}

void OptionsMenu::exportJSON() {
    std::ofstream file(Constants::JSON_OPTION_PATH);
    if (!file.is_open()) {
        std::cerr << "[Options] Impossible de créer ou d'ouvrir le fichier " << Constants::JSON_OPTION_PATH << " pour l'écriture.\n";
        return;
    }

    try {
        json j;

        // On récupère les valeurs actuelles du soundManager pour les sauvegarder
        if (m_soundManager) {
            j["muted"] = m_soundManager->isMuted();
            // Attention : assure-toi d'avoir une méthode pour récupérer le volume dans ton SoundManager (ex: getMasterVolume())
            // Si elle n'existe pas, il faudra stocker le volume dans une variable membre de OptionsMenu
            j["volume"] = m_soundManager->getMasterVolume();
        }
        else {
			throw std::runtime_error("SoundManager non initialisé, impossible d'exporter les paramètres.");
        }

        // Écriture dans le fichier avec une indentation de 4 espaces pour que ce soit lisible
        file << j.dump(4);
        std::cout << "[Options] Paramètres exportés avec succès.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Options] Erreur lors de l'export JSON : " << e.what() << "\n";
    }

    file.close();
}