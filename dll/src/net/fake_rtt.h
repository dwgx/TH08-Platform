#pragma once

#include "udp_socket.h"

#include <winsock2.h>

#include <array>
#include <cstdint>

namespace th08_platform::net::fake_rtt {

void configure(std::uint32_t delay_ms);
void shutdown();
std::uint32_t delay_ms();
bool send_or_queue(UdpSocket& socket, const sockaddr_in& addr, const void* data, int size);
void drain(UdpSocket& socket);

}  // namespace th08_platform::net::fake_rtt
