#pragma once
// Simple thread-safe file logger for Phase 0 diagnostics.
// Writes to %LOCALAPPDATA%\th08_platform\log.txt

namespace th08_platform {

void log_init();
void log_line(const char* fmt, ...);
void log_shutdown();

}  // namespace th08_platform
