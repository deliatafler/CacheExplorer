cmake_minimum_required(VERSION 3.22)

project(CacheExplorer VERSION 0.1.0 LANGUAGES CXX)

set(CACHEEXPLORER_DISPLAY_VERSION "0.1.0-beta.3")

set(CACHEEXPLORER_LINUX_PACKAGE_TAG
    "Linux"
    CACHE STRING
    "Distribution label used in Linux package filenames")

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

if(MSVC)
    add_compile_options(/EHsc)
endif()

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

if(CACHEEXPLORER_BUILD_QT_GUI)
    if(APPLE)
        set(cacheexplorer_macos_package_arch "${CMAKE_OSX_ARCHITECTURES}")
        if(NOT cacheexplorer_macos_package_arch)
            set(cacheexplorer_macos_package_arch "${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        string(REPLACE ";" "-" cacheexplorer_macos_package_arch
            "${cacheexplorer_macos_package_arch}")

        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_PACKAGE_FILE_NAME
            "CacheExplorer-${CACHEEXPLORER_DISPLAY_VERSION}-macOS-${cacheexplorer_macos_package_arch}")
        set(CPACK_DMG_VOLUME_NAME "CacheExplorer ${CACHEEXPLORER_DISPLAY_VERSION}")
        set(CPACK_DMG_FORMAT "UDZO")
    elseif(WIN32)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "bin")
        include(InstallRequiredSystemLibraries)

        set(CPACK_GENERATOR "NSIS")
        set(CPACK_PACKAGE_FILE_NAME
            "CacheExplorer-${CACHEEXPLORER_DISPLAY_VERSION}-Windows-x64-Setup")
        set(CPACK_PACKAGE_INSTALL_DIRECTORY "CacheExplorer")
        set(CPACK_PACKAGE_EXECUTABLES "CacheExplorer" "CacheExplorer")
        set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
        set(CPACK_NSIS_DISPLAY_NAME "CacheExplorer")
        set(CPACK_NSIS_PACKAGE_NAME
            "CacheExplorer ${CACHEEXPLORER_DISPLAY_VERSION}")
        set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\CacheExplorer.exe")
        set(CPACK_NSIS_MUI_ICON
            "${PROJECT_SOURCE_DIR}/cachegui/resources/cacheexplorer.ico")
        set(CPACK_NSIS_MUI_UNIICON
            "${PROJECT_SOURCE_DIR}/cachegui/resources/cacheexplorer.ico")
        set(CPACK_NSIS_HELP_LINK
            "https://github.com/deliatafler/CacheExplorer/issues")
        set(CPACK_NSIS_URL_INFO_ABOUT
            "https://github.com/deliatafler/CacheExplorer")
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_PACKAGE_CHECKSUM "SHA256")
        set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    elseif(UNIX)
        set(CPACK_GENERATOR "DEB")
        set(CPACK_PACKAGE_FILE_NAME
            "CacheExplorer-${CACHEEXPLORER_DISPLAY_VERSION}-${CACHEEXPLORER_LINUX_PACKAGE_TAG}-${CMAKE_SYSTEM_PROCESSOR}")
        set(CPACK_DEBIAN_PACKAGE_NAME "cacheexplorer")
        string(REPLACE "-" "~" cacheexplorer_debian_package_version
            "${CACHEEXPLORER_DISPLAY_VERSION}")
        set(CPACK_DEBIAN_PACKAGE_VERSION
            "${cacheexplorer_debian_package_version}")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "CacheExplorer contributors")
        set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")
    endif()

    if(APPLE OR WIN32 OR UNIX)
        set(CPACK_PACKAGE_NAME "CacheExplorer")
        set(CPACK_PACKAGE_VENDOR "CacheExplorer contributors")
        set(CPACK_PACKAGE_CONTACT
            "https://github.com/deliatafler/CacheExplorer/issues")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
            "Browser and PNG exporter for Second Life viewer texture caches")
        set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
        include(CPack)
    endif()
endif()
