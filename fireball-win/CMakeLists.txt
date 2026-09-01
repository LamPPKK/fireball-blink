cmake_minimum_required(VERSION 3.16)
project(FireballWin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(FIREBALL_WIN_SOURCES
    src/domain_models.cpp
    src/search_engines.cpp
    src/shields_engine.cpp
    src/password_vault.cpp
    src/sync_engine.cpp
    src/beam_client.cpp
    src/webview2_host.cpp
    src/app_window.cpp
)

# Test Suite Executable
add_executable(fireball_win_tests
    ${FIREBALL_WIN_SOURCES}
    tests/test_runner.cpp
)

if (WIN32)
    # Windows native executable
    add_executable(fireball_win WIN32
        ${FIREBALL_WIN_SOURCES}
        src/main_win.cpp
    )
    target_link_libraries(fireball_win PRIVATE d2d1 dwrite windowscodecs crypt32 shlwapi)
endif()
