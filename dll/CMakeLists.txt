cmake_minimum_required(VERSION 3.20)
project(th08_platform LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "th08_platform requires Windows")
endif()

# th08.exe is 32-bit — our DLL must also be 32-bit to share its address space.
# Configure via: cmake -B build -A Win32
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Must build as 32-bit (Win32). Re-run: cmake -B build -A Win32")
endif()

include(FetchContent)

# MinHook — x86 inline hooking library
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.3
)
FetchContent_MakeAvailable(minhook)

# MinHook v1.3.3 ships no CMakeLists, so define the static lib ourselves
add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)

# ==== th08_platform.dll ====
add_library(th08_platform SHARED
    src/main.cpp
    src/logging.cpp
    src/hooks/game_loop.cpp
    src/hooks/input.cpp
    src/hooks/instance_unmutex.cpp
    src/net/udp_socket.cpp
    src/net/lockstep.cpp
    src/net/pump_thread.cpp
    src/net/fake_rtt.cpp
    src/net/rollback.cpp
    src/state/frame_state.cpp
    src/state/heap_journal.cpp
    src/state/player2.cpp
    src/state/player2_hook.cpp
    src/state/dual_collision.cpp
    src/state/enemy_aim.cpp
    src/state/item_routing.cpp
    src/state/p2_input.cpp
    src/state/p2_lives.cpp
    src/state/hud.cpp
    src/state/p2_mirror.cpp
)
target_include_directories(th08_platform PRIVATE
    src
    ${minhook_SOURCE_DIR}/include
)
target_link_libraries(th08_platform PRIVATE minhook ws2_32)
target_compile_definitions(th08_platform PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)
target_compile_options(th08_platform PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)
set_target_properties(th08_platform PROPERTIES
    OUTPUT_NAME "th08_platform"
    PREFIX ""
)

# ==== th08_platform_loader.exe ====
add_executable(loader loader/loader.cpp)
set_target_properties(loader PROPERTIES OUTPUT_NAME "th08_platform_loader")
target_compile_definitions(loader PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# Put DLL + loader side-by-side in the same bin dir so loader can resolve
# th08_platform.dll from its own directory.
set_target_properties(th08_platform loader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
