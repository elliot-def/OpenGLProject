#pragma once

#include <cstdint>
#include <map>
#include <memory>

#include <glm/glm.hpp>

// Déclarations anticipées
class SteamManager;
class Camera;
class LightManager;
class Renderer;
class TextureManager;
class ShaderManager;
class Shader;
class Player;
class ModelEntity;

// ---------------------------------------------------------------------------
// MultiplayerManager
//
// Synchronise la position, la direction et l'animation des autres joueurs du
// lobby Steam via ISteamNetworkingMessages (P2P fiable). Chaque client diffuse
// son état ~10×/s ; à la réception, un modèle Megan est créé (ou mis à jour)
// pour chaque pair distant et rendu dans la scène.
//
// Le manager possède les ModelEntity des joueurs distants et les détruit.
// ---------------------------------------------------------------------------
class MultiplayerManager {
public:
    MultiplayerManager(SteamManager* steam, Camera* camera, LightManager* lightManager,
                       Renderer* renderer, TextureManager* textureManager,
                       ShaderManager* shaderManager, Player* player, ModelEntity* localHuman);
    ~MultiplayerManager();

    // À appeler chaque frame en STATE_PLAYING : diffuse l'état local, reçoit
    // les états distants et fait avancer les animations des pairs.
    void update(float dt);

    // Dessine tous les joueurs distants (à appeler après m_scene->draw()).
    void draw();

    // Appelé quand le lobby est créé/rejoint : repart d'une liste vide (les
    // joueurs distants seront recréés paresseusement à la réception de leur
    // premier paquet).
    void onLobbyChanged();

    // Appelé quand le lobby est quitté : libère tous les joueurs distants.
    void onLobbyLeft();

    // Définit le modèle local (personnage Megan) dont on lit l'animation
    // courante. Appelé par Game::adoptLoadedEntities() une fois le chargement
    // des modèles terminé (le pointeur n'est pas connu à la construction).
    void setLocalHuman(ModelEntity* localHuman) { m_localHuman = localHuman; }

private:
    struct RemotePlayer {
        uint64_t steamID = 0;
        std::unique_ptr<ModelEntity> entity;
        glm::vec3 targetPos{0.0f};
        float targetYawDeg = 0.0f;   // cap (degrés) reçu
        int   animIndex    = -1;     // animation à jouer
        int   lastAnimIndex = -1;    // dernière animation appliquée
        float timeSincePacket = 0.0f;
    };

    // Crée paresseusement le modèle d'un pair distant (Megan configuré via
    // ModelLoader::configureHumanCharacter).
    void ensureRemotePlayer(uint64_t steamID);

    // Sérialise et diffuse l'état local à tous les membres du lobby.
    void broadcastLocalState();

    // Lit les paquets P2P reçus et met à jour les joueurs distants.
    void processIncomingMessages();

    // Applique position + cap + animation au modèle d'un joueur distant.
    void applyState(RemotePlayer& rp);

    SteamManager*   m_steam;
    Camera*         m_camera;
    LightManager*   m_lightManager;
    Renderer*       m_renderer;
    TextureManager* m_textureManager;
    ShaderManager*  m_shaderManager;
    Player*         m_player;
    ModelEntity*    m_localHuman;
    Shader*         m_skinnedShader = nullptr;

    std::map<uint64_t, std::unique_ptr<RemotePlayer>> m_remotePlayers;
    float m_sendTimer = 0.0f;

    static constexpr float SEND_INTERVAL  = 0.1f;  // 10 Hz
    static constexpr float STALE_TIMEOUT  = 5.0f;  // déconnexion présumée
};
