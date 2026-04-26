// th08_platform_loader.exe
//
// Launches th08.exe and injects th08_platform.dll via the classic
// CreateRemoteThread + LoadLibraryA pattern. The loader is deliberately
// minimal - everything interesting lives inside the DLL.
//
// Usage: th08_platform_loader.exe <path_to_th08.exe>
//        [--listen <port>] [--peer <ip:port>] [--host]

#include <windows.h>
#include <psapi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string resolve_dll_path()
{
    char self[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::filesystem::path p(self);
    p.replace_filename("th08_platform.dll");
    return p.string();
}

bool is_flag(const char* value)
{
    return value && value[0] == '-' && value[1] == '-';
}

bool inject_dll(HANDLE proc, const std::string& dll_path)
{
    const SIZE_T sz = dll_path.size() + 1;
    LPVOID remote = VirtualAllocEx(proc, nullptr, sz,
                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        std::fprintf(stderr, "VirtualAllocEx failed: %lu\n", GetLastError());
        return false;
    }
    if (!WriteProcessMemory(proc, remote, dll_path.c_str(), sz, nullptr)) {
        std::fprintf(stderr, "WriteProcessMemory failed: %lu\n", GetLastError());
        return false;
    }

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    auto loadlib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(kernel32, "LoadLibraryA"));
    if (!loadlib) {
        std::fprintf(stderr, "LoadLibraryA not found\n");
        return false;
    }

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadlib, remote, 0, nullptr);
    if (!thread) {
        std::fprintf(stderr, "CreateRemoteThread failed: %lu\n", GetLastError());
        return false;
    }
    WaitForSingleObject(thread, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);

    if (exit_code == 0) {
        std::fprintf(stderr, "LoadLibraryA returned 0 - DLL failed to load\n");
        return false;
    }
    std::printf("LoadLibraryA returned module handle 0x%lx\n", exit_code);
    return true;
}

int usage()
{
    std::fprintf(stderr,
        "usage: th08_platform_loader.exe <path_to_th08.exe> [--listen <port>] [--peer <ip:port>] [--host]\n"
        "\n"
        "options:\n"
        "  --listen <port>    set TH08_PLATFORM_LISTEN to <port>\n"
        "  --peer <ip:port>   set TH08_PLATFORM_PEER to <ip:port>\n"
        "  --host             set TH08_PLATFORM_HOST=1 and default listen port 7480\n"
        "                     unless --listen is also given\n"
        "\n"
        "examples:\n"
        "  th08_platform_loader.exe C:\\games\\th08\\th08.exe --host\n"
        "  th08_platform_loader.exe C:\\games\\th08\\th08.exe --host --listen 7481\n"
        "  th08_platform_loader.exe C:\\games\\th08\\th08.exe --peer 127.0.0.1:7480\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) return usage();

    const std::string target = argv[1];
    const std::string dll = resolve_dll_path();
    const char* listen_value = nullptr;
    const char* peer_value = nullptr;
    bool host_mode = false;

    if (!std::filesystem::exists(target)) {
        std::fprintf(stderr, "target not found: %s\n", target.c_str());
        return 1;
    }
    if (!std::filesystem::exists(dll)) {
        std::fprintf(stderr, "th08_platform.dll not found next to loader at: %s\n",
                     dll.c_str());
        return 1;
    }

    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--peer") == 0) {
            if (i + 1 >= argc || is_flag(argv[i + 1])) return usage();
            peer_value = argv[++i];
        } else if (std::strcmp(argv[i], "--listen") == 0) {
            if (i + 1 >= argc || is_flag(argv[i + 1])) return usage();
            listen_value = argv[++i];
        } else if (std::strcmp(argv[i], "--host") == 0) {
            host_mode = true;
        } else {
            return usage();
        }
    }

    if (peer_value) {
        SetEnvironmentVariableA("TH08_PLATFORM_PEER", peer_value);
    }
    if (host_mode) {
        SetEnvironmentVariableA("TH08_PLATFORM_HOST", "1");
    }
    if (listen_value) {
        SetEnvironmentVariableA("TH08_PLATFORM_LISTEN", listen_value);
    } else if (host_mode) {
        SetEnvironmentVariableA("TH08_PLATFORM_LISTEN", "7480");
    } else if (peer_value) {
        // Peer mode without an explicit --listen: default to peer_port+1 so
        // host (peer_port) and peer (peer_port+1) don't collide on same machine.
        const char* colon = std::strrchr(peer_value, ':');
        char fallback[16] = "7481";
        if (colon) {
            const long pp = std::strtol(colon + 1, nullptr, 10);
            if (pp > 0 && pp < 65535) {
                std::snprintf(fallback, sizeof(fallback), "%ld", pp + 1);
            }
        }
        SetEnvironmentVariableA("TH08_PLATFORM_LISTEN", fallback);
    }

    // Phase 7 (RUEEE-style co-op): Phase 5 in-process 2P hooks must be
    // ON in net mode. They give us g_Player2, dual_collision, item_routing,
    // p2_lives, hud, p2_mirror -- the equivalent of what RUEEE adds at
    // the decomp level. The previous policy auto-disabled them under the
    // assumption that they cost ~30 fps; that cost was actually the
    // rollback per-frame capture (~40 ms/frame walking ~19 MB of state),
    // which was bypassed in `eb7f311`. With rollback off, Phase 5 is
    // expected to fit in the 16.6 ms frame budget. To force Phase 5 off,
    // set TH08_PLATFORM_DISABLE_MULTIPLAYER=1 in the parent shell.
    //
    // Also: in net mode the P2 input source defaults to "network" so
    // p2_input.cpp pulls peer's keyboard bits via lockstep instead of
    // the local user's keyboard. Override with
    // TH08_PLATFORM_P2_INPUT_MODE=stationary/mirror/demo/network if you
    // want different.
    if (host_mode || peer_value) {
        char existing_mode[16] = {};
        if (GetEnvironmentVariableA("TH08_PLATFORM_P2_INPUT_MODE", existing_mode,
                                    sizeof(existing_mode)) == 0) {
            SetEnvironmentVariableA("TH08_PLATFORM_P2_INPUT_MODE", "network");
        }
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::vector<char> cmdline(target.begin(), target.end());
    cmdline.push_back('\0');

    std::filesystem::path game_dir = std::filesystem::path(target).parent_path();

    if (!CreateProcessA(target.c_str(), cmdline.data(), nullptr, nullptr, FALSE,
                         CREATE_SUSPENDED, nullptr, game_dir.string().c_str(),
                         &si, &pi)) {
        std::fprintf(stderr, "CreateProcessA failed: %lu\n", GetLastError());
        return 1;
    }

    std::printf("launched %s pid=%lu (suspended), injecting %s\n",
                target.c_str(), pi.dwProcessId, dll.c_str());

    if (!inject_dll(pi.hProcess, dll)) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }

    ResumeThread(pi.hThread);
    std::printf("thread resumed, game running\n");

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
