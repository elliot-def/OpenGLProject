#pragma once

#define NOMINMAX
#define GLM_ENABLE_EXPERIMENTAL

#include <atomic>
#include <thread>
#include <vector>
#include <memory>

#include "gamestate.h"
#include "SoundManager.h"

// Déclarations anticipées
class Window;
class Renderer;
class CollisionManager;
class CursorManager;
class TextureManager;
class ShaderManager;
class InputManager;
class Player;
class Camera;
class LightManager;
class Socket;
class MenuManager;
class TextRenderer;
class ModelEntity;
class FirstPersonArms;
class SteamManager;
class LoadingScreen;
class ModelLoader;
class Scene;
class MultiplayerManager;
class LobbyChat;
class VoiceChat;

class Game {
public:
    Game();
    Game(int argc, char* argv[]);
    ~Game();

    void run();
    void stop();
    // changeState : restoreFocus=true indique un retour vers un menu deja
    // visite (la selection manette sauvegardee est restauree au lieu de
    // repartir du premier element).
    void changeState(GameState state, bool restoreFocus = false);

    // Acces au gestionnaire de menus (ex: sous-menu de touches des Options)
    MenuManager* getMenuManager() { return m_menuManager.get(); }

    // Acces au gestionnaire Steam (utilise pour choisir le mode manette)
    SteamManager* getSteamManager() { return m_steamManager.get(); }

    // Acces au gestionnaire d'entrees (ex: sensibilites dans les Options)
    InputManager* getInputManager() { return m_inputManager.get(); }

    // Bascule le HUD debug du personnage 3P (touche F3, voir InputManager)
    void toggleDebugHUD();
private:
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

    // Monde 3D : tout ce qui est gameplay/rendu (cubes, lumières, entités,
    // skybox) vit dans Scene. Game ne garde que l'orchestration.
    std::unique_ptr<Scene> m_scene;

    // Vues non-propriétaires : le ModelLoader possède les entités et les
    // détruit ; Game ne garde que des pointeurs (remplis via adoptLoadedEntities()).
    ModelEntity* m_modelEntity = nullptr;
    ModelEntity* m_fropyEntity = nullptr;
    ModelEntity* m_humanEntity = nullptr;
    FirstPersonArms* m_firstPersonArms = nullptr;
    std::unique_ptr<SteamManager> m_steamManager;
    std::unique_ptr<ModelLoader> m_modelLoader;  // possède les entités 3D chargées
    // Joueurs distants du lobby Steam (possède leurs ModelEntity, les dessine).
    std::unique_ptr<MultiplayerManager> m_multiplayerManager;

    // Chat du lobby Steam (logs systeme + messages des joueurs, saisie Entree).
    std::unique_ptr<LobbyChat> m_lobbyChat;

    // Chat vocal du lobby Steam (capture micro + lecture des voix distantes).
    std::unique_ptr<VoiceChat> m_voiceChat;

    bool m_isRunning = true;

    // Mode hors-ligne forcé : argument -offline / -nosteam au lancement.
    // Steam n'est alors PAS initialisé (ni lancé) → aucune capture de la
    // manette par Steam Input, XInput/GLFW reste seul maître.
    bool m_offlineMode = false;
    
    int m_argc;
    char** m_argv;

    // HUD debug des animations du personnage 3P (touche F3).
    // Off par défaut : coûteux en FPS (concats std::string + texte par frame).
    bool m_debugHUD = false;

    // Phases de chargement (modèles 3D en thread séparé)
    enum class InitPhase { LOADING, READY };
    InitPhase m_initPhase = InitPhase::LOADING;

    // Thread de chargement des modèles 3D (contexte GL partagé)
    std::atomic<bool> m_loadingDone{false};
    std::thread       m_loadingThread;
    GLFWwindow*       m_loaderWindow = nullptr;
    // Copie dans Game les pointeurs vers les entités possédées par m_modelLoader
    // puis les transmet à Scene. À appeler dès que le chargement est terminé
    // (thread joint ou fallback synchrone).
    void adoptLoadedEntities();

    static constexpr float LOADING_EXTRA_DELAY   = 0.5f;   // délai après 100%
    static constexpr float LOADING_FADE_DURATION = 0.5f;   // durée du fondu
    float m_loadingFadeTimer = 0.0f;

    std::unique_ptr<LoadingScreen> m_loadingScreen;

    void initialize();     // Window + Renderer + Steam async start (+ ressources si Steam prêt)
    void loadResources();  // Chargement des ressources lourdes (textures, modèles, sons...)
    void update();

    // Simulation du monde (physique/gravité, caméra, lumières, audio 3D,
    // synchro P2P des joueurs distants). Appelée par update() en jeu ET par
    // la boucle quand un menu est ouvert : le personnage continue de tomber
    // et son état reste diffusé aux autres joueurs du lobby (sinon il était
    // gelé en l'air et disparaissait chez les pairs après ~5 s de silence
    // P2P, STALE_TIMEOUT).
    void updateWorld(float dt);
    void draw();

    // Notification de bascule clavier/souris <-> manette (icônes kenney).
    // À appeler entre beginTextFrame() et flushTextFrame().
    void drawInputNotification();

    // TextRenderer : délimitation de frame pour le batching des glyphes
    // (1 seul draw call de texte par frame au lieu de ~2 draw calls par glyphe).
    void beginTextFrame();
    void flushTextFrame();
};
