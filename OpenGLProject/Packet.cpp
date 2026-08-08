#include "Packet.h"

// Sérialiser le paquet pour l'envoi
std::vector<char> Packet::serialize() const {
    PacketHeader header;

    header.magic = htons(Constants::Network::PACKET_MAGIC);
    header.type = htons(static_cast<uint16_t>(m_type));
    header.length = htonl(static_cast<uint32_t>(m_payload.size()));

    std::vector<char> buffer(HEADER_SIZE + m_payload.size());

    // Copier le header
    memcpy(buffer.data(), &header, HEADER_SIZE);

    // Copier le payload JSON
    if (!m_payload.empty()) {
        memcpy(buffer.data() + HEADER_SIZE, m_payload.data(), m_payload.size());
    }

    return buffer;
}

/*
 * Désérialiser depuis des données brutes
 *
 * @param data Pointeur vers les données brutes
 * @param size Taille des données
 *
 * @return Paquet désérialisé
*/
Packet Packet::deserialize(const char* data, size_t size) {
    if (size < HEADER_SIZE) {
        throw std::runtime_error("Paquet trop petit");
    }

    PacketHeader header;
    // Copie le header depuis les données
    memcpy(&header, data, HEADER_SIZE);

    // Conversion network -> host
    header.magic = ntohs(header.magic);
    header.type = ntohs(header.type);
    header.length = ntohl(header.length);

    // Vérifier le magic number
    if (header.magic != Constants::Network::PACKET_MAGIC) {
        throw std::runtime_error("Magic number invalide");
    }

    // Vérifier la longueur
    if (size < HEADER_SIZE + header.length) {
        throw std::runtime_error("Paquet incomplet");
    }

    // Extraire le payload JSON
    std::string payload;
    if (header.length > 0) {
        payload.assign(data + HEADER_SIZE, header.length);
    }

    return Packet(static_cast<PacketType>(header.type), payload);
}