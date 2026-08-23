#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>

// Déclarations anticipées (services détenus par Game)
class Camera;
class Player;
class CollisionManager;
class LightManager;
class Renderer;
class ShaderManager;
class Shader;
class TextureManager;
class InputManager;
class Cube;
class CubeRenderer;
class ModelEntity;
class Skybox;
class CharacterAnimationController;

// ---------------------------------------------------------------------------
// Scene : monde 3D du jeu (cubes, lumières, entités riggées, skybox).
//
// Scene POSSÈDE les objets du monde (cubes, transparences, skybox) et reçoit
// des vues non-propriétaires vers les services de Game (caméra, joueur,
// collision, lumières, renderer, shaders, textures, input) ainsi que les
// entités 3D chargées par ModelLoader (qui les possède).
//
// Objectif : extraire de Game.cpp toute la logique du monde (update/draw)
// pour que Game ne garde que l'orchestration (menus, son, réseau, loading).
// ---------------------------------------------------------------------------
class Scene {
public:
    Scene(Camera* camera, Player* player, CollisionManager* collisionManager,
          LightManager* lightManager, Renderer* renderer, ShaderManager* shaderManager,
          TextureManager* textureManager, InputManager* inputManager);
    ~Scene();

    // ── Construction du monde (appelé pendant loadResources) ─────────────
    // Séparés pour conserver la progression visuelle du loading screen.
    void loadLights();   // lumières ponctuelles (point lights)
    void loadCubes();    // cubes + collisions statiques + BVH

    // Skybox (contexte GL principal — à appeler après le thread de chargement).
    void createSkybox(const char* path);

    // Adopte les entités 3D chargées par ModelLoader (vues non-propriétaires).
    void adoptEntities(ModelEntity* modelEntity, ModelEntity* fropyEntity,
                       ModelEntity* humanEntity, CharacterAnimationController* characterAnim);

    // ── Boucle du monde (STATE_PLAYING) ───────────────────────────────────
    void update(float deltaTime);
    void draw();

    // ── Accesseurs pour Game (HUD, reload GPU post-loading) ──────────────
    ModelEntity* getModelEntity() const { return m_modelEntity; }
    ModelEntity* getFropyEntity() const { return m_fropyEntity; }
    ModelEntity* getHumanEntity() const { return m_humanEntity; }

private:
    // Services (vues non-propriétaires, possédés par Game)
    Camera*            m_camera;
    Player*            m_player;
    CollisionManager*  m_collisionManager;
    LightManager*      m_lightManager;
    Renderer*          m_renderer;
    ShaderManager*     m_shaderManager;
    TextureManager*    m_textureManager;
    InputManager*      m_inputManager;

    // Shaders résolus UNE SEULE FOIS (plus de getShader() par frame)
    Shader* m_skyboxShader  = nullptr;
    Shader* m_modelShader   = nullptr;
    Shader* m_skinnedShader = nullptr;

    // Monde possédé par la scène
    std::vector<std::unique_ptr<Cube>> m_cubes;   // descripteurs d'instances (centre/edge/shader)
    std::unique_ptr<CubeRenderer> m_cubeRenderer; // rendu instancié : cube unitaire + lots par shader
    std::unique_ptr<Skybox> m_skybox;

    // Entités 3D (vues non-propriétaires, possédées par ModelLoader)
    ModelEntity*                m_modelEntity = nullptr;
    ModelEntity*                m_fropyEntity = nullptr;
    ModelEntity*                m_humanEntity = nullptr;
    CharacterAnimationController* m_characterAnim = nullptr;

    // ── Dirty-flags collisions dynamiques ─────────────────────────────────
    // L'AABB world n'est recalculée que si la matrice modèle de l'entité a
    // changé depuis la dernière frame (gros gain pour des entités statiques :
    // évite de retransformer les 8 coins × tous les sous-meshes chaque frame).
    glm::mat4 m_lastBackpackMatrix{ 1.0f };
    bool      m_backpackDirty = true;
    glm::mat4 m_lastFropyMatrix{ 1.0f };
    bool      m_fropyDirty = true;

    // Etat no-clip de la frame precedente : sert a detecter la TRANSITION
    // (entree/sortie) pour forcer l'idle une seule fois a l'entree et
    // reinitialiser la machine a animations a la sortie.
    bool m_wasNoClip = false;
};
