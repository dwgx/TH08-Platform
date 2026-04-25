#include "udp_socket.h"

#include <ws2tcpip.h>

namespace th08_platform::net {

UdpSocket::~UdpSocket()
{
    close();
}

bool UdpSocket::open(std::uint16_t listen_port)
{
    close();

    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(listen_port);

    if (::bind(socket_, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) ==
        SOCKET_ERROR) {
        close();
        return false;
    }

    return true;
}

bool UdpSocket::send(const sockaddr_in& addr, const void* data, int size)
{
    if (socket_ == INVALID_SOCKET) {
        return false;
    }

    return ::sendto(socket_, static_cast<const char*>(data), size, 0,
                    reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == size;
}

std::optional<UdpDatagram> UdpSocket::recv(int timeout_ms)
{
    if (socket_ == INVALID_SOCKET) {
        return std::nullopt;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_, &read_fds);

    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    const int ready = ::select(0, &read_fds, nullptr, nullptr, &tv);
    if (ready <= 0) {
        return std::nullopt;
    }

    UdpDatagram datagram;
    int from_len = sizeof(datagram.from);
    datagram.size = ::recvfrom(socket_, reinterpret_cast<char*>(datagram.bytes.data()),
                               static_cast<int>(datagram.bytes.size()), 0,
                               reinterpret_cast<sockaddr*>(&datagram.from), &from_len);
    if (datagram.size == SOCKET_ERROR) {
        return std::nullopt;
    }

    return datagram;
}

void UdpSocket::close()
{
    if (socket_ != INVALID_SOCKET) {
        ::closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
}

}  // namespace th08_platform::net
