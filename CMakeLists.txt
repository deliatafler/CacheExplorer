cmake_minimum_required(VERSION 3.22)

project(CacheExplorer VERSION 0.1.0 LANGUAGES CXX)

set(CACHEEXPLORER_DISPLAY_VERSION
    "0.1.0-beta"
    CACHE STRING
    "User-facing CacheExplorer version string")

option(CACHEEXPLORER_BUILD_QT_GUI
    "Build the primary Qt 6 cross-platform GUI"
    OFF)

option(CACHEEXPLORER_BUILD_TESTS
    "Build CacheExplorer regression tests"
    ON)

option(CACHEEXPLORER_STATIC_MSVC_RUNTIME
    "Use the static MSVC runtime (/MT). Turn this off for official prebuilt Qt SDKs."
    ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CACHEEXPLORER_BUILD_TESTS)
    enable_testing()
endif()

# Statically link the Microsoft C/C++ runtime by default.
# Release uses /MT; Debug uses /MTd. Official shared Qt SDKs use /MD, so the
# prebuilt-Qt helper disables this option for that build path.
if(MSVC AND CACHEEXPLORER_STATIC_MSVC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(cachelib)
add_subdirectory(cachecli)

if(CACHEEXPLORER_BUILD_QT_GUI)
    message(STATUS
        "Primary Qt GUI enabled. Prefer an official/prebuilt Qt SDK via CMAKE_PREFIX_PATH or Qt6_DIR; "
        "use the vcpkg qt-gui feature only for optional static/reproducible Qt experiments.")
    add_subdirectory(cachegui)
endif()

if(CACHEEXPLORER_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(APPLE AND CACHEEXPLORER_BUILD_QT_GUI)
    set(cacheexplorer_macos_package_arch "${CMAKE_OSX_ARCHITECTURES}")
    if(NOT cacheexplorer_macos_package_arch)
        set(cacheexplorer_macos_package_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    string(REPLACE ";" "-" cacheexplorer_macos_package_arch
        "${cacheexplorer_macos_package_arch}")

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_NAME "CacheExplorer")
    set(CPACK_PACKAGE_VENDOR "CacheExplorer contributors")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "CacheExplorer-${CACHEEXPLORER_DISPLAY_VERSION}-macOS-${cacheexplorer_macos_package_arch}")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_DMG_VOLUME_NAME "CacheExplorer ${CACHEEXPLORER_DISPLAY_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
    include(CPack)
endif()
