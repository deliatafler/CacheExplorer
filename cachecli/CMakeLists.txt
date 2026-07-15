add_executable(cachecli
    main.cpp
)

target_compile_definitions(cachecli
    PRIVATE
        CACHEEXPLORER_VERSION="${CACHEEXPLORER_DISPLAY_VERSION}"
)

target_link_libraries(cachecli PRIVATE cachelib)
