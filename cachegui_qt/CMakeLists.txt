find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(cachegui_qt WIN32
    CacheEntryTableModel.cpp
    CacheEntryTableModel.h
    GalleryListView.cpp
    GalleryListView.h
    main.cpp
    PreviewCache.cpp
    PreviewCache.h
    PreviewDecodeWorker.cpp
    PreviewDecodeWorker.h
    PreviewImage.cpp
    PreviewImage.h
)

target_compile_features(cachegui_qt
    PRIVATE
        cxx_std_17
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
