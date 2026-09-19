cmake_minimum_required(VERSION 3.16)
project(dock-lyrics VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(DDEShell REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Network DBus)

add_library(dock-lyrics SHARED
    src/lyricsapplet.h
    src/lyricsapplet.cpp
    src/playerprobe.h
    src/playerprobe.cpp
    src/lyricsfetcher.h
    src/lyricsfetcher.cpp
    src/lrcparser.h
    src/lrcparser.cpp
)

target_include_directories(dock-lyrics PRIVATE src)
target_link_libraries(dock-lyrics PRIVATE
    Dde::Shell
    Qt6::Network
    Qt6::DBus
)

ds_install_package(PACKAGE com.github.dock-lyrics TARGET dock-lyrics)
