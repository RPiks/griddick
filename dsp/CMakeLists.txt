cmake_minimum_required(VERSION 3.16)

# When built via top-level umbrella, project() is already set.
if(NOT TARGET pixdsp)
    # Standalone: cmake -S dsp -B dsp/build
    project(pixdsp C)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

# Product tract: IFIR, DDS, hypot, AGC, DPLL.
# polydec + hessian baud come from prebuilt/pico/libpixdsp_closed.a
set(PIXDSP_SOURCES
    src/pixdsp_agc_i16.c
    src/pixdsp_dds_i32.c
    src/pixdsp_hypot_i32.c
    src/pixdsp_i32.c
    src/pixdsp_dpll.c
    src/pixdsp_ifir_i32.c
)

set(PIXDSP_CLOSED "${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/pico/libpixdsp_closed.a")
if(NOT EXISTS "${PIXDSP_CLOSED}")
    message(FATAL_ERROR
        "Missing ${PIXDSP_CLOSED}. Build it with dsp/mk-closed-a.sh "
        "(needs dsp/private sources on the build machine).")
endif()

option(PIXDSP_BUILD_SHARED "Build pixdsp as a shared library" OFF)

if(PIXDSP_BUILD_SHARED)
    add_library(pixdsp SHARED ${PIXDSP_SOURCES})
else()
    add_library(pixdsp STATIC ${PIXDSP_SOURCES})
endif()
target_include_directories(pixdsp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(pixdsp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(pixdsp PUBLIC m "${PIXDSP_CLOSED}")
target_compile_options(pixdsp PRIVATE -Wall -Wextra)
set_target_properties(pixdsp PROPERTIES
    OUTPUT_NAME "pixdsp"
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
