#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "MapLoader.h"

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
class Mesh;
class Texture;
class ModelEntity;
class Skybox;
class CharacterAnimationController;
class Door;
class TvScreen;
class Creature;

// ---------------------------------------------------------------------------
// Scene : monde 3D du jeu (carte, lumières, entités riggées, skybox).
//
// Scene POSSÈDE la carte (via MapLoader : sol, murs, toit, portes) et le
// skybox, et reçoit des vues non-propriétaires vers les services de Game
// (caméra, joueur, collision, lumières, renderer, shaders, textures, input)
// ainsi que les entités 3D chargées par ModelLoader (qui les possède).
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
    void loadLights();                   // lumières ponctuelles (point lights)
    void loadMap(const char* jsonPath);  // carte JSON (sol, murs, toit, portes)

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
    Shader* m_doorShader    = nullptr;  // "door" : matière unie (surlignage via contour)
    Shader* m_outlineShader = nullptr;  // "outline" : contour des portes interactives
    Shader* m_maskShader    = nullptr;  // "image/mask" : écrans TV (easter egg DVD)

    // Rendu instancié des cubes de lumière (cube unitaire + lots par shader).
    std::unique_ptr<CubeRenderer> m_cubeRenderer;
    std::unique_ptr<Skybox> m_skybox;

    // ── Carte : sol, murs, toit, portes (chargée depuis un JSON) ──────────
    // Le MapLoader possède les meshs et les portes ; la Scene n'en garde que
    // des vues non-propriétaires (même convention que ModelLoader).
    std::unique_ptr<MapLoader> m_map;

    // Colliders dynamiques des portes : matrices de la frame précédente
    // (dirty-flag, même principe que backpack/fropy plus bas).
    std::vector<glm::mat4> m_lastDoorMatrices;

    // ── Écrans TV ("tvs" dans la carte) ───────────────────────────────
    // Placements créés par le MapLoader ; la Scene possède les TvScreen
    // (easter egg DVD animé, visible/animé seulement à proximité).
    std::vector<std::unique_ptr<TvScreen>> m_tvScreens;

    // ── Matériaux de la carte ────────────────────────────────────────────
    // "brickwall" (murs + pignons du toit) : textures brickwall (.jpg chargées
    // directement, le TextureManager ne parcourt que les .png). Shader dédié
    // "cube/wall" : TBN reconstruit par dérivées dans le fragment shader
    // (pas de tangentes dans Vertex), mêmes uniforms de lumière que
    // severallights (LightManager::applyToShader).
    Shader*                  m_wallShader   = nullptr;
    std::unique_ptr<Texture> m_wallDiffuse;   // brickwall.jpg
    std::unique_ptr<Texture> m_wallNormal;    // brickwall_normal.jpg
    // "container" / "parquet" / "leaf" / "grey_roof" / "riet" (caisses,
    // sol parquet, terre extérieure, toit, tapis d'entrée) : shader
    // "cube/severallights" en mode mesh unique (useInstanceModel=0) +
    // textures du TextureManager, UV en mètres comme le reste de la carte.
    Shader*  m_severallightsShader = nullptr;
    Texture* m_containerTexture = nullptr;
    Texture* m_parquetTexture   = nullptr;
    Texture* m_leafTexture      = nullptr;
    Texture* m_greyRoofTexture  = nullptr;
    Texture* m_rietTexture      = nullptr;

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

    // ── Soleil & créature ────────────────────────────────────────────────
    // Soleil visible : tête de fropy JAUNE (shader "door" = couleur unie),
    // placée à l'opposé du rayonnement du dir light (ambiance de la map).
    std::unique_ptr<ModelEntity> m_sunHead;
    // Créature qui poursuit le joueur : corps sombre en cubes (instancié via
    // CubeRenderer + shader "cube/lightsource") + tête de fropy (Creature.h).
    std::unique_ptr<ModelEntity> m_creatureHead;
    std::unique_ptr<Creature>    m_creature;
    Shader* m_creatureBodyShader = nullptr;   // "cube/lightsource" (émissif)

    // E (touche Grab / RB manette) : ouvre/ferme la porte visee la plus proche.
    void tryToggleDoor();

    // Etat no-clip de la frame precedente : sert a detecter la TRANSITION
    // (entree/sortie) pour forcer l'idle une seule fois a l'entree et
    // reinitialiser la machine a animations a la sortie.
    bool m_wasNoClip = false;
};
