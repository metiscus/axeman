cmake_minimum_required(VERSION 3.10)
project(Axeman)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure we're targeting Windows
if(NOT WIN32)
    message(FATAL_ERROR "Axeman is a Windows-only tool.")
endif()

add_executable(axeman src/main.cpp)

# Link necessary Windows libraries
target_link_libraries(axeman psapi)
