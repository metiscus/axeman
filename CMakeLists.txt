cmake_minimum_required(VERSION 3.10)
project(Axeman)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure we're targeting Windows
if(NOT WIN32)
    message(FATAL_ERROR "Axeman is a Windows-only tool.")
endif()

# WIN32 tells CMake to build a GUI app (WinMain) instead of Console (main)
add_executable(axeman WIN32 src/main.cpp)

# Link necessary Windows libraries
# user32: Window management
# shell32: System tray icons
# psapi: Memory info
# advapi32: Process tokens/security
# gdiplus: PNG icon loading
target_link_libraries(axeman psapi user32 shell32 advapi32 gdiplus)