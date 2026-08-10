#include "Socket.h"
#include "Packet.h"

#include "constants/network.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
// ── Windows : Winsock ──────────────────────────────────────────────────────
#define SOCK_LAST_ERROR       WSAGetLastError()
#define SOCK_EWOULDBLOCK      WSAEWOULDBLOCK
#define SOCK_ETIMEDOUT        WSAETIMEDOUT
#define SOCK_ECONNREFUSED     WSAECONNREFUSED
#define SOCK_EHOSTUNREACH     WSAEHOSTUNREACH
static void sock_init()   { WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa); }
static void sock_cleanup(){ WSACleanup(); }
static void sock_nonblock(SOCKET s) { u_long mode = 1; ioctlsocket(s, FIONBIO, &mode); }
static void sock_inet_pton(int af, const char* src, void* dst) { InetPtonA(af, src, dst); }
static void sock_inet_ntop(int af, const void* src, char* dst, int size) { InetNtopA(af, src, dst, size); }
#else
// ── macOS / Linux : POSIX ──────────────────────────────────────────────────
#include <cstring>  // strerror
#define SOCK_LAST_ERROR       errno
#define SOCK_EWOULDBLOCK      EWOULDBLOCK
#define SOCK_ETIMEDOUT        ETIMEDOUT
#define SOCK_ECONNREFUSED     ECONNREFUSED
#define SOCK_EHOSTUNREACH     EHOSTUNREACH
static void sock_init()   { /* no-op on POSIX */ }
static void sock_cleanup(){ /* no-op on POSIX */ }
static void sock_nonblock(SOCKET s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
static void sock_inet_pton(int af, const char* src, void* dst) { inet_pton(af, src, dst); }
static void sock_inet_ntop(int af, const void* src, char* dst, int size) { inet_ntop(af, src, dst, size); }
#endif

void Socket::connectToServerAsync(const ServerInfo& serverInfo) {
    std::thread([this, serverInfo]() {
        connectToServer(serverInfo);
    }).detach();
}

bool Socket::connectToServer(const ServerInfo& serverInfo) {
    printf("[Socket] Initialisation socket...\n");
    sock_init();

    printf("[Socket] Creation du socket...\n");
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        printf("[Socket] ERREUR: creation du socket echouee\n");
        sock_cleanup();
        return false;
    }

    printf("[Socket] Tentative de connexion a %s:%d...\n", serverInfo.ip.c_str(), serverInfo.port);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(serverInfo.port);
    sock_inet_pton(AF_INET, serverInfo.ip.c_str(), &addr.sin_addr);

    if (connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        int error = SOCK_LAST_ERROR;
        switch (error) {
        case SOCK_ETIMEDOUT:
            printf("[Socket]   -> Le serveur n'a pas repondu (timeout)\n");
            printf("[Socket]   -> Verifier que le serveur est demarre\n");
            break;
        case SOCK_ECONNREFUSED:
            printf("[Socket]   -> Connexion refusee, il y a-t-il deja une autre connexion en cours ?\n");
            printf("[Socket]   -> Le port est peut-etre ferme\n");
            break;
        case SOCK_EHOSTUNREACH:
            printf("[Socket]   -> Hote injoignable\n");
            printf("[Socket]   -> Verifier l'adresse IP\n");
            break;
        default:
            printf("[Socket]   -> Erreur socket: %d (%s)\n", error,
#ifdef _WIN32
                   ""
#else
                   strerror(error)
#endif
            );
        }

        closesocket(m_socket);
        sock_cleanup();
        return false;
    }

    printf("[Socket] Connexion reussie!\n");

    sock_nonblock(m_socket);

    // Récupérer les informations locales (IP et port client)
    sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);
    if (getsockname(m_socket, (sockaddr*)&localAddr, &addrLen) == 0) {
        char ipStr[INET_ADDRSTRLEN];
        sock_inet_ntop(AF_INET, &localAddr.sin_addr, ipStr, sizeof(ipStr));
        m_localIP = ipStr;
        m_localPort = ntohs(localAddr.sin_port);
    }

    m_running.store(true);
    m_netThread = std::thread(&Socket::networkLoop, this);
    return true;
}


void Socket::stop() {
    if (!m_running.load()) return;
    m_running.store(false);

    if (m_netThread.joinable()) m_netThread.join();

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        sock_cleanup();
    }
}

void Socket::sendPacket(const std::string& data) {
    std::lock_guard<std::mutex> lock(m_outMutex);

    OutgoingMessage msg;
    msg.data = data;
    m_outgoing.push(msg);
}

void Socket::sendPacket(const Packet& packet) {
    std::lock_guard<std::mutex> lock(m_outMutex);
    
    OutgoingMessage msg;
    std::vector<char> serialized = packet.serialize();
    msg.data.assign(serialized.begin(), serialized.end());
    m_outgoing.push(msg);
}

bool Socket::pollEvent(ClientEvent& event) {
    std::lock_guard<std::mutex> lock(m_inMutex);
    if (m_incoming.empty()) return false;
    event = m_incoming.front();
    m_incoming.pop();
    return true;
}


// Thread qui tourne en fond

void Socket::networkLoop() {
    char buffer[Constants::Network::MAX_PACKET_SIZE];

    while (m_running.load()) {
        // Recevoir les données du serveur
        int n = recv(m_socket, buffer, sizeof(buffer), 0);

        if (n > 0) {
            // Données reçues
            std::lock_guard<std::mutex> lock(m_inMutex);
            ClientEvent evt;
            evt.type = EventType::DataReceived;
            evt.data = std::string(buffer, n);
            m_incoming.push(evt);
        }
        else if (n == 0 || (n == SOCKET_ERROR && SOCK_LAST_ERROR != SOCK_EWOULDBLOCK)) {
            // Déconnecté du serveur
            printf("[Socket] Deconnecte du serveur\n");

            std::lock_guard<std::mutex> lock(m_inMutex);
            ClientEvent evt;
            evt.type = EventType::ServerDisconnected;
            m_incoming.push(evt);

            m_running.store(false);
            break;
        }

        // Envoyer les données en attente
        {
            std::lock_guard<std::mutex> lock(m_outMutex);
            while (!m_outgoing.empty()) {
                const auto& msg = m_outgoing.front();
                send(m_socket, msg.data.c_str(), (int)msg.data.size(), 0);
                m_outgoing.pop();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
