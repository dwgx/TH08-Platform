#include "lockstep.h"

#include "../logging.h"
#include "fake_rtt.h"
#include "protocol.h"
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

using protocol::Bits;
using protocol::Control;
using protocol::CtrlPack;
using protocol::IGC_NONE;
using protocol::kKeyPackFrameNum;
using protocol::kMultiNetVer;
using protocol::Pack;
using protocol::PACK_HELLO;
using protocol::PACK_PING;
using protocol::PACK_PONG;
using protocol::PACK_USUAL;
using protocol::ReadFromInt;
using protocol::WriteToInt;

static_assert(sizeof(Pack) == protocol::kExpectedPackSize,
              "Pack layout drifted from RUEEE wire format");

constexpr DWORD kHandshakeWaitMs    = 5000;
constexpr DWORD kHandshakeRetryMs   = 250;
constexpr DWORD kPeriodicPingMs     = 250;
constexpr DWORD kDisconnectTimeoutMs = 4000;

enum class ConnectionState {
    Disabled,
    Solo,
    Connected,
};

struct LockstepState {
    std::mutex mutex;
    WSADATA wsa{};
    bool wsa_ready = false;
    bool configured = false;
    bool peer_known = false;
    bool is_host = false;
    ConnectionState connection = ConnectionState::Disabled;
    UdpSocket socket;
    sockaddr_in peer_addr{};
    std::string peer_label;

    std::map<std::int32_t, Bits<16>> ctrl_bits_self;
    std::map<std::int32_t, Bits<16>> ctrl_bits_rcved;

    std::uint32_t next_seq = 1;
    ULONGLONG last_periodic_ping_tick = 0;
    ULONGLONG last_receive_tick = 0;
    std::uint64_t last_rtt_ms = 0;
    std::uint32_t host_seed = 0;
    bool host_seed_generated = false;
    std::uint32_t peer_received_seed = 0;
    bool peer_seed_received = false;

    std::int32_t last_sent_frame = -kKeyPackFrameNum;
};

LockstepState g_state;

ULONGLONG now_ms()
{
    return ::GetTickCount64();
}

bool same_endpoint(const sockaddr_in& lhs, const sockaddr_in& rhs)
{
    return lhs.sin_family == rhs.sin_family &&
           lhs.sin_port == rhs.sin_port &&
           lhs.sin_addr.s_addr == rhs.sin_addr.s_addr;
}

std::string format_endpoint(const sockaddr_in& addr)
{
    char ip[INET_ADDRSTRLEN] = {};
    const char* ip_str = ::InetNtopA(AF_INET, const_cast<IN_ADDR*>(&addr.sin_addr),
                                     ip, static_cast<DWORD>(sizeof(ip)));
    if (!ip_str) {
        ip_str = "unknown";
    }
    char out[64];
    std::snprintf(out, sizeof(out), "%s:%u", ip_str,
                  static_cast<unsigned>(ntohs(addr.sin_port)));
    return out;
}

bool parse_peer_spec(const char* peer_spec, sockaddr_in& out_addr,
                     std::string& out_label)
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

bool send_pack_locked(const Pack& pack)
{
    if (!g_state.peer_known) {
        return false;
    }
    return fake_rtt::send_or_queue(g_state.socket, g_state.peer_addr,
                                   reinterpret_cast<const void*>(&pack),
                                   static_cast<int>(sizeof(Pack)));
}

void mark_connected_locked()
{
    if (g_state.connection != ConnectionState::Connected) {
        g_state.connection = ConnectionState::Connected;
        g_state.last_receive_tick = now_ms();
        log_line("peer connected at %s", g_state.peer_label.c_str());
    }
}

void send_ping_locked(Control ctrl_type = protocol::Ctrl_No_Ctrl)
{
    if (!g_state.peer_known) {
        return;
    }
    Pack p;
    p.type = PACK_PING;
    p.seq = g_state.next_seq++;
    p.sendTick = now_ms();
    p.echoTick = 0;
    if (g_state.is_host && g_state.host_seed_generated) {
        p.ctrl.ctrl_type = protocol::Ctrl_Set_InitSetting;
        p.ctrl.init_setting.delay = static_cast<std::int32_t>(g_state.host_seed);
        p.ctrl.init_setting.ver = static_cast<std::int32_t>(kMultiNetVer);
    } else {
        p.ctrl.ctrl_type = ctrl_type;
    }
    p.ctrl.frame = 0;
    p.ctrl.init_setting.ver = static_cast<std::int32_t>(kMultiNetVer);
    send_pack_locked(p);
    g_state.last_periodic_ping_tick = now_ms();
}

void handle_packet_locked(const Pack& packet, const sockaddr_in& from)
{
    // Listen-only host: first valid packet teaches us the peer addr.
    if (!g_state.peer_known) {
        g_state.peer_addr = from;
        g_state.peer_label = format_endpoint(from);
        g_state.peer_known = true;
        log_line("learned peer addr from inbound packet: %s",
                 g_state.peer_label.c_str());
    } else if (!same_endpoint(from, g_state.peer_addr)) {
        return;  // unsolicited packet from someone else
    }

    g_state.last_receive_tick = now_ms();

    switch (packet.type) {
    case PACK_HELLO:
    case PACK_PING: {
        Pack reply;
        reply.type = PACK_PONG;
        reply.seq = packet.seq;
        reply.sendTick = packet.sendTick;
        reply.echoTick = now_ms();
        reply.ctrl = packet.ctrl;
        reply.ctrl.init_setting.ver = static_cast<std::int32_t>(kMultiNetVer);
        send_pack_locked(reply);
        mark_connected_locked();
        if (packet.ctrl.ctrl_type == protocol::Ctrl_Set_InitSetting) {
            if (!g_state.peer_seed_received) {
                g_state.peer_received_seed =
                    static_cast<std::uint32_t>(packet.ctrl.init_setting.delay);
                g_state.peer_seed_received = true;
                log_line("phase 6c: received shared seed = 0x%08x",
                         g_state.peer_received_seed);
            }
        }
        break;
    }
    case PACK_PONG: {
        const auto rtt = now_ms() - packet.sendTick;
        g_state.last_rtt_ms = rtt;
        mark_connected_locked();
        break;
    }
    case PACK_USUAL: {
        if (packet.ctrl.ctrl_type == protocol::Ctrl_Key) {
            const std::int32_t newest = packet.ctrl.frame;
            for (int i = 0; i < kKeyPackFrameNum; ++i) {
                const std::int32_t f = newest - i;
                if (f < 0) continue;
                g_state.ctrl_bits_rcved[f] = packet.ctrl.keys[i];
            }
        }
        break;
    }
    default:
        break;
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
            break;
        }
        if (datagram->size != static_cast<int>(sizeof(Pack))) {
            timeout_ms = 0;
            continue;
        }
        Pack packet;
        std::memcpy(&packet, datagram->bytes.data(), sizeof(Pack));
        handle_packet_locked(packet, datagram->from);
        timeout_ms = 0;
    }

    if (g_state.peer_known && g_state.connection == ConnectionState::Connected) {
        const ULONGLONG t = now_ms();
        if (t - g_state.last_periodic_ping_tick >= kPeriodicPingMs) {
            send_ping_locked();
        }
        if (g_state.last_receive_tick != 0 &&
            t - g_state.last_receive_tick >= kDisconnectTimeoutMs) {
            log_line("peer silent for %llu ms, falling back to solo",
                     static_cast<unsigned long long>(t - g_state.last_receive_tick));
            g_state.connection = ConnectionState::Solo;
        }
    }
}

bool wait_for_handshake_locked(DWORD timeout_ms)
{
    const ULONGLONG start = now_ms();
    ULONGLONG next_send = start;

    while (now_ms() - start < timeout_ms) {
        const ULONGLONG t = now_ms();
        if (t >= next_send) {
            send_ping_locked();
            next_send = t + kHandshakeRetryMs;
        }
        pump_locked(50);
        if (g_state.connection == ConnectionState::Connected) {
            return true;
        }
    }
    return false;
}

bool initialize_socket_locked(std::uint16_t listen_port)
{
    if (::WSAStartup(MAKEWORD(2, 2), &g_state.wsa) != 0) {
        log_line("lockstep init failed: WSAStartup failed");
        return false;
    }
    g_state.wsa_ready = true;

    if (!g_state.socket.open(listen_port)) {
        log_line("lockstep init failed: bind listen port %u failed",
                 static_cast<unsigned>(listen_port));
        g_state.socket.close();
        ::WSACleanup();
        g_state.wsa_ready = false;
        return false;
    }

    g_state.configured = true;
    g_state.connection = ConnectionState::Solo;
    g_state.ctrl_bits_self.clear();
    g_state.ctrl_bits_rcved.clear();
    g_state.next_seq = 1;
    g_state.last_periodic_ping_tick = 0;
    g_state.last_receive_tick = 0;
    g_state.last_rtt_ms = 0;
    g_state.host_seed = 0;
    g_state.host_seed_generated = false;
    g_state.peer_received_seed = 0;
    g_state.peer_seed_received = false;
    g_state.last_sent_frame = -kKeyPackFrameNum;
    return true;
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
        log_line("lockstep init failed: invalid TH08_PLATFORM_PEER='%s'",
                 peer_spec ? peer_spec : "");
        return false;
    }

    if (!initialize_socket_locked(listen_port)) {
        return false;
    }

    g_state.peer_known = true;
    g_state.is_host = false;
    g_state.peer_addr = peer_addr;
    g_state.peer_label = peer_label;

    if (!wait_for_handshake_locked(kHandshakeWaitMs)) {
        log_line("peer never appeared during init, will keep retrying via pump");
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
    g_state.is_host = true;
    g_state.peer_addr = {};
    g_state.peer_label.clear();
    if (!g_state.host_seed_generated) {
        char env[32] = {};
        const DWORD env_len =
            ::GetEnvironmentVariableA("TH08_PLATFORM_SEED", env, sizeof(env));
        std::uint32_t seed;
        if (env_len > 0) {
            char* end = nullptr;
            seed = std::strtoul(env, &end, 0);
        } else {
            seed = static_cast<std::uint32_t>(
                ::GetTickCount64() ^ ::GetCurrentProcessId());
        }
        g_state.host_seed = seed;
        g_state.host_seed_generated = true;
        log_line("phase 6c: generated host seed = 0x%08x", seed);
    }
    return true;
}

void shutdown()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.configured) {
        return;
    }
    g_state.socket.close();
    g_state.ctrl_bits_self.clear();
    g_state.ctrl_bits_rcved.clear();
    g_state.peer_known = false;
    g_state.peer_label.clear();
    g_state.configured = false;
    g_state.connection = ConnectionState::Disabled;
    g_state.last_periodic_ping_tick = 0;
    g_state.last_receive_tick = 0;
    g_state.last_rtt_ms = 0;
    g_state.host_seed = 0;
    g_state.host_seed_generated = false;
    g_state.peer_received_seed = 0;
    g_state.peer_seed_received = false;
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

void capture_local_input(std::uint64_t frame, std::uint16_t input)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.configured) return;

    Bits<16> b;
    ReadFromInt(b, input);
    const auto f = static_cast<std::int32_t>(frame);
    g_state.ctrl_bits_self[f] = b;

    constexpr std::int32_t kKeepFrames = 60;
    while (!g_state.ctrl_bits_self.empty() &&
           g_state.ctrl_bits_self.begin()->first < f - kKeepFrames) {
        g_state.ctrl_bits_self.erase(g_state.ctrl_bits_self.begin());
    }
    while (!g_state.ctrl_bits_rcved.empty() &&
           g_state.ctrl_bits_rcved.begin()->first < f - kKeepFrames) {
        g_state.ctrl_bits_rcved.erase(g_state.ctrl_bits_rcved.begin());
    }
}

void send_input_pack_if_due(std::uint64_t frame)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.configured ||
        g_state.connection != ConnectionState::Connected) {
        return;
    }
    const auto f = static_cast<std::int32_t>(frame);
    if (f - g_state.last_sent_frame < kKeyPackFrameNum) {
        return;
    }
    g_state.last_sent_frame = f;

    Pack p;
    p.type = PACK_USUAL;
    p.seq = g_state.next_seq++;
    p.sendTick = now_ms();
    p.echoTick = 0;
    p.ctrl.ctrl_type = protocol::Ctrl_Key;
    p.ctrl.frame = f;
    for (int i = 0; i < kKeyPackFrameNum; ++i) {
        const std::int32_t want = f - i;
        const auto it = g_state.ctrl_bits_self.find(want);
        if (it != g_state.ctrl_bits_self.end()) {
            p.ctrl.keys[i] = it->second;
        } else {
            p.ctrl.keys[i].Clear();
        }
        p.ctrl.igc_type[i] = IGC_NONE;
        p.ctrl.rng_seed[i] = 0;
    }
    send_pack_locked(p);
}

std::uint16_t peek_remote_input(std::uint64_t frame)
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    const auto it = g_state.ctrl_bits_rcved.find(static_cast<std::int32_t>(frame));
    if (it == g_state.ctrl_bits_rcved.end()) {
        return 0;
    }
    std::uint16_t out = 0;
    WriteToInt(it->second, out);
    return out;
}

std::uint64_t last_rtt_ms()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.last_rtt_ms;
}

bool has_shared_seed()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.is_host ? g_state.host_seed_generated : g_state.peer_seed_received;
}

std::uint32_t shared_seed()
{
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.is_host ? g_state.host_seed : g_state.peer_received_seed;
}

void send_local_input(std::uint64_t /*frame*/, std::uint16_t /*input*/,
                      std::uint32_t /*checksum*/)
{
}

std::optional<ConfirmedFrame> peek_confirmed_frame(std::uint64_t /*frame*/)
{
    return std::nullopt;
}

std::vector<ConfirmedFrame> consume_confirmed_frames_up_to(std::uint64_t /*frame*/)
{
    return {};
}

std::uint16_t last_known_remote_input()
{
    return 0;
}

}  // namespace th08_platform::net
