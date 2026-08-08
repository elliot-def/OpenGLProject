#include "Socket.h"
#include "Packet.h"

#include "constants/network.h"

void Socket::connectToServerAsync(const ServerInfo& serverInfo) {
    std::thread([this, serverInfo]() {
        connectToServer(serverInfo);
    }).detach();
}

bool Socket::connectToServer(const ServerInfo& serverInfo) {
    printf("[Socket] Initialisation Winsock...\n");
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[Socket] ERREUR: WSAStartup failed\n");
        return false;
    }

    printf("[Socket] Creation du socket...\n");
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        printf("[Socket] ERREUR: creation du socket echouee\n");
        WSACleanup();
        return false;
    }

    printf("[Socket] Tentative de connexion a %s:%d...\n", serverInfo.ip.c_str(), serverInfo.port);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(serverInfo.port);
    InetPtonA(AF_INET, serverInfo.ip.c_str(), &addr.sin_addr);

    if (connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        switch (error) {
        case WSAETIMEDOUT:
            printf("[Socket]   -> Le serveur n'a pas repondu (timeout)\n");
            printf("[Socket]   -> Verifier que le serveur est demarre\n");
            break;
        case WSAECONNREFUSED:
            printf("[Socket]   -> Connexion refusee, il y a-t-il deja une autre connexion en cours ?\n");
            printf("[Socket]   -> Le port est peut-etre ferme\n");
            break;
        case WSAEHOSTUNREACH:
            printf("[Socket]   -> Hote injoignable\n");
            printf("[Socket]   -> Verifier l'adresse IP\n");
            break;
        default:
            printf("[Socket]   -> Erreur inconnue\n");
        }

        closesocket(m_socket);
        WSACleanup();
        return false;
    }

    printf("[Socket] Connexion reussie!\n");

    u_long mode = 1;
    ioctlsocket(m_socket, FIONBIO, &mode);

    // Récupérer les informations locales (IP et port client)
    sockaddr_in localAddr;
    int addrLen = sizeof(localAddr);
    if (getsockname(m_socket, (sockaddr*)&localAddr, &addrLen) == 0) {
        char ipStr[INET_ADDRSTRLEN];
        InetNtopA(AF_INET, &localAddr.sin_addr, ipStr, sizeof(ipStr));
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
        WSACleanup();
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
        else if (n == 0 || (n == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
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