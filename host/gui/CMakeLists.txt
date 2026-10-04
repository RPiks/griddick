# Griddick Station (gtstation) — Qt6 host GUI.
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(gtstation
    main.cpp
    MainWindow.cpp
    MainWindow.h
    IoThread.cpp
    IoThread.h
    LevelBar.cpp
    LevelBar.h
    Waterfall.cpp
    Waterfall.h
    fft_r2.cpp
    fft_r2.h
)

target_link_libraries(gtstation PRIVATE Qt6::Widgets gtnc)
target_compile_options(gtstation PRIVATE -Wall -Wextra)
target_compile_definitions(gtstation PRIVATE
    GT_VERSION="${GT_VERSION}"
    _POSIX_C_SOURCE=200809L
    _DEFAULT_SOURCE
)

set_target_properties(gtstation PROPERTIES
    BUILD_RPATH "${GT_RPATH}"
    INSTALL_RPATH "${GT_RPATH}"
)
