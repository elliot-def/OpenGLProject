#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

#include "constants/network.h"

class Packet; // Déclaration anticipée

struct ServerInfo {
    std::string ip = Constants::Network::SERVER_IP;
    int port = Constants::Network::SERVER_PORT;
};

enum class EventType {
    ServerConnected,
    ServerDisconnected,
    DataReceived
};

struct ClientEvent {
    EventType type;
    std::string data;
};

struct OutgoingMessage {
    std::string data;
};

class Socket {
public:
    Socket() = default;
    ~Socket() { stop(); }

	// Se connecte au serveur selon les ServerInfo fournis (Asynchrone)
    void connectToServerAsync(const ServerInfo& serverInfo);

    // Se connecte au serveur selon les ServerInfo fournis
    bool connectToServer(const ServerInfo& serverInfo);

    // Arrête le client et ferme toutes les connexions
    void stop();

    // Envoie un paquet au server
    void sendPacket(const std::string& data);
    // Envoie un paquet au serveur
    void sendPacket(const Packet& packet);

    // Récupère le prochain événement (connexion, déconnexion, données)
    bool pollEvent(ClientEvent& event);

    // Retourne la liste des clients connectés
    ServerInfo getServer() const { return m_server; };
	std::string getLocalIP() const { return m_localIP; }
	int getLocalPort() const { return m_localPort; }

private:
    void networkLoop();

    SOCKET m_socket = INVALID_SOCKET;

    ServerInfo m_server;

    std::queue<ClientEvent> m_incoming;
    std::mutex m_inMutex;

    std::queue<OutgoingMessage> m_outgoing;
    std::mutex m_outMutex;

    std::thread m_netThread;
    std::atomic<bool> m_running{ false };

	std::string m_localIP = "";
	int m_localPort = 0;
};
