// Buffer de réception pour gérer la fragmentation

#include "packet.h"

class ReceiveBuffer {
public:
    // Ajouter des données reçues
    void append(const char* data, size_t size);

    // Essayer d'extraire un paquet complet
    bool extractPacket(Packet& outPacket);

    // Vide le buffer
    void clear();

    // Taille actuelle du buffer
    size_t size() const;

private:
    std::vector<char> m_buffer;
};