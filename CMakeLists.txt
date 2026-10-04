cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake")

project(griddick C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Pico UF2: pixdsp + app.
set(PIXDSP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(dsp)
target_compile_definitions(pixdsp PRIVATE PICO_ON_DEVICE=1)
add_subdirectory(app)
