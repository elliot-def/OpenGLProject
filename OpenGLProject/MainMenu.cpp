#include "MainMenu.h"
#include "Game.h"
#include "DVDShape.h"
#include "ShaderManager.h"
#include "SoundManager.h"
#include "Sound.h"
#include "Renderer.h"
#include "SteamManager.h"
#include "Log.h"

#include "constants/window.h"
#include "constants/menu.h"
#include "constants/resource.h"

MainMenu::MainMenu(Game* game, SoundManager* soundManager, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager, CursorManager* cursorManager, const std::string& t, bool bg) :
        Menu(game, soundManager, textRenderers, shaderManager, cursorManager, t, bg), m_renderer(renderer), m_colorDVDLogo(glm::vec3(1.0f, 1.0f, 1.0f)) {
    addItem("Jouer", Constants::Window::WINDOW_WIDTH / 2, 700, 200, 50, [this]() {
        m_game->changeState(STATE_PLAYING);
        });
    addItem("Multijoueur", Constants::Window::WINDOW_WIDTH / 2, 750, 200, 50, [this]() {
        // Crée un lobby friends-only et ouvre l'overlay d'invitation Steam.
        // L'entrée en jeu se fait via l'invitation acceptée (callback
        // onLobbyEntered -> changeState(STATE_PLAYING)) ou via "Jouer".
        SteamManager* steam = m_game->getSteamManager();
        if (steam && steam->isInitialized()) {
            steam->createLobby();
        } else {
            LOG_WARN("[MainMenu] Steam indisponible : multijoueur desactive.");
        }
        });
    addItem("Options", Constants::Window::WINDOW_WIDTH / 2, 800, 200, 50, [this]() {
        m_game->changeState(STATE_OPTIONS);
        });
    addItem("Quitter", Constants::Window::WINDOW_WIDTH / 2, 900, 200, 50, [this]() {
        m_game->stop();
        });
    // Dans la classe qui instancie le menu :
    auto* dvd = new DVDShape(m_shaderManager->getShader("image/mask"), m_renderer, 100.0f, 100.0f, 330.0f, 195.0f, 200.0f, 180.0f);
    // Musique du menu. Remplace le chemin si tu préfères l'atmosphère horror :
    // "./res/sounds/menu/atmo-horror-ghost-birds-03.wav".
    Sound* menuMusic = m_soundManager->load("menu_music", Constants::Resource::SOUND_MENU_MUSIC, true, 1.0f, 1.0f);
    m_weirdSounds.push_back(m_soundManager->load("weird_sound1", Constants::Resource::SOUND_MENU_GHOST_BIRDS_03, false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound2", Constants::Resource::SOUND_MENU_GHOST_BIRDS_02, false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound3", Constants::Resource::SOUND_MENU_GHOST_BIRDS_01, false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound4", Constants::Resource::SOUND_MENU_SWELLING_DUNGEON, false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound5", Constants::Resource::SOUND_MENU_SILENCE_INVESTIGATION, false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound6", Constants::Resource::SOUND_MENU_FISHMAN_GRUNT, false, 2.0f, 1.0f));

    m_clickSound = m_soundManager->load("menu_click_sound", Constants::Resource::SOUND_MENU_CLICK, false, 6.0f, 1.0f);
    //m_clickSound = m_soundManager->load("menu_click_sound", "./res/sounds/menu/Pokemon (A Button) - Sound Effect (HD).wav", false, 6.0f, 1.0f);

    addShape(0, dvd);
    // Easter egg DVD : dessiné PAR-DESSUS le texte du menu (après le flush)
    addOverlayShape(dvd);
}

MainMenu::~MainMenu() {
	Sound* menuMusic = m_soundManager->get("menu_music");
	if (menuMusic) {
		menuMusic->stop();
		m_soundManager->unload("menu_music");
	}
	for (Sound* sound : m_weirdSounds) {
		if (sound) {
			sound->stop();
			m_soundManager->unload(sound->getFilePath());
		}
	}
}

void MainMenu::update(bool isAFK) {
    auto now = std::chrono::system_clock::now();
    if (std::chrono::duration<float>(now - m_lastWeirdSoundPlayed).count() > Constants::Menu::WEIRD_SOUND_INTERVAL) {
        Sound* weirdSound = m_weirdSounds[std::rand() % m_weirdSounds.size()];
        weirdSound->play();
        m_lastWeirdSoundPlayed = now;
    }

    if (isAFK) {
        // Récupère ou caste la shape DVD (id arbitraire, ex: 0)
        auto it = m_shapes.find(0);
        if (it == m_shapes.end()) return;

        DVDShape* dvd = static_cast<DVDShape*>(it->second->shape.get());
        dvd->update(Constants::Window::WINDOW_WIDTH, Constants::Window::WINDOW_HEIGHT);

        dvd->setIsVisible(true);

    }
    else {
        auto it = m_shapes.find(0);
        if (it == m_shapes.end()) return;
        if (!m_shapes.at(0)->shape->getIsVisible()) return;

        DVDShape* dvd = static_cast<DVDShape*>(it->second->shape.get());
        dvd->setIsVisible(false);
        dvd->setPosition(static_cast<float>(std::rand() % (Constants::Window::WINDOW_WIDTH - static_cast<int>(dvd->getSize().x))),
            static_cast<float>(std::rand() % (Constants::Window::WINDOW_HEIGHT - static_cast<int>(dvd->getSize().y))));
    }
}

void MainMenu::resetWeirdSoundPlayedTime() {
	m_lastWeirdSoundPlayed = std::chrono::system_clock::now();
}