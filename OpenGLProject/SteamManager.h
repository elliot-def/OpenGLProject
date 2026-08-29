#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>

// Steamworks SDK
#include "dependencies/steam/steam_api.h"

// ---------------------------------------------------------------------------
// SteamManager
//
// Gère l'initialisation du SDK Steamworks, les callbacks asynchrones, et le
// système de lobbies pour les invitations multijoueur (Option A : ISteamMatchmaking).
//
// Double flux de réception des invitations :
//   1. En jeu  → Callback GameLobbyJoinRequested_t
//   2. Fermé   → Argument +connect_lobby dans la ligne de commande
//
// Cycle de vie :
//   init()          → SteamAPI_Init (bloquant)
//   initAsyncStart()→ Lance l'init, lance Steam si nécessaire, non-bloquant
//   initAsyncPoll() → À appeler chaque frame jusqu'à READY ou FAILED
//   chaque frame    → runCallbacks()
//   fin             → shutdown() → SteamAPI_Shutdown
// ---------------------------------------------------------------------------

// État de l'initialisation asynchrone
enum class SteamInitStatus {
    NOT_STARTED,
    WAITING,   // Steam lancé, en attente de connexion
    READY,     // Steam initialisé avec succès
    FAILED     // Échec (timeout ou autre erreur)
};

class SteamManager {
public:
    SteamManager() = default;
    ~SteamManager();

    // ---- Cycle de vie ----

    // Initialise le client Steam. Retourne true si succès.
    // Doit être appelé une seule fois au démarrage.
    bool init();

    // Init non-bloquante : essaie une première fois, lance Steam si absent,
    // puis retourne immédiatement. Appeler initAsyncPoll() chaque frame ensuite.
    SteamInitStatus initAsyncStart();

    // À appeler chaque frame après initAsyncStart().
    // Retourne WAITING tant que Steam n'est pas prêt, READY quand OK, FAILED si échec.
    SteamInitStatus initAsyncPoll(float dt);

    // Vrai si on est en train d'attendre Steam après l'avoir lancé
    bool isWaitingForSteam() const { return m_initStatus == SteamInitStatus::WAITING; }

    // Traite les événements Steam en attente (invitations, overlay, etc.)
    // À appeler chaque frame dans la boucle de rendu.
    void runCallbacks();

    // Libère proprement le SDK Steam.
    void shutdown();

    // Analyse les arguments de lancement pour détecter une invitation
    // (ex : +connect_lobby <SteamID_Lobby>). À appeler après init().
    void parseCommandLine(int argc, char* argv[]);

    // ---- Lobbies ----

    // Crée un lobby Steam. Résultat asynchrone via callback onLobbyCreated.
    void createLobby(ELobbyType eType = k_ELobbyTypeFriendsOnly, int maxMembers = 4);

    // Rejoint un lobby existant.
    void joinLobby(CSteamID steamIDLobby);

    // Quitte le lobby courant.
    void leaveLobby();

    // Ouvre l'overlay Steam pour inviter un ami au lobby courant.
    void openInviteDialog();

    // ── Réseau P2P (Steam Networking Messages) ────────────────────────────
    // Synchronisation de la position/direction/animation des joueurs du lobby
    // via ISteamNetworkingMessages (UDP-like, fiable, routé par Steam).

    // Message P2P reçu : expéditeur + charge utile binaire.
    struct P2PMessage {
        CSteamID sender;
        std::vector<uint8_t> data;
    };

    // Envoie un paquet binaire FIABLE à un pair identifié par son SteamID.
    // Retourne false si Steam n'est pas prêt ou si l'envoi échoue.
    bool sendP2P(CSteamID target, const void* data, uint32_t size);

    // Diffuse un paquet à tous les membres du lobby courant (sauf le local).
    bool broadcastP2P(const void* data, uint32_t size);

    // Variante NON-FIABLE (chat vocal temps reel) : les paquets peuvent se
    // perdre/arriver en desordre, pas de retransmission.
    bool sendP2PUnreliable(CSteamID target, const void* data, uint32_t size);
    bool broadcastP2PUnreliable(const void* data, uint32_t size);

    // Lit les messages P2P en attente sur le canal 0. Retourne le nombre lu
    // (les sessions entrantes sont acceptées automatiquement via callback).
    int receiveP2P(std::vector<P2PMessage>& out, int maxMessages = 32);

    // Membres du lobby courant, joueur local exclu. Vide si hors lobby.
    std::vector<CSteamID> getLobbyMembers() const;

    // État du lobby
    bool isInLobby() const { return m_inLobby; }
    CSteamID getCurrentLobbyID() const { return m_currentLobby; }
    CSteamID getLocalSteamID() const { return m_localSteamID; }
    const char* getLocalPersonaName() const;
    bool isInitialized() const { return m_initialized; }

    // AppID utilisé (lu depuis steam_appid.txt ou AppID de test 480)
    AppId_t getAppID() const { return m_appID; }

    // ---- Callbacks utilisateur ----
    // Le jeu peut s'enregistrer pour réagir aux événements Steam.

    using LobbyCallback = std::function<void(CSteamID lobbyID)>;
    using LobbyMemberCallback = std::function<void(CSteamID lobbyID, CSteamID memberID)>;
    // Changement d'un membre du lobby (autre que le local) : flags bruts
    // LobbyChatUpdate_t (k_EChatMemberStateChangeEntered/Left/Disconnected/Kicked).
    using LobbyMemberUpdateCallback = std::function<void(CSteamID lobbyID, CSteamID memberID, uint32_t flags)>;
    using SimpleCallback     = std::function<void()>;

    void setOnLobbyCreated(LobbyCallback cb)   { m_onLobbyCreated = std::move(cb); }
    void setOnLobbyEntered(LobbyCallback cb)   { m_onLobbyEntered = std::move(cb); }
    void setOnLobbyLeft(SimpleCallback cb)      { m_onLobbyLeft = std::move(cb); }

    // Appelé quand une invitation entrante doit être rejointe
    // (soit en jeu via callback, soit via ligne de commande).
    void setOnInviteReceived(LobbyCallback cb)  { m_onInviteReceived = std::move(cb); }

    // Appelé quand un membre du lobby (autre que le local) entre/sort du
    // lobby (flags LobbyChatUpdate_t). Utilise par le chat pour les logs
    // systeme "X a rejoint/quitte le lobby".
    void setOnLobbyMemberUpdate(LobbyMemberUpdateCallback cb) { m_onLobbyMemberUpdate = std::move(cb); }

    // Nom public d'un utilisateur Steam (persona name). Retourne une chaine
    // vide si Steam n'est pas initialise.
    const char* getPersonaName(CSteamID steamID) const;

private:
    bool               m_initialized  = false;
    bool               m_inLobby      = false;
    AppId_t            m_appID        = 480;   // Spacewar (test)
    CSteamID           m_currentLobby;
    CSteamID           m_localSteamID;

    // ---- Init asynchrone ----
    SteamInitStatus    m_initStatus   = SteamInitStatus::NOT_STARTED;
    float              m_launchTimer  = 0.0f;
    int                m_launchAttempt = 0;
    static constexpr int    MAX_LAUNCH_ATTEMPTS = 10;
    static constexpr float  LAUNCH_RETRY_INTERVAL = 1.0f;  // secondes

    // ---- Callbacks Steam (manuels : enregistrés après init) ----
    STEAM_CALLBACK_MANUAL(SteamManager, onGameOverlayActivated,   GameOverlayActivated_t,   m_cbOverlay);
    STEAM_CALLBACK_MANUAL(SteamManager, onGameLobbyJoinRequested, GameLobbyJoinRequested_t, m_cbLobbyJoin);
    STEAM_CALLBACK_MANUAL(SteamManager, onLobbyEnter,             LobbyEnter_t,             m_cbEnter);
    STEAM_CALLBACK_MANUAL(SteamManager, onLobbyChatUpdate,        LobbyChatUpdate_t,        m_cbChatUpdate);
    STEAM_CALLBACK_MANUAL(SteamManager, onNetworkingSessionRequest, SteamNetworkingMessagesSessionRequest_t, m_cbSessionRequest);

    // ---- CallResult pour CreateLobby / JoinLobby ----
    CCallResult<SteamManager, LobbyCreated_t> m_callResultCreated;
    void onLobbyCreated(LobbyCreated_t* pCallback, bool bIOFailure);

    // ---- Callbacks utilisateur ----
    LobbyCallback       m_onLobbyCreated;
    LobbyCallback       m_onLobbyEntered;
    SimpleCallback      m_onLobbyLeft;
    LobbyCallback       m_onInviteReceived;
    LobbyMemberUpdateCallback m_onLobbyMemberUpdate;

    // ---- Détail interne ----
    void completeInit();   // finalise l'init après SteamAPI_InitEx OK
    void setLobbyMetadata(CSteamID lobbyID);
};
