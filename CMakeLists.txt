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

# ── Ejecutable ────────────────────────────────────────────────────────────────
add_executable(app
    main.cpp
    IOStateMonitor/IOStateMonitor.cpp
    DigitalEdgeDetector/DigitalEdgeDetector.cpp
    Control/Control.cpp
)

target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(app PRIVATE qpcpp)
