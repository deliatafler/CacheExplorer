add_executable(cachelib_tests
    cachelib_tests.cpp
)

target_link_libraries(cachelib_tests
    PRIVATE
        cachelib
)

target_compile_features(cachelib_tests
    PRIVATE
        cxx_std_17
)

if(MSVC)
    target_compile_options(cachelib_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(cachelib_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME cachelib_tests
    COMMAND cachelib_tests
)
