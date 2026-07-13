add_library(cachelib
    UUID.cpp
    TextureCacheDatabase.cpp
    TextureRebuilder.cpp
)

target_include_directories(cachelib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(cachelib
    PUBLIC
        cxx_std_17
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
