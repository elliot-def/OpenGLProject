#pragma once

#define NOMINMAX
#define GLM_ENABLE_EXPERIMENTAL

#include <atomic>
#include <thread>
#include <vector>
#include <memory>

#include "gamestate.h"
#include "SoundManager.h"

// D�clarations anticip�es
class Window;
class Renderer;
class CollisionManager;
class CursorManager;
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
class FirstPersonArms;
class SteamManager;
class LoadingScreen;
class Skybox;

class Game {
public:
    Game();
    Game(int argc, char* argv[]);
    ~Game();

    void run();
    void stop();
    void changeState(GameState state);
private:
    std::vector<std::unique_ptr<Cube>> m_cubes;
    std::vector<std::unique_ptr<Cube>> m_alphacubes;
    std::vector<std::unique_ptr<LightSource>> m_lights;

    std::unique_ptr<CursorManager> m_cursorManager;
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
    ModelEntity* m_fropyEntity;
    ModelEntity* m_humanEntity = nullptr;
    std::unique_ptr<FirstPersonArms> m_firstPersonArms;
    std::unique_ptr<SteamManager> m_steamManager;
    std::unique_ptr<Skybox> m_skybox;

    bool m_isRunning = true;
    
    int m_argc;
    char** m_argv;

    // Phases de chargement (modèles 3D en thread séparé)
    enum class InitPhase { LOADING, READY };
    InitPhase m_initPhase = InitPhase::LOADING;

    // Thread de chargement des modèles 3D (contexte GL partagé)
    std::atomic<bool> m_loadingDone{false};
    std::thread       m_loadingThread;
    GLFWwindow*       m_loaderWindow = nullptr;
    void loadModelsAsync();  // exécuté sur le thread de chargement

    static constexpr float LOADING_EXTRA_DELAY   = 0.5f;   // délai après 100%
    static constexpr float LOADING_FADE_DURATION = 0.5f;   // durée du fondu
    float m_loadingFadeTimer = 0.0f;

    std::unique_ptr<LoadingScreen> m_loadingScreen;

    void initialize();     // Window + Renderer + Steam async start (+ ressources si Steam prêt)
    void loadResources();  // Chargement des ressources lourdes (textures, modèles, sons...)
    void update();
    void draw();
};
