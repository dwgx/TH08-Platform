#pragma once

#include <winsock2.h>

#include <array>
#include <cstdint>
#include <optional>

namespace th08_platform::net {

struct UdpDatagram {
    sockaddr_in from{};
    int size = 0;
    std::array<std::uint8_t, 16> bytes{};
};

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open(std::uint16_t listen_port);
    bool send(const sockaddr_in& addr, const void* data, int size);
    std::optional<UdpDatagram> recv(int timeout_ms);
    void close();

private:
    SOCKET socket_ = INVALID_SOCKET;
};

}  // namespace th08_platform::net
