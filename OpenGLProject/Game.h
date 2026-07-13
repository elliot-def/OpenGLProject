#pragma once

#define NOMINMAX
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>
#include <memory>

#include "gamestate.h"
#include "SoundManager.h"

// Déclarations anticipées
class Window;
class Renderer;
class CollisionManager;
class TextureManager;
class ShaderManager;
class InputManager;
class Player;
class Camera;
class Cube;
class LightSource;
class LightManager;
class Socket;
class MenuManager;
class TextRenderer;
class ModelEntity;

class Game {
public:
    Game();
    ~Game(); // tu peux même le supprimer si tu n’as rien de spécial à libérer

    void run();
    void stop();
    void changeState(GameState state);
private:
    std::vector<std::unique_ptr<Cube>> m_cubes;
    std::vector<std::unique_ptr<Cube>> m_alphacubes;
    std::vector<std::unique_ptr<LightSource>> m_lights;

    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<CollisionManager> m_collisionManager;
    std::unique_ptr<TextureManager> m_textureManager;
    std::unique_ptr<ShaderManager> m_shaderManager;
    std::unique_ptr<InputManager> m_inputManager;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<LightManager> m_lightManager;
    std::unique_ptr<SoundManager> m_soundManager;
    std::unique_ptr<Socket> m_socket;
    std::unique_ptr<MenuManager> m_menuManager;
    std::unique_ptr<std::vector<std::unique_ptr<TextRenderer>>> m_textRenderers;

    ModelEntity* m_modelEntity;

    bool m_isRunning = true;

    void initialize();
    void update();
    void draw();
};
