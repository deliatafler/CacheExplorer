find_package(OpenJPEG CONFIG REQUIRED)
find_package(PNG REQUIRED)

add_library(cachelib
    UUID.cpp
    TextureCacheDatabase.cpp
    TextureRebuilder.cpp
    TextureExporter.cpp
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
        PNG::PNG
)

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
