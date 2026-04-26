#pragma once

namespace th08_platform::net {

// Background thread that calls net::poll() every ~16ms. Required because
// nothing in the existing codebase drives poll() outside the peer's
// initial 5-second handshake wait — title-screen and pre-stage frames
// would otherwise leave the host's UDP buffer un-drained, blocking
// PING/PONG handshake completion.
//
// Spawn after net::initialize / initialize_listen_only succeeds. Stop
// from DLL_PROCESS_DETACH BEFORE net::shutdown so the thread isn't
// touching the socket at shutdown.
bool start_pump_thread();
void stop_pump_thread();

}  // namespace th08_platform::net
