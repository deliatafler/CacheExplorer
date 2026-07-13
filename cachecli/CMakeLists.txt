add_executable(cachecli
    main.cpp
)

target_link_libraries(cachecli PRIVATE cachelib)
