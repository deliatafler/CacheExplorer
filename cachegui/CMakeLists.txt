find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(cachegui WIN32
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

set_target_properties(cachegui
    PROPERTIES
        OUTPUT_NAME CacheExplorer
)

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
