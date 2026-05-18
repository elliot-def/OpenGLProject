#include "MainMenu.h"
#include "Game.h"
#include "DVDShape.h"
#include "ShaderManager.h"
#include "Renderer.h"

MainMenu::MainMenu(Game* game, Renderer* renderer, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager, const std::string& t, bool bg) : Menu(game, textRenderers, shaderManager, t, bg), m_renderer(renderer), m_colorDVDLogo(glm::vec3(1.0f, 1.0f, 1.0f)) {
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
    auto* dvd = new DVDShape(m_shaderManager->getShader("shape"), m_renderer, 100.0f, 100.0f, 100.0f, 50.0f, 10.0f, 8.0f);
    addShape(0, dvd);
}

void MainMenu::update(bool isAFK) {
    if(isAFK) {
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

        DVDShape* dvd = static_cast<DVDShape*>(it->second);
        dvd->setIsVisible(false);
        dvd->setPosition(100, 100);
    }
}
