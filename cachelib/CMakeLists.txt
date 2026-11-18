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
    UUID.cpp
    LegacyTextureCacheSource.cpp
    TextureCacheSource.cpp
    TextureCacheDatabase.cpp
    TextureCacheChanges.cpp
    TextureExportState.cpp
    TextureRebuilder.cpp
    TextureExporter.cpp
    TextureSelection.cpp
    ViewerTextureCacheSource.cpp
    J2CDecoder.cpp
    PngWriter.cpp
)

target_include_directories(cachelib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
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
