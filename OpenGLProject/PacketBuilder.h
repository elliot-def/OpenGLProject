#include "Packet.h"

// Fonctions helpers pour créer des paquets JSON facilement
namespace PacketBuilder {
    inline std::string buildJSON(const std::vector<std::pair<std::string, std::string>>& fields) {
        std::string json = "{";
        for (size_t i = 0; i < fields.size(); i++) {
            json += "\"" + fields[i].first + "\":\"" + fields[i].second + "\"";
            if (i < fields.size() - 1) json += ",";
        }
        json += "}";
        return json;
    }

    inline Packet createPing() {
        return Packet(PacketType::Ping, "{}");
    }

    inline Packet createPong() {
        return Packet(PacketType::Pong, "{}");
    }

    inline Packet createLogin(const std::string& username, const std::string& password) {
        std::string json = buildJSON({
            {"username", username},
            {"password", password}
            });
        return Packet(PacketType::Login, json);
    }

    inline Packet createChatMessage(const std::string& sender, const std::string& message) {
        std::string json = buildJSON({
            {"sender", sender},
            {"message", message}
            });
        return Packet(PacketType::ChatMessage, json);
    }

    inline Packet createPlayerMove(int x, int y, int z) {
        std::string json = "{\"x\":" + std::to_string(x) +
            ",\"y\":" + std::to_string(y) +
            ",\"z\":" + std::to_string(z) + "}";
        return Packet(PacketType::PlayerMove, json);
    }
}