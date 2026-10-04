cmake_minimum_required(VERSION 3.16)

# Host unit test for Pico HDLC/FCS (C99).
# Standalone: ./modem/build.sh
#
# Layout:
#   include/ constants/types pulled by ax25_internal.h
#   src/     ax25_hdlc.c, ax25_fcs.c, ax25_fix.c
#   tests/   test_modem_ax25_hdlc, test_modem_ax25_fix
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(modem_ax25_tests C)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    option(MODEM_BUILD_TESTS "Build modem unit tests" ON)
endif()

if(NOT TARGET pixdsp)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../dsp"
        "${CMAKE_BINARY_DIR}/dsp"
    )
endif()

if(MODEM_BUILD_TESTS OR (NOT DEFINED MODEM_BUILD_TESTS))
    if(NOT TARGET test_modem_ax25_hdlc)
        add_executable(test_modem_ax25_hdlc
            tests/test_modem_ax25_hdlc.c
            src/ax25_hdlc.c
            src/ax25_fcs.c
        )
        target_include_directories(test_modem_ax25_hdlc PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(test_modem_ax25_hdlc PRIVATE pixdsp m)
        target_compile_options(test_modem_ax25_hdlc PRIVATE -Wall -Wextra)

        add_executable(test_modem_ax25_fix
            tests/test_modem_ax25_fix.c
            src/ax25_hdlc.c
            src/ax25_fcs.c
            src/ax25_fix.c
        )
        target_include_directories(test_modem_ax25_fix PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(test_modem_ax25_fix PRIVATE pixdsp m)
        target_compile_options(test_modem_ax25_fix PRIVATE -Wall -Wextra)

        if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
            set_target_properties(test_modem_ax25_hdlc PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
            )
            set_target_properties(test_modem_ax25_fix PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
            )
        endif()

        enable_testing()
        add_test(NAME test_modem_ax25_hdlc COMMAND test_modem_ax25_hdlc)
        add_test(NAME test_modem_ax25_fix COMMAND test_modem_ax25_fix)
        add_test(NAME test_modem_ax25_fix_on COMMAND test_modem_ax25_fix --fix)
        add_test(NAME test_modem_ax25_fix_off COMMAND test_modem_ax25_fix --no-fix)
    endif()
endif()
