#pragma once

#define NOMINMAX
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>
#include <memory>

#include "gamestate.h"
#include "SoundManager.h"
#include "Animator.h"

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
class Model;

class Game {
public:
    Game();
    ~Game(); // tu peux m�me le supprimer si tu n�as rien de sp�cial � lib�rer

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

    // Avant-bras rigues first-person : modele skinned importe depuis
    // res/rigging/arm/ + Animator pour la lecture des AnimationClip.
    std::unique_ptr<Model>    m_armsModel;
    std::unique_ptr<Animator> m_armsAnimator;

    bool m_isRunning = true;

    void initialize();
    void update();
    void draw();
};
