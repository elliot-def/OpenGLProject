#include "SteamManager.h"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
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
        printf("[SteamManager] ERREUR SteamAPI_InitEx: %d - %s\n", result, errMsg);
        switch (result) {
            case k_ESteamAPIInitResult_NoSteamClient:
                printf("[SteamManager]   -> Steam n'est pas lancé.\n");
                break;
            case k_ESteamAPIInitResult_VersionMismatch:
                printf("[SteamManager]   -> Version du client Steam obsolète.\n");
                break;
            default:
                printf("[SteamManager]   -> Erreur générique.\n");
                break;
        }
        return false;
    }

    m_localSteamID = SteamUser()->GetSteamID();
    m_initialized  = true;

    // Enregistrer les callbacks (seulement après SteamAPI_Init réussi)
    m_cbOverlay.Register(this, &SteamManager::onGameOverlayActivated);
    m_cbLobbyJoin.Register(this, &SteamManager::onGameLobbyJoinRequested);
    m_cbEnter.Register(this, &SteamManager::onLobbyEnter);
    m_cbChatUpdate.Register(this, &SteamManager::onLobbyChatUpdate);

    printf("[SteamManager] Initialisé avec succès.\n");
    printf("[SteamManager]   AppID       : %u\n", SteamUtils()->GetAppID());
    printf("[SteamManager]   SteamID     : %llu\n", m_localSteamID.ConvertToUint64());
    printf("[SteamManager]   PersonaName : %s\n", SteamFriends()->GetPersonaName());

    m_appID = SteamUtils()->GetAppID();

    return true;
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
    printf("[SteamManager] Arrêté.\n");
}

// ---------------------------------------------------------------------------
// Boucle de callbacks
// ---------------------------------------------------------------------------

void SteamManager::runCallbacks() {
    if (!m_initialized) return;
    SteamAPI_RunCallbacks();
}

// ---------------------------------------------------------------------------
// Ligne de commande (invitation reçue jeu fermé)
// ---------------------------------------------------------------------------

void SteamManager::parseCommandLine(int argc, char* argv[]) {
    if (!m_initialized) return;

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "+connect_lobby") == 0 && i + 1 < argc) {
            uint64 lobbyID = strtoull(argv[i + 1], nullptr, 10);
            CSteamID steamIDLobby(lobbyID);

            if (steamIDLobby.IsValid() && steamIDLobby.IsLobby()) {
                printf("[SteamManager] Invitation lobby détectée en ligne de commande : %llu\n", lobbyID);
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
        printf("[SteamManager] ERREUR: Steam non initialisé, impossible de créer un lobby.\n");
        return;
    }
    printf("[SteamManager] Création d'un lobby (type=%d, max=%d)...\n", eType, maxMembers);
    SteamAPICall_t hCall = SteamMatchmaking()->CreateLobby(eType, maxMembers);
    m_callResultCreated.Set(hCall, this, &SteamManager::onLobbyCreated);
}

void SteamManager::joinLobby(CSteamID steamIDLobby) {
    if (!m_initialized) {
        printf("[SteamManager] ERREUR: Steam non initialisé, impossible de rejoindre un lobby.\n");
        return;
    }
    printf("[SteamManager] Rejoindre le lobby %llu...\n", steamIDLobby.ConvertToUint64());
    SteamMatchmaking()->JoinLobby(steamIDLobby);
}

void SteamManager::leaveLobby() {
    if (!m_initialized || !m_inLobby) return;
    printf("[SteamManager] Quitter le lobby %llu...\n", m_currentLobby.ConvertToUint64());
    SteamMatchmaking()->LeaveLobby(m_currentLobby);
    m_currentLobby = CSteamID();
    m_inLobby = false;
}

void SteamManager::openInviteDialog() {
    if (!m_initialized || !m_inLobby) {
        printf("[SteamManager] ERREUR: Pas dans un lobby, impossible d'ouvrir l'invitation.\n");
        return;
    }
    printf("[SteamManager] Ouverture de l'overlay d'invitation Steam...\n");
    SteamFriends()->ActivateGameOverlayInviteDialog(m_currentLobby);
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
        printf("[SteamManager] Overlay Steam activé.\n");
    } else {
        printf("[SteamManager] Overlay Steam désactivé.\n");
    }
}

// ---------------------------------------------------------------------------
// Callbacks Steam - Lobby Join Request (invitation reçue EN JEU)
// ---------------------------------------------------------------------------

void SteamManager::onGameLobbyJoinRequested(GameLobbyJoinRequested_t* pCallback) {
    printf("[SteamManager] Invitation lobby reçue en jeu : %llu\n",
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
        printf("[SteamManager] ERREUR création du lobby (result=%d, IOFailure=%d)\n",
               pCallback->m_eResult, bIOFailure);
        return;
    }

    m_currentLobby = CSteamID(pCallback->m_ulSteamIDLobby);
    m_inLobby      = true;

    printf("[SteamManager] Lobby créé : %llu\n", m_currentLobby.ConvertToUint64());

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
        printf("[SteamManager] ERREUR: échec pour rejoindre le lobby (code=%d)\n",
               pCallback->m_EChatRoomEnterResponse);
        return;
    }

    m_currentLobby = CSteamID(pCallback->m_ulSteamIDLobby);
    m_inLobby      = true;

    printf("[SteamManager] Entré dans le lobby %llu\n", m_currentLobby.ConvertToUint64());

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

    // Le joueur local a quitté le lobby (kicked, disconnected, ou left)
    if (userChanged == m_localSteamID) {
        uint32 flags = pCallback->m_rgfChatMemberStateChange;
        if (flags & (k_EChatMemberStateChangeLeft |
                     k_EChatMemberStateChangeDisconnected |
                     k_EChatMemberStateChangeKicked)) {
            printf("[SteamManager] Joueur local a quitté le lobby.\n");
            m_inLobby = false;
            m_currentLobby = CSteamID();
            if (m_onLobbyLeft) {
                m_onLobbyLeft();
            }
        }
    } else {
        const char* name = SteamFriends()->GetFriendPersonaName(userChanged);
        printf("[SteamManager] Changement dans le lobby pour %s (SteamID %llu)\n",
               name, userChanged.ConvertToUint64());
    }
}
