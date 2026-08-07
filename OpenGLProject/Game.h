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
class CharacterAnimationController;
class ModelLoader;

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

    // Vues non-propriétaires : le ModelLoader possède les entités et les
    // détruit ; Game ne garde que des pointeurs (remplis via adoptLoadedEntities()).
    ModelEntity* m_modelEntity = nullptr;
    ModelEntity* m_fropyEntity = nullptr;
    ModelEntity* m_humanEntity = nullptr;
    FirstPersonArms* m_firstPersonArms = nullptr;
    std::unique_ptr<SteamManager> m_steamManager;
    std::unique_ptr<Skybox> m_skybox;
    CharacterAnimationController* m_characterAnim = nullptr;
    std::unique_ptr<ModelLoader> m_modelLoader;  // possède les entités 3D chargées

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
    // Copie dans Game les pointeurs vers les entités possédées par m_modelLoader.
    // À appeler dès que le chargement est terminé (thread joint ou fallback synchrone).
    void adoptLoadedEntities();

    static constexpr float LOADING_EXTRA_DELAY   = 0.5f;   // délai après 100%
    static constexpr float LOADING_FADE_DURATION = 0.5f;   // durée du fondu
    float m_loadingFadeTimer = 0.0f;

    std::unique_ptr<LoadingScreen> m_loadingScreen;

    void initialize();     // Window + Renderer + Steam async start (+ ressources si Steam prêt)
    void loadResources();  // Chargement des ressources lourdes (textures, modèles, sons...)
    void update();
    void draw();

    // TextRenderer : délimitation de frame pour le batching des glyphes
    // (1 seul draw call de texte par frame au lieu de ~2 draw calls par glyphe).
    void beginTextFrame();
    void flushTextFrame();
};
