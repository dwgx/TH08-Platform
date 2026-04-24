#include "logging.h"

#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <string>

namespace th08_platform {

namespace {
std::mutex g_log_mtx;
FILE* g_log_file = nullptr;

std::string resolve_log_path()
{
    char appdata[MAX_PATH] = {};
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata) != S_OK) {
        return "th08_platform.log";
    }
    std::string dir = std::string(appdata) + "\\th08_platform";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir + "\\log.txt";
}
}  // namespace

void log_init()
{
    std::lock_guard<std::mutex> lk(g_log_mtx);
    if (g_log_file) return;
    auto path = resolve_log_path();
    g_log_file = fopen(path.c_str(), "a");
    if (g_log_file) {
        std::time_t t = std::time(nullptr);
        char ts[64];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        fprintf(g_log_file, "\n==== th08_platform session start %s ====\n", ts);
        fflush(g_log_file);
    }
}

void log_line(const char* fmt, ...)
{
    std::lock_guard<std::mutex> lk(g_log_mtx);
    if (!g_log_file) return;
    std::time_t t = std::time(nullptr);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&t));
    fprintf(g_log_file, "[%s] ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log_file, fmt, ap);
    va_end(ap);
    fputc('\n', g_log_file);
    fflush(g_log_file);
}

void log_shutdown()
{
    std::lock_guard<std::mutex> lk(g_log_mtx);
    if (g_log_file) {
        log_line("==== session end ====");
        fclose(g_log_file);
        g_log_file = nullptr;
    }
}

}  // namespace th08_platform
