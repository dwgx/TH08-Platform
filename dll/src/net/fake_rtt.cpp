#include "fake_rtt.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <cstring>

namespace th08_platform::net::fake_rtt {

namespace {

struct QueuedPacket {
    ULONGLONG deadline = 0;
    sockaddr_in addr{};
    int size = 0;
    std::array<std::uint8_t, 64> bytes{};
};

std::uint32_t g_delay_ms = 0;
std::deque<QueuedPacket> g_queue;

}  // namespace

void configure(std::uint32_t delay_ms)
{
    g_delay_ms = delay_ms;
    g_queue.clear();
}

void shutdown()
{
    g_queue.clear();
    g_delay_ms = 0;
}

std::uint32_t delay_ms()
{
    return g_delay_ms;
}

bool send_or_queue(UdpSocket& socket, const sockaddr_in& addr, const void* data, int size)
{
    if (g_delay_ms == 0 || size <= 0) {
        return socket.send(addr, data, size);
    }

    QueuedPacket packet;
    packet.deadline = ::GetTickCount64() + g_delay_ms;
    packet.addr = addr;
    packet.size = std::min<int>(size, static_cast<int>(packet.bytes.size()));
    std::memcpy(packet.bytes.data(), data, packet.size);
    g_queue.push_back(packet);
    return true;
}

void drain(UdpSocket& socket)
{
    if (g_delay_ms == 0) {
        return;
    }

    const ULONGLONG now = ::GetTickCount64();
    while (!g_queue.empty() && g_queue.front().deadline <= now) {
        const auto packet = g_queue.front();
        g_queue.pop_front();
        socket.send(packet.addr, packet.bytes.data(), packet.size);
    }
}

}  // namespace th08_platform::net::fake_rtt
