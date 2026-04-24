cmake_minimum_required(VERSION 3.20)
project(th08_platform LANGUAGES CXX)

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

# ==== th08_platform.dll ====
add_library(th08_platform SHARED
    src/main.cpp
    src/logging.cpp
    src/hooks/game_loop.cpp
)
target_include_directories(th08_platform PRIVATE src)
target_link_libraries(th08_platform PRIVATE minhook)
target_compile_definitions(th08_platform PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
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
