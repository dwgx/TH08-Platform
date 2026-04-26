#include "pump_thread.h"

#include "lockstep.h"
#include "../logging.h"

#include <windows.h>

#include <atomic>
#include <thread>

namespace th08_platform::net {

namespace {

std::thread g_pump_thread;
std::atomic<bool> g_pump_stop{false};
std::atomic<bool> g_pump_running{false};

void pump_loop()
{
    while (!g_pump_stop.load(std::memory_order_acquire)) {
        poll();
        ::Sleep(16);  // ~60Hz; matches game tick budget
    }
}

}  // namespace

bool start_pump_thread()
{
    if (g_pump_running.load(std::memory_order_acquire)) {
        return true;
    }

    g_pump_stop.store(false, std::memory_order_release);
    g_pump_running.store(true, std::memory_order_release);

    try {
        g_pump_thread = std::thread(pump_loop);
    } catch (...) {
        g_pump_running.store(false, std::memory_order_release);
        log_line("pump_thread: std::thread construction failed");
        return false;
    }

    log_line("pump_thread: started (16ms tick)");
    return true;
}

void stop_pump_thread()
{
    if (!g_pump_running.load(std::memory_order_acquire)) {
        return;
    }

    g_pump_stop.store(true, std::memory_order_release);
    if (g_pump_thread.joinable()) {
        g_pump_thread.join();
    }
    g_pump_running.store(false, std::memory_order_release);
    log_line("pump_thread: stopped");
}

}  // namespace th08_platform::net
