#pragma once

#include <cstdint>

namespace Constants {
	namespace Network {
        inline constexpr const int MAX_PACKET_SIZE = 1024;
        inline constexpr const char* SERVER_IP = "127.0.0.1";
        inline constexpr const int SERVER_PORT = 3333;

        inline constexpr uint16_t PACKET_MAGIC = 0xABCD; // Magic number for packet validation
	}
}