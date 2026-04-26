#include "lockstep.h"

#include "../logging.h"
#include "fake_rtt.h"
#include "udp_socket.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace th08_platform::net {

namespace {
constexpr std::array<std::uint8_t, 4> kMagic = {'T', 'H', '8', 'L'};
constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kFlagHandshake = 1u << 0;
constexpr std::uint8_t kFlagDisconnect = 1u << 1;
constexpr DWORD kHandshakeWaitMs = 5000;
constexpr DWORD kHandshakeRetryMs = 250;
constexpr std::uint32_t kMaxFrameLag = 10;
constexpr ULONGLONG kDisconnectTimeoutMs = 2000;

enum class ConnectionState {
    Disabled,
    Solo,
    Connected,
};

struct Packet {
    std::uint8_t flags = 0;
    std::uint32_t frame = 0;
    std::uint16_t input = 0;
    std::uint32_t checksum = 0;
};

struct LockstepState {
    std::mutex mutex;
    WSADATA wsa{};
    bool wsa_ready = false;
    bool configured = false;
    bool peer_known = false;
    ConnectionState connection = ConnectionState::Disabled;
    UdpSocket socket;
    sockaddr_in peer_addr{};
    std::string peer_label;
    std::map<std::uint32_t, ConfirmedFrame> peer_frames;
    std::uint16_t last_known_input = 0;
    ULONGLONG last_receive_tick = 0;
};

LockstepState g_state;

std::uint16_t load_u16(const std::uint8_t* ptr)
{
    return static_cast<std::uint16_t>(ptr[0]) |
           (static_cast<std::uint16_t>(ptr[1]) << 8);
}

std::uint32_t load_u32(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[0]) |
           (static_cast<std::uint32_t>(ptr[1]) << 8) |
           (static_cast<std::uint32_t>(ptr[2]) << 16) |
           (static_cast<std::uint32_t>(ptr[3]) << 24);
}

void store_u16(std::uint8_t* ptr, std::uint16_t value)
{
    ptr[0] = static_cast<std::uint8_t>(value & 0xffu);
    ptr[1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
}

void store_u32(std::uint8_t* ptr, std::uint32_t value)
{
    ptr[0] = static_cast<std::uint8_t>(value & 0xffu);
    ptr[1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    ptr[2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    ptr[3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
}

std::string format_endpoint(const sockaddr_in& addr)
{
    char ip[INET_ADDRSTRLEN] = {};
    const char* ip_str = ::InetNtopA(AF_INET, const_cast<IN_ADDR*>(&addr.sin_addr), ip,
                                     static_cast<DWORD>(sizeof(ip)));
    if (!ip_str) {
        ip_str = "unknown";
    }

    char out[64];
    std::snprintf(out, sizeof(out), "%s:%u", ip_str,
                  static_cast<unsigned>(ntohs(addr.sin_port)));
    return out;
}

bool same_endpoint(const sockaddr_in& lhs, const sockaddr_in& rhs)
{
    return lhs.sin_family == rhs.sin_family &&
           lhs.sin_port == rhs.sin_port &&
           lhs.sin_addr.s_addr == rhs.sin_addr.s_addr;
}

bool parse_peer_spec(const char* peer_spec, sockaddr_in& out_addr, std::string& out_label)
{
    if (!peer_spec || !*peer_spec) {
        return false;
    }

    const std::string spec(peer_spec);
    const auto split = spec.rfind(':');
    if (split == std::string::npos) {
        return false;
    }

    const std::string ip = spec.substr(0, split);
    const std::string port_str = spec.substr(split + 1);
    char* end = nullptr;
    const unsigned long port = std::strtoul(port_str.c_str(), &end, 10);
    if (!end || *end != '\0' || port == 0 || port > 65535) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    if (::InetPtonA(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        return false;
    }

    out_addr = addr;
    out_label = format_endpoint(out_addr);
    return true;
}

std::array<std::uint8_t, 20> encode_packet(const Packet& packet)
{
    std::array<std::uint8_t, 20> raw{};
    std::memcpy(raw.data(), kMagic.data(), kMagic.size());
    raw[4] = kVersion;
    raw[5] = packet.flags;
    store_u16(raw.data() + 6, 0);
    store_u32(raw.data() + 8, packet.frame);
    store_u16(raw.data() + 12, packet.input);
    store_u16(raw.data() + 14, 0);
    store_u32(raw.data() + 16, packet.checksum);
    return raw;
}

bool decode_packet(const UdpDatagram& datagram, Packet& out_packet)
{
    if (datagram.size != 20) {
        return false;
    }
    if (std::memcmp(datagram.bytes.data(), kMagic.data(), kMagic.size()) != 0) {
        return false;
    }
    if (datagram.bytes[4] != kVersion) {
        return false;
    }

    out_packet.flags = datagram.bytes[5];
    out_packet.frame = load_u32(datagram.bytes.data() + 8);
    out_packet.input = load_u16(datagram.bytes.data() + 12);
    out_packet.checksum = load_u32(datagram.bytes.data() + 16);
    return true;
}

bool send_packet_locked(const Packet& packet)
{
    if (!g_state.peer_known) {
        return false;
    }

    const auto raw = encode_packet(packet);
    return fake_rtt::send_or_queue(g_state.socket, g_state.peer_addr, raw.data(),
                                   static_cast<int>(raw.size()));
}

bool initialize_socket_locked(std::uint16_t listen_port)
{
    if (::WSAStartup(MAKEWORD(2, 2), &g_state.wsa) != 0) {
        th08_platform::log_line("lockstep init failed: WSAStartup failed");
        return false;
    }
    g_state.wsa_ready = true;

    if (!g_state.socket.open(listen_port)) {
        th08_platform::log_line("lockstep init failed: bind listen port %u failed",
                                static_cast<unsigned>(listen_port));
        g_state.socket.close();
        ::WSACleanup();
        g_state.wsa_ready = false;
        return false;
    }

    g_state.configured = true;
    g_state.connection = ConnectionState::Solo;
    g_state.peer_frames.clear();
    g_state.last_known_input = 0;
    g_state.last_receive_tick = 0;
    return true;
}

void mark_connected_locked()
{
    if (g_state.connection != ConnectionState::Connected) {
        g_state.connection = ConnectionState::Connected;
        g_state.last_receive_tick = ::GetTickCount64();
        th08_platform::log_line("peer connected at %s", g_state.peer_label.c_str());
    }
}

void drop_stale_frames_locked(std::uint32_t current_frame)
{
    for (auto it = g_state.peer_frames.begin(); it != g_state.peer_frames.end();) {
        const std::uint32_t frame = it->first;
        if (frame + kMaxFrameLag < current_frame) {
            it = g_state.peer_frames.erase(it);
        } else {
            ++it;
        }
    }
}

void fall_back_to_solo_locked(const char* reason)
{
    if (g_state.connection == ConnectionState::Connected) {
        th08_platform::log_line("%s", reason);
    }
    g_state.connection = ConnectionState::Solo;
    g_state.peer_frames.clear();
    g_state.last_receive_tick = 0;
}

void handle_packet_locked(const Packet& packet, const sockaddr_in& from)
{
    if (!g_state.peer_known) {
        g_state.peer_addr = from;
        g_state.peer_label = format_endpoint(from);
        g_state.peer_known = true;
    } else if (!same_endpoint(from, g_state.peer_addr)) {
        return;
    }

    if ((packet.flags & kFlagDisconnect) != 0) {
        fall_back_to_solo_locked("peer disconnected, falling back to solo");
        return;
    }

    if ((packet.flags & kFlagHandshake) != 0) {
        const bool was_connected = g_state.connection == ConnectionState::Connected;
        mark_connected_locked();
        if (!was_connected) {
            send_packet_locked(Packet{.flags = kFlagHandshake});
        }
    }

    if ((packet.flags & kFlagHandshake) == 0 && g_state.connection == ConnectionState::Connected) {
        g_state.peer_frames[packet.frame] = ConfirmedFrame{
            .frame = packet.frame,
            .input = packet.input,
            .checksum = packet.checksum,
        };
        g_state.last_known_input = packet.input;
        g_state.last_receive_tick = ::GetTickCount64();
    }
}

void pump_locked(int timeout_ms)
{
    if (!g_state.configured) {
        return;
    }

    fake_rtt::drain(g_state.socket);

    while (true) {
        const auto datagram = g_state.socket.recv(timeout_ms);
        if (!datagram.has_value()) {
            if (g_state.connection == ConnectionState::Connected &&
                g_state.last_receive_tick != 0 &&
                (::GetTickCount64() - g_state.last_receive_tick) >= kDisconnectTimeoutMs) {
                fall_back_to_solo_locked("peer disconnected, falling back to solo");
            }
            return;
        }

        Packet packet;
        if (decode_packet(*datagram, packet)) {
            handle_packet_locked(packet, datagram->from);
        }

        timeout_ms = 0;
    }
}

bool wait_for_handshake_locked(DWORD timeout_ms)
{
    const ULONGLONG start = ::GetTickCount64();
    ULONGLONG next_send = start;

    while (::GetTickCount64() - start < timeout_ms) {
        const ULONGLONG now = ::GetTickCount64();
        if (now >= next_send) {
            send_packet_locked(Packet{.flags = kFlagHandshake});
            next_send = now + kHandshakeRetryMs;
        }

        pump_locked(50);
        if (g_state.connection == ConnectionState::Connected) {
            return true;
        }
    }

    return false;
}
}  // namespace

bool initialize(const char* peer_spec, std::uint16_t listen_port)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);

    if (g_state.configured) {
        return true;
    }

    sockaddr_in peer_addr{};
    std::string peer_label;
    if (!parse_peer_spec(peer_spec, peer_addr, peer_label)) {
        th08_platform::log_line("lockstep init failed: invalid TH08_PLATFORM_PEER='%s'",
                                peer_spec ? peer_spec : "");
        return false;
    }

    if (!initialize_socket_locked(listen_port)) {
        return false;
    }

    g_state.peer_known = true;
    g_state.peer_addr = peer_addr;
    g_state.peer_label = peer_label;

    if (!wait_for_handshake_locked(kHandshakeWaitMs)) {
        th08_platform::log_line("peer never appeared, running solo");
    }

    return true;
}

bool initialize_listen_only(std::uint16_t listen_port)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);

    if (g_state.configured) {
        return true;
    }

    if (!initialize_socket_locked(listen_port)) {
        return false;
    }

    g_state.peer_known = false;
    g_state.peer_addr = {};
    g_state.peer_label.clear();
    return true;
}

void shutdown()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);

    if (!g_state.configured) {
        return;
    }

    if (g_state.connection == ConnectionState::Connected) {
        send_packet_locked(Packet{.flags = kFlagDisconnect});
    }

    g_state.socket.close();
    g_state.peer_frames.clear();
    g_state.peer_known = false;
    g_state.peer_label.clear();
    g_state.configured = false;
    g_state.connection = ConnectionState::Disabled;
    g_state.last_known_input = 0;
    g_state.last_receive_tick = 0;
    fake_rtt::shutdown();

    if (g_state.wsa_ready) {
        ::WSACleanup();
        g_state.wsa_ready = false;
    }
}

void set_fake_rtt_ms(std::uint32_t fake_rtt_ms)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    fake_rtt::configure(fake_rtt_ms);
}

bool is_configured()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.configured;
}

bool is_connected()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.connection == ConnectionState::Connected;
}

void poll()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    pump_locked(0);
}

void send_local_input(std::uint64_t frame, std::uint16_t input, std::uint32_t checksum)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);

    if (g_state.connection != ConnectionState::Connected) {
        return;
    }

    if (!send_packet_locked(Packet{
            .flags = 0,
            .frame = static_cast<std::uint32_t>(frame),
            .input = input,
            .checksum = checksum,
        })) {
        th08_platform::log_line("lockstep send failed to %s", g_state.peer_label.c_str());
    }

    pump_locked(0);
}

std::optional<ConfirmedFrame> peek_confirmed_frame(std::uint64_t frame)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);

    if (g_state.connection != ConnectionState::Connected) {
        return std::nullopt;
    }

    pump_locked(0);

    const auto target_frame = static_cast<std::uint32_t>(frame);
    if (const auto found = g_state.peer_frames.find(target_frame);
        found != g_state.peer_frames.end()) {
        return found->second;
    }

    return std::nullopt;
}

std::vector<ConfirmedFrame> consume_confirmed_frames_up_to(std::uint64_t frame)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    std::vector<ConfirmedFrame> confirmed_frames;

    if (g_state.connection != ConnectionState::Connected) {
        return confirmed_frames;
    }

    pump_locked(0);

    const auto target_frame = static_cast<std::uint32_t>(frame);
    drop_stale_frames_locked(target_frame);

    for (auto it = g_state.peer_frames.begin(); it != g_state.peer_frames.end();) {
        if (it->first <= target_frame) {
            confirmed_frames.push_back(it->second);
            it = g_state.peer_frames.erase(it);
        } else {
            ++it;
        }
    }

    return confirmed_frames;
}

std::uint16_t last_known_remote_input()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.last_known_input;
}

}  // namespace th08_platform::net
