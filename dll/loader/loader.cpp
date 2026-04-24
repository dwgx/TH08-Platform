// th08_platform_loader.exe
//
// Launches th08.exe and injects th08_platform.dll via the classic
// CreateRemoteThread + LoadLibraryA pattern. The loader is deliberately
// minimal — everything interesting lives inside the DLL.
//
// Usage: th08_platform_loader.exe <path_to_th08.exe> [--peer <ip:port>]

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
        std::fprintf(stderr, "LoadLibraryA returned 0 — DLL failed to load\n");
        return false;
    }
    std::printf("LoadLibraryA returned module handle 0x%lx\n", exit_code);
    return true;
}

int usage()
{
    std::fprintf(stderr,
        "usage: th08_platform_loader.exe <path_to_th08.exe> [--peer ip:port]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) return usage();

    const std::string target = argv[1];
    const std::string dll = resolve_dll_path();

    if (!std::filesystem::exists(target)) {
        std::fprintf(stderr, "target not found: %s\n", target.c_str());
        return 1;
    }
    if (!std::filesystem::exists(dll)) {
        std::fprintf(stderr, "th08_platform.dll not found next to loader at: %s\n",
                     dll.c_str());
        return 1;
    }

    // Optional --peer is stashed in an env var the DLL will pick up in Phase 3+.
    for (int i = 2; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--peer") == 0) {
            SetEnvironmentVariableA("TH08_PLATFORM_PEER", argv[i + 1]);
            ++i;
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
