find_package(OpenJPEG CONFIG REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(PNG REQUIRED)

add_library(cachelib
    AuroraCacheFormat.cpp
    AuroraCrypto.cpp
    AuroraCredential.cpp
    AuroraCacheKey.cpp
    AuroraCacheProbe.cpp
    AuroraSealedFile.cpp
    AuroraTextureCacheSource.cpp
    Bc7Decoder.cpp
    UUID.cpp
    LegacyTextureCacheSource.cpp
    TextureCacheSource.cpp
    TextureCacheDatabase.cpp
    TextureCacheChanges.cpp
    TextureDecoder.cpp
    TextureExportState.cpp
    TextureRebuilder.cpp
    TextureExporter.cpp
    TextureSelection.cpp
    ViewerTextureCacheSource.cpp
    J2CDecoder.cpp
    PngWriter.cpp
)

if(WIN32)
    target_sources(cachelib PRIVATE AuroraCredentialWindows.cpp)
elseif(APPLE)
    target_sources(cachelib PRIVATE AuroraCredentialMac.cpp)
elseif(UNIX)
    target_sources(cachelib PRIVATE AuroraCredentialLinux.cpp)
else()
    target_sources(cachelib PRIVATE AuroraCredentialUnsupported.cpp)
endif()

target_include_directories(cachelib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    SYSTEM PRIVATE
        ${PROJECT_SOURCE_DIR}/third_party/bcdec
)

target_compile_features(cachelib
    PUBLIC
        cxx_std_17
)

target_link_libraries(cachelib
    PRIVATE
        openjp2
        OpenSSL::Crypto
        PNG::PNG
)

if(WIN32)
    target_link_libraries(cachelib
        PRIVATE
            Advapi32
    )
endif()

if(APPLE)
    target_link_libraries(cachelib
        PRIVATE
            "-framework CoreFoundation"
            "-framework Security"
    )
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LibSecret REQUIRED IMPORTED_TARGET libsecret-1)
    target_link_libraries(cachelib PRIVATE PkgConfig::LibSecret)
endif()

if(MSVC)
    target_compile_options(cachelib
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(cachelib
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()
