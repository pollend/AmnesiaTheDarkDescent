cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 17)

# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif ()

include(SpirvTools.cmake)
include(Dependencies.cmake)
include(OS.cmake)
include(Graphics.cmake)
include(TheForge.cmake)