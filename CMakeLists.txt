cmake_minimum_required(VERSION 3.22)

project(CacheExplorer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Statically link the Microsoft C/C++ runtime.
# Release uses /MT; Debug uses /MTd.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(cachelib)
add_subdirectory(cachecli)
add_subdirectory(cachegui)
