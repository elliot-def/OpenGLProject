#include "SteamManager.h"
#include "Log.h"
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SteamManager::~SteamManager() {
    shutdown();
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

bool SteamManager::init() {
    if (m_initialized) return true;

    // Lire l'AppID depuis steam_appid.txt (optionnel, mais bon pour le dev local)
    // Ce fichier est déjà présent à côté de l'exécutable (AppID 480 = Spacewar)
    // SteamAPI_Init le lit automatiquement, on ne fait rien de spécial.

    SteamErrMsg errMsg;
    ESteamAPIInitResult result = SteamAPI_InitEx(&errMsg);

    if (result != k_ESteamAPIInitResult_OK) {
        if (result == k_ESteamAPIInitResult_NoSteamClient) {
            logPrintf("[SteamManager] Steam n'est pas lance, tentative de lancement...\n");
#ifdef _WIN32
            // Lancer Steam via le protocole steam://
            ShellExecuteA(NULL, "open", "steam://open/main", NULL, NULL, SW_HIDE);
            logPrintf("[SteamManager]   -> Steam lance, attente du demarrage (3s)...\n");

            // Attendre que le client Steam démarre avant la 1ère tentative.
            // Sans ce délai, SteamAPI_InitEx échoue avec FailedGeneric
            // (ConnectToGlobalUser failed) car le client n'a pas eu le temps
            // de se connecter au compte utilisateur.
            Sleep(3000);

            // Réessayer pendant max 20 secondes.
            // On continue sur NoSteamClient (Steam pas encore lancé) ET sur
            // FailedGeneric (Steam lancé mais pas encore loggé).
            for (int attempt = 0; attempt < 20; ++attempt) {
                SteamAPI_Shutdown();  // nettoyer l'état entre les tentatives
                result = SteamAPI_InitEx(&errMsg);
                if (result == k_ESteamAPIInitResult_OK) goto steam_ready;
                if (result != k_ESteamAPIInitResult_NoSteamClient &&
                    result != k_ESteamAPIInitResult_FailedGeneric) break;
                logPrintf("[SteamManager]   -> Tentative %d/20 (%d - %s)...\n", attempt + 1, result, errMsg);
                Sleep(1000);
            }
            logPrintf("[SteamManager] ERREUR: Steam n'a pas pu etre initialise apres lancement (%d - %s).\n", result, errMsg);
#else
            logPrintf("[SteamManager]   -> Steam n'est pas lance (lancement automatique non supporte sur cette plateforme).\n");
#endif
            return false;
        }

        logPrintf("[SteamManager] ERREUR SteamAPI_InitEx: %d - %s\n", result, errMsg);
        switch (result) {
            case k_ESteamAPIInitResult_VersionMismatch:
                logPrintf("[SteamManager]   -> Version du client Steam obsolete.\n");
                break;
            default:
                logPrintf("[SteamManager]   -> Erreur generique.\n");
                break;
        }
        return false;
    }

steam_ready:
    completeInit();
    return true;
}

// ---------------------------------------------------------------------------
// Initialisation asynchrone (non-bloquante)
// ---------------------------------------------------------------------------

SteamInitStatus SteamManager::initAsyncStart() {
    if (m_initialized) return SteamInitStatus::READY;
    if (m_initStatus == SteamInitStatus::WAITING) return SteamInitStatus::WAITING;

    SteamErrMsg errMsg;
    ESteamAPIInitResult result = SteamAPI_InitEx(&errMsg);

    if (result == k_ESteamAPIInitResult_OK) {
        // Steam déjà prêt !
        goto async_steam_ready;
    }

    if (result == k_ESteamAPIInitResult_NoSteamClient) {
        logPrintf("[SteamManager] Steam n'est pas lance, lancement asynchrone...\n");
#ifdef _WIN32
        ShellExecuteA(NULL, "open", "steam://open/main", NULL, NULL, SW_HIDE);
        m_initStatus    = SteamInitStatus::WAITING;
        m_launchTimer   = 0.0f;
        m_launchAttempt = 0;
        logPrintf("[SteamManager]   -> Steam lance, attente non-bloquante...\n");
        return SteamInitStatus::WAITING;
#else
        logPrintf("[SteamManager]   -> Steam n'est pas lance (lancement auto non supporte).\n");
        m_initStatus = SteamInitStatus::FAILED;
        return SteamInitStatus::FAILED;
#endif
    }

    // Autre erreur
    logPrintf("[SteamManager] ERREUR SteamAPI_InitEx: %d - %s\n", result, errMsg);
    m_initStatus = SteamInitStatus::FAILED;
    return SteamInitStatus::FAILED;

async_steam_ready:
    completeInit();
    m_initStatus = SteamInitStatus::READY;
    return SteamInitStatus::READY;
}

SteamInitStatus SteamManager::initAsyncPoll(float dt) {
    if (m_initStatus != SteamInitStatus::WAITING)
        return m_initStatus;

    m_launchTimer += dt;
    if (m_launchTimer < LAUNCH_RETRY_INTERVAL)
        return SteamInitStatus::WAITING;

    m_launchTimer = 0.0f;
    ++m_launchAttempt;

    SteamErrMsg errMsg;
    SteamAPI_Shutdown();
    ESteamAPIInitResult result = SteamAPI_InitEx(&errMsg);

    if (result == k_ESteamAPIInitResult_OK) {
        logPrintf("[SteamManager] Steam connecte apres %d tentative(s) !\n", m_launchAttempt);
        goto async_steam_ready;
    }

    if (result != k_ESteamAPIInitResult_NoSteamClient) {
        logPrintf("[SteamManager] ERREUR pendant l'attente: %d - %s\n", result, errMsg);
        m_initStatus = SteamInitStatus::FAILED;
        return SteamInitStatus::FAILED;
    }

    if (m_launchAttempt >= MAX_LAUNCH_ATTEMPTS) {
        logPrintf("[SteamManager] Timeout: Steam n'a pas repondu apres %d tentatives.\n", MAX_LAUNCH_ATTEMPTS);
        m_initStatus = SteamInitStatus::FAILED;
        return SteamInitStatus::FAILED;
    }

    logPrintf("[SteamManager]   -> Tentative %d/%d...\n", m_launchAttempt, MAX_LAUNCH_ATTEMPTS);
    return SteamInitStatus::WAITING;

async_steam_ready:
    completeInit();
    m_initStatus = SteamInitStatus::READY;
    return SteamInitStatus::READY;
}

// ---------------------------------------------------------------------------
// Finalisation commune de l'init Steam
// ---------------------------------------------------------------------------

void SteamManager::completeInit() {
    m_localSteamID = SteamUser()->GetSteamID();
    m_initialized  = true;

    m_cbOverlay.Register(this, &SteamManager::onGameOverlayActivated);
    m_cbLobbyJoin.Register(this, &SteamManager::onGameLobbyJoinRequested);
    m_cbEnter.Register(this, &SteamManager::onLobbyEnter);
    m_cbChatUpdate.Register(this, &SteamManager::onLobbyChatUpdate);
    m_cbSessionRequest.Register(this, &SteamManager::onNetworkingSessionRequest);

    logPrintf("[SteamManager] Initialise avec succes.\n");
    logPrintf("[SteamManager]   AppID       : %u\n", SteamUtils()->GetAppID());
    logPrintf("[SteamManager]   SteamID     : %llu\n", m_localSteamID.ConvertToUint64());
    logPrintf("[SteamManager]   PersonaName : %s\n", SteamFriends()->GetPersonaName());

    m_appID = SteamUtils()->GetAppID();
}

// ---------------------------------------------------------------------------
// Arrêt
// ---------------------------------------------------------------------------

void SteamManager::shutdown() {
    if (!m_initialized) return;

    if (m_inLobby) {
        leaveLobby();
    }

    SteamAPI_Shutdown();
    m_initialized = false;
    logPrintf("[SteamManager] Arrete.\n");
}

// ---------------------------------------------------------------------------
// Boucle de callbacks
// ---------------------------------------------------------------------------

void SteamManager::runCallbacks() {
    if (!m_initialized) return;
    SteamAPI_RunCallbacks();
}

// ---------------------------------------------------------------------------
// Ligne de commande (invitation recue jeu fermé)
// ---------------------------------------------------------------------------

void SteamManager::parseCommandLine(int argc, char* argv[]) {
    if (!m_initialized) return;

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "+connect_lobby") == 0 && i + 1 < argc) {
            uint64 lobbyID = strtoull(argv[i + 1], nullptr, 10);
            CSteamID steamIDLobby(lobbyID);

            if (steamIDLobby.IsValid() && steamIDLobby.IsLobby()) {
                logPrintf("[SteamManager] Invitation lobby detectee en ligne de commande : %llu\n", lobbyID);
                if (m_onInviteReceived) {
                    m_onInviteReceived(steamIDLobby);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Gestion des lobbies
// ---------------------------------------------------------------------------

void SteamManager::createLobby(ELobbyType eType, int maxMembers) {
    if (!m_initialized) {
        logPrintf("[SteamManager] ERREUR: Steam non initialise, impossible de creer un lobby.\n");
        return;
    }
    logPrintf("[SteamManager] Creation d'un lobby (type=%d, max=%d)...\n", eType, maxMembers);
    SteamAPICall_t hCall = SteamMatchmaking()->CreateLobby(eType, maxMembers);
    m_callResultCreated.Set(hCall, this, &SteamManager::onLobbyCreated);
}

void SteamManager::joinLobby(CSteamID steamIDLobby) {
    if (!m_initialized) {
        logPrintf("[SteamManager] ERREUR: Steam non initialise, impossible de rejoindre un lobby.\n");
        return;
    }
    logPrintf("[SteamManager] Rejoindre le lobby %llu...\n", steamIDLobby.ConvertToUint64());
    SteamMatchmaking()->JoinLobby(steamIDLobby);
}

void SteamManager::leaveLobby() {
    if (!m_initialized || !m_inLobby) return;
    logPrintf("[SteamManager] Quitter le lobby %llu...\n", m_currentLobby.ConvertToUint64());
    SteamMatchmaking()->LeaveLobby(m_currentLobby);
    m_currentLobby = CSteamID();
    m_inLobby = false;
}

void SteamManager::openInviteDialog() {
    if (!m_initialized || !m_inLobby) {
        logPrintf("[SteamManager] ERREUR: Pas dans un lobby, impossible d'ouvrir l'invitation.\n");
        return;
    }
    logPrintf("[SteamManager] Ouverture de l'overlay d'invitation Steam...\n");
    SteamFriends()->ActivateGameOverlayInviteDialog(m_currentLobby);
}

// ---------------------------------------------------------------------------
// Réseau P2P (Steam Networking Messages)
// ---------------------------------------------------------------------------

bool SteamManager::sendP2P(CSteamID target, const void* data, uint32_t size) {
    if (!m_initialized) return false;
    if (!target.IsValid() || !data || size == 0) return false;

    ISteamNetworkingMessages* net = SteamNetworkingMessages();
    if (!net) return false;

    SteamNetworkingIdentity identity;
    identity.SetSteamID(target);

    EResult result = net->SendMessageToUser(
        identity, data, size, k_nSteamNetworkingSend_Reliable, 0);

    if (result != k_EResultOK) {
        logPrintf("[SteamManager] sendP2P vers %llu echoue (result=%d)\n",
               target.ConvertToUint64(), result);
        return false;
    }
    return true;
}

bool SteamManager::broadcastP2P(const void* data, uint32_t size) {
    if (!m_initialized || !m_inLobby) return false;

    bool anySent = false;
    for (CSteamID member : getLobbyMembers()) {
        if (sendP2P(member, data, size)) anySent = true;
    }
    return anySent;
}

int SteamManager::receiveP2P(std::vector<P2PMessage>& out, int maxMessages) {
    if (!m_initialized || maxMessages <= 0) return 0;

    ISteamNetworkingMessages* net = SteamNetworkingMessages();
    if (!net) return 0;

    // Tampon fixe : on ne dépasse pas maxMessages par frame.
    SteamNetworkingMessage_t* messages[64];
    const int capacity = maxMessages < 64 ? maxMessages : 64;
    const int count = net->ReceiveMessagesOnChannel(0, messages, capacity);

    for (int i = 0; i < count; ++i) {
        SteamNetworkingMessage_t* msg = messages[i];
        if (!msg) continue;

        P2PMessage outMsg;
        outMsg.sender = msg->m_identityPeer.GetSteamID();
        const uint8_t* bytes = static_cast<const uint8_t*>(msg->m_pData);
        outMsg.data.assign(bytes, bytes + msg->m_cbSize);
        out.push_back(std::move(outMsg));

        msg->Release();
    }
    return count;
}

std::vector<CSteamID> SteamManager::getLobbyMembers() const {
    std::vector<CSteamID> members;
    if (!m_initialized || !m_inLobby) return members;

    const int count = SteamMatchmaking()->GetNumLobbyMembers(m_currentLobby);
    members.reserve(static_cast<size_t>(count > 0 ? count : 0));
    for (int i = 0; i < count; ++i) {
        CSteamID member = SteamMatchmaking()->GetLobbyMemberByIndex(m_currentLobby, i);
        if (member.IsValid() && member != m_localSteamID) {
            members.push_back(member);
        }
    }
    return members;
}

// ---------------------------------------------------------------------------
// Callbacks Steam - Demande de session P2P entrante
// ---------------------------------------------------------------------------

void SteamManager::onNetworkingSessionRequest(SteamNetworkingMessagesSessionRequest_t* pCallback) {
    // Les lobbies sont FriendsOnly : on accepte toute session P2P entrante
    // (l'envoi d'un message vers un pair accepte implicitement sa session,
    // mais une requête entrante nécessite cet appel explicite).
    ISteamNetworkingMessages* net = SteamNetworkingMessages();
    if (net) {
        net->AcceptSessionWithUser(pCallback->m_identityRemote);
        logPrintf("[SteamManager] Session P2P acceptee (SteamID %llu)\n",
               pCallback->m_identityRemote.GetSteamID().ConvertToUint64());
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const char* SteamManager::getLocalPersonaName() const {
    if (!m_initialized) return "";
    return SteamFriends()->GetPersonaName();
}

void SteamManager::setLobbyMetadata(CSteamID lobbyID) {
    // Tag unique pour filtrer nos lobbies (évite les collisions avec Spacewar 480)
    SteamMatchmaking()->SetLobbyData(lobbyID, "game", "OpenGLProject");
    SteamMatchmaking()->SetLobbyData(lobbyID, "version", "1.0");
}

// ---------------------------------------------------------------------------
// Callbacks Steam - Overlay
// ---------------------------------------------------------------------------

void SteamManager::onGameOverlayActivated(GameOverlayActivated_t* pCallback) {
    if (pCallback->m_bActive) {
        logPrintf("[SteamManager] Overlay Steam active.\n");
    } else {
        logPrintf("[SteamManager] Overlay Steam desactive.\n");
    }
}

// ---------------------------------------------------------------------------
// Callbacks Steam - Lobby Join Request (invitation recue EN JEU)
// ---------------------------------------------------------------------------

void SteamManager::onGameLobbyJoinRequested(GameLobbyJoinRequested_t* pCallback) {
    logPrintf("[SteamManager] Invitation lobby recue en jeu : %llu\n",
           pCallback->m_steamIDLobby.ConvertToUint64());

    // Si déjà dans un lobby, on le quitte d'abord
    if (m_inLobby) {
        leaveLobby();
    }

    // Propager au jeu
    if (m_onInviteReceived) {
        m_onInviteReceived(pCallback->m_steamIDLobby);
    }
}

// ---------------------------------------------------------------------------
// Callbacks Steam - Lobby Created (réponse à CreateLobby)
// ---------------------------------------------------------------------------

void SteamManager::onLobbyCreated(LobbyCreated_t* pCallback, bool bIOFailure) {
    if (bIOFailure || pCallback->m_eResult != k_EResultOK) {
        logPrintf("[SteamManager] ERREUR creation du lobby (result=%d, IOFailure=%d)\n",
               pCallback->m_eResult, bIOFailure);
        return;
    }

    m_currentLobby = CSteamID(pCallback->m_ulSteamIDLobby);
    m_inLobby      = true;

    logPrintf("[SteamManager] Lobby cree : %llu\n", m_currentLobby.ConvertToUint64());

    // Définir les métadonnées du lobby (filtre anti-collision Spacewar)
    setLobbyMetadata(m_currentLobby);

    // Notifier le jeu
    if (m_onLobbyCreated) {
        m_onLobbyCreated(m_currentLobby);
    }
}

// ---------------------------------------------------------------------------
// Callbacks Steam - Lobby Enter (quand on rejoint un lobby)
// ---------------------------------------------------------------------------

void SteamManager::onLobbyEnter(LobbyEnter_t* pCallback) {
    if (pCallback->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
        logPrintf("[SteamManager] ERREUR: echec pour rejoindre le lobby (code=%d)\n",
               pCallback->m_EChatRoomEnterResponse);
        return;
    }

    m_currentLobby = CSteamID(pCallback->m_ulSteamIDLobby);
    m_inLobby      = true;

    logPrintf("[SteamManager] Entre dans le lobby %llu\n", m_currentLobby.ConvertToUint64());

    if (m_onLobbyEntered) {
        m_onLobbyEntered(m_currentLobby);
    }
}

// ---------------------------------------------------------------------------
// Callbacks Steam - Lobby Chat Update (entrées/sorties de membres)
// ---------------------------------------------------------------------------

void SteamManager::onLobbyChatUpdate(LobbyChatUpdate_t* pCallback) {
    CSteamID userChanged  = pCallback->m_ulSteamIDUserChanged;
    CSteamID userMaking   = pCallback->m_ulSteamIDMakingChange;
    CSteamID lobbyID      = pCallback->m_ulSteamIDLobby;

    // Le joueur local a quitte le lobby (kicked, disconnected, ou left)
    if (userChanged == m_localSteamID) {
        uint32 flags = pCallback->m_rgfChatMemberStateChange;
        if (flags & (k_EChatMemberStateChangeLeft |
                     k_EChatMemberStateChangeDisconnected |
                     k_EChatMemberStateChangeKicked)) {
            logPrintf("[SteamManager] Joueur local a quitte le lobby.\n");
            m_inLobby = false;
            m_currentLobby = CSteamID();
            if (m_onLobbyLeft) {
                m_onLobbyLeft();
            }
        }
    } else {
        const char* name = SteamFriends()->GetFriendPersonaName(userChanged);
        logPrintf("[SteamManager] Changement dans le lobby pour %s (SteamID %llu)\n",
               name, userChanged.ConvertToUint64());
    }
}
