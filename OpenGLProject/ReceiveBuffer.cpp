#include "ReceiveBuffer.h"

// Ajouter des données reçues
void ReceiveBuffer::append(const char* data, size_t size) {
    m_buffer.insert(m_buffer.end(), data, data + size);
}

// Essayer d'extraire un paquet complet
bool ReceiveBuffer::extractPacket(Packet& outPacket) {
    // Besoin du header complet (8 bytes)
    if (m_buffer.size() < HEADER_SIZE) {
        return false;
    }

    // Lire le header pour connaître la taille totale
    PacketHeader header;
    memcpy(&header, m_buffer.data(), HEADER_SIZE);

    // Conversion network -> host
    header.magic = ntohs(header.magic);
    header.type = ntohs(header.type);
    header.length = ntohl(header.length);

    // Vérifier le magic
    if (header.magic != Constants::Network::PACKET_MAGIC) {
        // Données corrompues, vider le buffer
        m_buffer.clear();
        throw std::runtime_error("Magic number invalide - buffer corrompu");
    }

    // Calculer la taille totale du paquet
    size_t totalSize = HEADER_SIZE + header.length;

    // Vérifier si on a le paquet complet
    if (m_buffer.size() < totalSize) {
        return false; // Attendre plus de données
    }

    // Extraire le paquet
    outPacket = Packet::deserialize(m_buffer.data(), totalSize);

    // Retirer le paquet du buffer
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + totalSize);

    return true;
}

void ReceiveBuffer::clear() {
    m_buffer.clear();
}

size_t ReceiveBuffer::size() const {
    return m_buffer.size();
}