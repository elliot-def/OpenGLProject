#include "MainMenu.h"
#include "Game.h"
#include "DVDShape.h"
#include "ShaderManager.h"
#include "SoundManager.h"
#include "Sound.h"
#include "Renderer.h"

MainMenu::MainMenu(Game* game, SoundManager* soundManager, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager, const std::string& t, bool bg) :
        Menu(game, soundManager, textRenderers, shaderManager, t, bg), m_renderer(renderer), m_colorDVDLogo(glm::vec3(1.0f, 1.0f, 1.0f)) {
    addItem("Jouer", Constants::WINDOW_WIDTH / 2, 700, 200, 50, [this]() {
        m_game->changeState(STATE_PLAYING);
        });
    addItem("Options", Constants::WINDOW_WIDTH / 2, 800, 200, 50, [this]() {
        m_game->changeState(STATE_OPTIONS);
        });
    addItem("Quitter", Constants::WINDOW_WIDTH / 2, 900, 200, 50, [this]() {
        m_game->stop();
        });
    // Dans la classe qui instancie le menu :
    auto* dvd = new DVDShape(m_shaderManager->getShader("image/masque"), m_renderer, 100.0f, 100.0f, 330.0f, 195.0f, 200.0f, 180.0f);
    Sound* menuMusic = m_soundManager->load("menu_music", "./res/sounds/menu/industry-garage-ventilation-system-01.wav", true, 1.0f, 1.0f);
    try {
        Sound* menuMusic = m_soundManager->load("menu_music", "./res/sounds/menu/./res/sounds/menu/atmo-horror-ghost-birds-03.wav", true, 1.0f, 1.0f);
    }
    catch (const std::exception& e) {
        std::cerr << "[MainMenu] " << e.what() << std::endl;
    }
    m_weirdSounds.push_back(m_soundManager->load("weird_sound1", "./res/sounds/menu/atmo-horror-ghost-birds-03.wav", false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound2", "./res/sounds/menu/atmo-horror-ghost-birds-02.wav", false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound3", "./res/sounds/menu/atmo-horror-ghost-birds-01.wav", false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound4", "./res/sounds/menu/musical-horror-swelling-dungeon-01.wav", false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound5", "./res/sounds/menu/musical-horror-silence-investigation-01.wav", false, 2.0f, 1.0f));
    m_weirdSounds.push_back(m_soundManager->load("weird_sound6", "./res/sounds/menu/creature-humanoid-fishman-grunt-02.wav", false, 2.0f, 1.0f));

    m_clickSound = m_soundManager->load("menu_click_sound", "./res/sounds/menu/ui-click-generic-plastic-01.wav", false, 6.0f, 1.0f);

    addShape(0, dvd);
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
    if (std::chrono::duration<float>(now - m_lastWeirdSoundPlayed).count() > Constants::WEIRD_SOUND_INTERVAL) {
        Sound* weirdSound = m_weirdSounds[std::rand() % m_weirdSounds.size()];
        weirdSound->play();
        m_lastWeirdSoundPlayed = now;
    }

    if (isAFK) {
        // Récupère ou caste la shape DVD (id arbitraire, ex: 0)
        auto it = m_shapes.find(0);
        if (it == m_shapes.end()) return;

        DVDShape* dvd = static_cast<DVDShape*>(it->second);
        dvd->update(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT);

        dvd->setIsVisible(true);

    }
    else {
        auto it = m_shapes.find(0);
        if (it == m_shapes.end()) return;
        if (!m_shapes[0]->getIsVisible()) return;

        DVDShape* dvd = static_cast<DVDShape*>(it->second);
        dvd->setIsVisible(false);
        dvd->setPosition(static_cast<float>(std::rand() % (Constants::WINDOW_WIDTH - static_cast<int>(dvd->getSize().x))),
            static_cast<float>(std::rand() % (Constants::WINDOW_HEIGHT - static_cast<int>(dvd->getSize().y))));
    }
}