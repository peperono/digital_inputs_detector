cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(digital_inputs_detector LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── QP/C++ port: win32 + kernel qv = win32-qv ────────────────────────────────
set(QPCPP_CFG_PORT   win32 CACHE STRING "" FORCE)
set(QPCPP_CFG_KERNEL qv    CACHE STRING "" FORCE)

# Usar qp_config.hpp del directorio raíz del proyecto
set(QPCPP_CFG_QPCONFIG_H_INCLUDE_PATH ${CMAKE_SOURCE_DIR} CACHE STRING "" FORCE)

add_subdirectory(qpcpp)

# ── nlohmann/json (header-only, descargado en configure-time) ─────────────────
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Fuentes compartidas entre app y test ──────────────────────────────────────
set(SHARED_SOURCES
    IOStateMonitor/IOStateMonitor.cpp
    DigitalEdgeDetector/DigitalEdgeDetector.cpp
    Control/Control.cpp
)

# ── Ejecutable principal ──────────────────────────────────────────────────────
add_executable(app
    main.cpp
    ${SHARED_SOURCES}
)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(app PRIVATE qpcpp)

# ── Ejecutable de test de integración ────────────────────────────────────────
add_executable(test_system
    test_system.cpp
    ${SHARED_SOURCES}
)
target_include_directories(test_system PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_system PRIVATE qpcpp nlohmann_json::nlohmann_json)
