cmake_minimum_required(VERSION 3.22)

project(CacheExplorer VERSION 0.1.0 LANGUAGES CXX)

option(CACHEEXPLORER_BUILD_QT_GUI
    "Build the primary Qt 6 cross-platform GUI"
    OFF)

option(CACHEEXPLORER_BUILD_LEGACY_WIN32_GUI
    "Build the legacy native Win32 GUI"
    ON)

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

if(CACHEEXPLORER_BUILD_LEGACY_WIN32_GUI)
    add_subdirectory(cachegui)
endif()

if(CACHEEXPLORER_BUILD_QT_GUI)
    message(STATUS
        "Primary Qt GUI enabled. Use CMAKE_PREFIX_PATH or Qt6_DIR for a prebuilt Qt developer build; "
        "use the vcpkg qt-gui feature for a static/reproducible Qt build.")
    add_subdirectory(cachegui_qt)
endif()
