#pragma once
#ifdef STEAM_OFFLINE
#include <cstdint>
#include <cstdio>
#include <functional>
using CSteamID = uint64_t;
enum ELobbyType : int { k_ELobbyTypePublic = 0, k_ELobbyTypeFriendsOnly = 1, k_ELobbyTypePrivate = 2, k_ELobbyTypeInvisible = 3 };
class SteamManager {
public:
    using InviteCallback = std::function<void(CSteamID)>;
    using LobbyCallback  = std::function<void(CSteamID)>;
    using VoidCallback   = std::function<void()>;
    bool init() { printf("[Steam] Mode hors-ligne (STEAM_OFFLINE)\n"); return false; }
    bool isInitialized() const { return false; }
    void shutdown() {}
    void runCallbacks() {}
    void parseCommandLine(int, char**) {}
    void createLobby(ELobbyType, int) {}
    void joinLobby(CSteamID) {}
    void leaveLobby() {}
    void openInviteDialog() {}
    const char* getLocalPersonaName() const { return "OfflinePlayer"; }
    void setOnInviteReceived(InviteCallback) {}
    void setOnLobbyCreated(LobbyCallback) {}
    void setOnLobbyEntered(LobbyCallback) {}
    void setOnLobbyLeft(VoidCallback) {}
};
#endif
