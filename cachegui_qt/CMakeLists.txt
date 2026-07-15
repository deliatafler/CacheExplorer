find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(cachegui_qt WIN32
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

set_target_properties(cachegui_qt
    PROPERTIES
        OUTPUT_NAME CacheExplorer
)

target_compile_features(cachegui_qt
    PRIVATE
        cxx_std_17
)

target_compile_definitions(cachegui_qt
    PRIVATE
        CACHEEXPLORER_VERSION="${CACHEEXPLORER_DISPLAY_VERSION}"
)

target_link_libraries(cachegui_qt
    PRIVATE
        cachelib
        Qt6::Widgets
)

if(MSVC)
    target_compile_options(cachegui_qt
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(cachegui_qt
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(cachegui_qt)
endif()
