find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(cachegui WIN32 MACOSX_BUNDLE
    CacheEntryTableModel.cpp
    CacheEntryTableModel.h
    GalleryFilterProxyModel.cpp
    GalleryFilterProxyModel.h
    GalleryActivityIndicator.cpp
    GalleryActivityIndicator.h
    GalleryListView.cpp
    GalleryListView.h
    GalleryPreviewController.cpp
    GalleryPreviewController.h
    GalleryPreviewQueue.cpp
    GalleryPreviewQueue.h
    GalleryPreviewScanner.cpp
    GalleryPreviewScanner.h
    main.cpp
    PreviewCache.cpp
    PreviewCache.h
    PreviewDecodeWorker.cpp
    PreviewDecodeWorker.h
    PreviewImage.cpp
    PreviewImage.h
    PreviewPanel.cpp
    PreviewPanel.h
    PreviewStatus.cpp
    PreviewStatus.h
    PreviewWorkerState.cpp
    PreviewWorkerState.h
    QtAboutDialog.cpp
    QtAboutDialog.h
    QtActionState.cpp
    QtActionState.h
    QtFileDialogs.cpp
    QtFileDialogs.h
    QtGallerySort.cpp
    QtGallerySort.h
    QtGalleryStatus.cpp
    QtGalleryStatus.h
    QtHelpers.cpp
    QtHelpers.h
    QtPreviewStateStore.cpp
    QtPreviewStateStore.h
    QtSelection.cpp
    QtSelection.h
    QtTextureExport.cpp
    QtTextureExport.h
    QtTryNextPreview.cpp
    QtTryNextPreview.h
    QtViewMode.cpp
    QtViewMode.h
    TryNextPreviewState.cpp
    TryNextPreviewState.h
)

if(WIN32)
    target_sources(cachegui
        PRIVATE
            resources/cacheexplorer.rc
    )
endif()

if(APPLE)
    set(cacheexplorer_macos_icon
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/cacheexplorer.icns")
    if(NOT EXISTS "${cacheexplorer_macos_icon}")
        message(FATAL_ERROR
            "Missing generated macOS icon: ${cacheexplorer_macos_icon}. "
            "Run scripts/generate-macos-icon.sh before configuring.")
    endif()

    set_source_files_properties("${cacheexplorer_macos_icon}"
        PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(cachegui PRIVATE "${cacheexplorer_macos_icon}")
endif()

qt_add_resources(cachegui "cacheexplorer_resources"
    PREFIX "/"
    FILES
        resources/cacheexplorer.png
)

set_target_properties(cachegui
    PROPERTIES
        OUTPUT_NAME CacheExplorer
)

if(APPLE)
    set_target_properties(cachegui
        PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "CacheExplorer"
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.deliatafler.CacheExplorer"
            MACOSX_BUNDLE_ICON_FILE "cacheexplorer.icns"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

target_compile_features(cachegui
    PRIVATE
        cxx_std_17
)

target_compile_definitions(cachegui
    PRIVATE
        CACHEEXPLORER_VERSION="${CACHEEXPLORER_DISPLAY_VERSION}"
)

target_link_libraries(cachegui
    PRIVATE
        cachelib
        Qt6::Widgets
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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(cachegui)
endif()

if(APPLE)
    install(TARGETS cachegui BUNDLE DESTINATION .)

    qt_generate_deploy_app_script(
        TARGET cachegui
        OUTPUT_SCRIPT cacheexplorer_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT "${cacheexplorer_deploy_script}")

    install(FILES
        "${PROJECT_SOURCE_DIR}/README.md"
        "${PROJECT_SOURCE_DIR}/RELEASE_NOTES.md"
        "${PROJECT_SOURCE_DIR}/LICENSE"
        DESTINATION .
    )
    install(FILES
        "${PROJECT_SOURCE_DIR}/docs/qt-user-guide.md"
        DESTINATION docs
    )
endif()
