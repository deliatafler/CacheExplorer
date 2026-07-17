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

if(CACHEEXPLORER_BUILD_QT_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    add_executable(cachegui_tests
        ${PROJECT_SOURCE_DIR}/cachegui/GalleryFilterProxyModel.cpp
        ${PROJECT_SOURCE_DIR}/cachegui/GalleryFilterProxyModel.h
        ${PROJECT_SOURCE_DIR}/cachegui/GalleryPreviewQueue.cpp
        ${PROJECT_SOURCE_DIR}/cachegui/GalleryPreviewQueue.h
        ${PROJECT_SOURCE_DIR}/cachegui/QtGalleryStatus.cpp
        ${PROJECT_SOURCE_DIR}/cachegui/QtGalleryStatus.h
        ${PROJECT_SOURCE_DIR}/cachegui/QtHelpers.cpp
        ${PROJECT_SOURCE_DIR}/cachegui/QtHelpers.h
        ${PROJECT_SOURCE_DIR}/cachegui/QtTextureDetails.cpp
        ${PROJECT_SOURCE_DIR}/cachegui/QtTextureDetails.h
        ${PROJECT_SOURCE_DIR}/cachegui/TryNextPreviewState.cpp
        ${PROJECT_SOURCE_DIR}/cachegui/TryNextPreviewState.h
        cachegui_tests.cpp
    )

    target_include_directories(cachegui_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/cachegui
    )

    target_link_libraries(cachegui_tests
        PRIVATE
            cachelib
            Qt6::Widgets
    )

    target_compile_features(cachegui_tests
        PRIVATE
            cxx_std_17
    )

    if(MSVC)
        target_compile_options(cachegui_tests
            PRIVATE
                /W4
                /permissive-
        )
    else()
        target_compile_options(cachegui_tests
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
        )
    endif()

    add_test(
        NAME cachegui_tests
        COMMAND cachegui_tests
    )

    # Prebuilt Qt uses shared runtime DLLs. Let CTest locate Qt6Core.dll
    # without requiring callers to amend PATH before running this target.
    set_tests_properties(cachegui_tests
        PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
