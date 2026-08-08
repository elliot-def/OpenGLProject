#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <winsock2.h>
#include <stdexcept>

#include "constants/network.h"

// Types de paquets
enum class PacketType : uint16_t {
    // Connexion
    Ping = 1,
    Pong = 2,

    // Authentification
    Login = 10,
    LoginSuccess = 11,
    LoginFailed = 12,

    // Jeu
    PlayerJoin = 20,
    PlayerLeave = 21,
    PlayerMove = 22,
    GameState = 23,

    // Chat
    ChatMessage = 30,

    // Erreurs
    Error = 99
};

// Structure du header (8 octets)
#pragma pack(push, 1)
struct PacketHeader {
    uint16_t magic;   // 0xABCD
    uint16_t type;    // Type de paquet
    uint32_t length;  // Longueur du payload JSON
};
#pragma pack(pop)

constexpr size_t HEADER_SIZE = sizeof(PacketHeader); // 8 bytes

// Classe Packet
class Packet {
public:
    Packet() : m_type(PacketType::Ping) {}

    /*
    * Constructeur de paquet
    *
    * @param type Type de paquet
    * @param jsonPayload Payload JSON sous forme de chaîne
    *
    * @return Nouveau paquet
    */
    Packet(PacketType type, const std::string& jsonPayload)
        : m_type(type), m_payload(jsonPayload) {
    }

    // Sérialiser le paquet pour l'envoi
    std::vector<char> serialize() const;

    // Désérialiser depuis des données brutes
    static Packet deserialize(const char* data, size_t size);

    // Getters
    inline PacketType getType() const { return m_type; }
    inline const std::string& getPayload() const { return m_payload; }

    // Setters
    void setPayload(const std::string& payload) { m_payload = payload; }

private:
    PacketType m_type;
    std::string m_payload; // JSON
};