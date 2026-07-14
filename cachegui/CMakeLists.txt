add_executable(cachegui WIN32
    GuiUtils.cpp
    main.cpp
)

target_compile_features(cachegui
    PRIVATE
        cxx_std_17
)

target_link_libraries(cachegui
    PRIVATE
        cachelib
        comctl32
        gdiplus
        ole32
        shell32
        uuid
)

target_compile_definitions(cachegui
    PRIVATE
        UNICODE
        _UNICODE
)

if(MSVC)
    target_compile_options(cachegui
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(cachegui
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()
