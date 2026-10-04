cmake_minimum_required(VERSION 3.16)
project(griddick_host C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(GT_VERSION "0.9.7")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/griddick" CACHE PATH "" FORCE)
endif()
# One RPATH for in-tree host/bin and /opt/griddick/bin. Do not embed the
# build tree path (Fedora rpm check-rpaths rejects it).
set(GT_RPATH "\$ORIGIN/../lib")
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)
set(CMAKE_SKIP_BUILD_RPATH OFF)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")

set(KISS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../kiss")

add_library(gtnc SHARED
    lib/src/gt_ident.c
    lib/src/gt_tty.c
    lib/src/gt_kiss.c
    lib/src/gt_util.c
    lib/src/gt_ax25.c
    lib/src/gt_aprs.c
    lib/src/gt_time.c
    lib/src/gt_cfg.c
    lib/src/gt_pcm.c
    lib/src/gt_link.c
    "${KISS_DIR}/kiss.c"
)
# DT_SONAME must be "libgtnc.so". NO_SONAME made ld record the build-tree
# path in DT_NEEDED (gtstation then looks under ~/proj/.../host/lib).
set_target_properties(gtnc PROPERTIES
    OUTPUT_NAME gtnc
    NO_SONAME OFF
)
target_include_directories(gtnc
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/include"
        "${KISS_DIR}"
)
target_compile_options(gtnc PRIVATE -Wall -Wextra)
target_compile_definitions(gtnc PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)

foreach(_t gtlog gttime gttx gtwav gtfeed gtcfg gtdump gtcall)
    add_executable(${_t} cli/${_t}.c)
    target_link_libraries(${_t} PRIVATE gtnc)
    target_compile_options(${_t} PRIVATE -Wall -Wextra)
    set_target_properties(${_t} PROPERTIES
        BUILD_RPATH "${GT_RPATH}"
        INSTALL_RPATH "${GT_RPATH}"
    )
endforeach()

add_executable(griddickd
    griddickd/griddickd.c
    griddickd/gd_conf.c
    griddickd/gd_wire.c
    griddickd/gd_tun.c
)
target_include_directories(griddickd PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/griddickd")
target_link_libraries(griddickd PRIVATE gtnc crypto)
target_compile_options(griddickd PRIVATE -Wall -Wextra)
target_compile_definitions(griddickd PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
set_target_properties(griddickd PROPERTIES
    BUILD_RPATH "${GT_RPATH}"
    INSTALL_RPATH "${GT_RPATH}"
)


find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    add_subdirectory(gui)
else()
    message(STATUS "Qt6 Widgets not found; skipping gtstation")
endif()

install(TARGETS gtnc
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(TARGETS
    gtlog gttime gttx gtwav gtfeed gtcfg gtdump gtcall griddickd
    RUNTIME DESTINATION bin
)
if(TARGET gtstation)
    install(TARGETS gtstation RUNTIME DESTINATION bin)
endif()
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/griddickd/scripts/griddickd.conf.example"
    DESTINATION "."
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/service/griddickd.service"
    DESTINATION "/etc/systemd/system"
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/service/griddick.sh"
    DESTINATION "/etc/profile.d"
)
