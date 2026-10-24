#pragma once

#include <functional>
#include <map>
#include <string>

#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "MapLoader.h"
#include "Creature.h"

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
    void loadMap(const char* jsonPath);
    // Emet un bruit (position monde + portee en m) entendu par fropy cette frame :
    // voix des joueurs, plus tard quetes, objets... A appeler avant update().
    void addNoise(const glm::vec3& position, float radius) { m_noises.push_back({ position, radius }); }  // carte JSON (sol, murs, toit, portes)

    // Skybox (contexte GL principal — à appeler après le thread de chargement).
    void createSkybox(const char* path);

    // Adopte les entités 3D chargées par ModelLoader (vues non-propriétaires).
    void adoptEntities(ModelEntity* humanEntity, CharacterAnimationController* characterAnim);

    // ── Boucle du monde (STATE_PLAYING) ───────────────────────────────────
    void update(float deltaTime);
    void draw();

    // ── Accesseurs pour Game (HUD, reload GPU post-loading) ──────────────
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
    std::unique_ptr<class FogParticles> m_fogParticles;      // brume en particules (nuit)
    float m_creatureSight = 20.0f;                            // portee de vue de la creature (m)
    Shader* m_leavesShader = nullptr;                       // "leaves" : feuilles au sol (alpha)
    std::unique_ptr<Texture> m_leafTextures[3];             // leaf_sheet_<n>.png
    std::map<std::string, std::unique_ptr<Texture>> m_modelTextures;   // textures des modeles OBJ (cache)
    Texture* getModelTexture(const std::string& path);
    std::unique_ptr<Texture> m_treeTexture;   // tree_palette.png (tronc + feuillage)
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
    ModelEntity*                m_humanEntity = nullptr;
    CharacterAnimationController* m_characterAnim = nullptr;

    // ── Dirty-flags collisions dynamiques ─────────────────────────────────
    // L'AABB world n'est recalculée que si la matrice modèle de l'entité a
    // changé depuis la dernière frame (gros gain pour des entités statiques :
    // évite de retransformer les 8 coins × tous les sous-meshes chaque frame).

    // ── Ciel (tete du soleil/lune + eblouissement) & creature ─────────────
    std::unique_ptr<class SkySystem> m_sky;
    // Créature qui poursuit le joueur : corps sombre en cubes (instancié via
    // CubeRenderer + shader "cube/lightsource") + tête de fropy (Creature.h).
    std::unique_ptr<ModelEntity> m_creatureModel;   // fropy articule (charge une fois, garde entre les cartes)
    std::unique_ptr<Creature>    m_creature;
    int m_creatureAnim = -1;                        // clip en cours (repos / marche / course)
    void ensureCreatureModel();
    void updateCreatureAnimation(float deltaTime);
    void openDoorsForCreature();   // fropy pousse les portes qui barrent sa trajectoire

    // E (touche Grab / RB manette) : ouvre/ferme la porte visee la plus proche.
    void tryToggleDoor();
public:
    // Options de partie appliquees au prochain loadMap (fropy actif, difficulte).
    void setMatchParams(bool creatureEnabled, float speedMul, float senseMul) {
        m_matchCreature = creatureEnabled; m_matchSpeedMul = speedMul; m_matchSenseMul = senseMul;
    }
    // Bouton de carte actionne (E) : nom du bouton (cf. MapLoader "buttons").
    void setButtonCallback(std::function<void(const std::string&)> cb) { m_buttonCallback = std::move(cb); }
    // Fropy touche le joueur (hors noclip) : la partie decide des vies.
    void setPlayerHitCallback(std::function<void()> cb) { m_playerHitCallback = std::move(cb); }
    // Libelle du bouton vise ("" si aucun) : invite "[E] ..." du HUD.
    std::string getAimedButtonLabel() const;
    // Fropy de la partie (null dans le lobby / en mode Exploration).
    const Creature* getCreature() const { return m_creature.get(); }
    // Boutons de la carte (postes de quete compris). Un bouton desactive n'est ni
    // dessine ni visable (poste de quete non tire ou deja fait).
    const std::vector<MapButton>& getMapButtons() const;
    void setButtonEnabled(const std::string& name, bool enabled);
private:
    bool  m_matchCreature = true;
    float m_matchSpeedMul = 1.0f;
    float m_matchSenseMul = 1.0f;
    int   m_aimedButton = -1;
    std::unique_ptr<ModelEntity> m_buttonModel;   // fropy tournant partage : dessine a chaque bouton de la carte
    // Rendu des parties de carte : indices par materiau (tries au chargement) et
    // sphere englobante (xyz centre, w rayon) pour le frustum culling.
    std::vector<std::vector<size_t>> m_partsByMaterial;
    std::vector<glm::vec4> m_partSpheres;
    glm::vec4 m_frustum[6];
    void buildPartIndex();
    bool partVisible(size_t partIndex) const;
    float m_hitCooldown = 0.0f;                                 // invulnerabilite apres un coup de fropy
    std::function<void()> m_playerHitCallback;
    std::function<void(const std::string&)> m_buttonCallback;
public:
    // Positions des joueurs distants, mises a jour chaque frame par Game.
    void setRemoteTargets(std::vector<glm::vec3> targets) { m_remoteTargets = std::move(targets); }
private:
    std::vector<NoiseEvent> m_noises;         // bruits emis depuis la derniere update
    std::vector<glm::vec3> m_remoteTargets;   // joueurs distants (multijoueur)

    // Etat no-clip de la frame precedente : sert a detecter la TRANSITION
    // (entree/sortie) pour forcer l'idle une seule fois a l'entree et
    // reinitialiser la machine a animations a la sortie.
    bool m_wasNoClip = false;
};
