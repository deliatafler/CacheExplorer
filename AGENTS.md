# CacheExplorer Development Notes

## Project purpose

CacheExplorer is a standalone, open-source browser and extractor for the Firestorm Viewer texture cache. It is intended as a modern replacement for SLCacheViewer.

The project must not link against or depend on Firestorm itself. Firestorm source code was used only to understand the cache format.

## Architecture

The project has three current targets:

* `cachelib`: reusable cache-reading, reconstruction, decoding, and export library
* `cachecli`: thin command-line frontend
* `cachegui_qt`: Qt 6 cross-platform GUI frontend

The GUI depends directly on `cachelib`. It must not wrap or invoke the CLI.

Business logic belongs in `cachelib`. CLI-specific argument parsing and console presentation belong in `cachecli`.

The project direction is Qt 6 for GUI development, beta testing, and
cross-platform support. The native Win32 GUI prototype has been removed; useful
follow-up ideas from it are parked in `docs/post-beta-roadmap.md`.

## Platform and build

Primary platform:

* Windows 11
* Visual Studio/MSVC
* C++17
* CMake
* Git Bash
* vcpkg manifest mode
* static MSVC runtime
* `x64-windows-static` vcpkg triplet

Dependencies:

* OpenJPEG
* libpng

Clean build from Git Bash:

```bash
rm -rf build

cmake -S . -B build -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
```

Executable:

```text
build/cachecli/Release/cachecli.exe
```

See `docs/qt-build.md` for detailed Qt GUI build, prebuilt-Qt, static-Qt, and deployment notes. See `docs/qt-packaging.md` for the shared-Qt package helper and smoke-test checklist. See `docs/qt-user-guide.md` for user-facing Qt GUI usage notes, `docs/beta-release-checklist.md` before tagging or sharing a beta, and `RELEASE_NOTES.md` for draft release notes.

Primary Qt GUI build with a prebuilt shared Qt installation. This is the preferred developer path because it avoids rebuilding Qt locally:

```bash
cmake -S . -B build-qt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON

cmake --build build-qt --config Release --target cachegui_qt
```

The CMake target remains `cachegui_qt`; its user-facing executable name is
`CacheExplorer.exe`.

Primary Qt GUI build with vcpkg-provided static Qt. This is useful for reproducible builds and distribution experiments, but first-time dependency setup is slow:

```bash
cmake -S . -B build-qt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DVCPKG_MANIFEST_FEATURES=qt-gui \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON

cmake --build build-qt --config Release --target cachegui_qt
```

The `qt-gui` vcpkg feature requests a minimal target Qt Widgets set, but the first Windows static vcpkg configure can still take a long time because host-side Qt tools pull and build a broader dependency graph.

## Firestorm texture-cache format

The selected cache directory directly contains:

```text
texture.entries
texture.cache
FastCache.cache
0/
1/
...
f/
```

`texture.entries` starts with a packed 44-byte `EntriesInfo` structure, followed by packed 28-byte entry records.

Each entry contains:

* 16-byte UUID
* signed 32-bit image size
* signed 32-bit body size
* unsigned 32-bit Unix timestamp

Deleted or unused slots usually contain:

```text
imageSize = -1
bodySize = 0
```

Firestorm treats an entry as usable when:

```cpp
imageSize > bodySize
```

The raw entry index must be retained as `cacheIndex`. Do not replace it with the index in the filtered valid-entry vector.

The texture header offset is:

```cpp
cacheIndex * 600
```

Each `texture.cache` record is 600 bytes.

The number of meaningful bytes taken from the header record is:

```cpp
min(imageSize, 600)
```

A body file is stored at:

```text
<first UUID character>/<UUID>.texture
```

Only `bodySize` bytes are meaningful. Real caches sometimes contain oversized body files with stale trailing bytes. These are warnings, not corruption. Undersized body files are errors.

A reconstructed JPEG2000 codestream is:

```text
meaningful header bytes + exactly bodySize body bytes
```

## Implemented components

### UUID

Provides:

* binary storage
* canonical string formatting
* string parsing
* null check
* C++17-compatible equality operators

### TextureCacheDatabase

Reads `texture.entries`, exposes a clean `CacheHeader`, filters free slots, and preserves `cacheIndex`.

### TextureRebuilder

Reconstructs cached JPEG2000 data in memory or exports a `.j2c` file.

### J2CDecoder

Uses OpenJPEG to decode reconstructed J2C data into 8-bit interleaved RGBA.

Supports:

* grayscale
* grayscale plus alpha
* RGB
* RGBA

`Decode` accepts a verbose flag. In normal and bulk operation, OpenJPEG information, warning, and error output should be quiet. Single-image debugging may enable warning and error diagnostics.

Temporary decoder tracing such as `Decoder: calling opj_decode` should not remain.

### PngWriter

Uses libpng to write RGBA PNG files.

### TextureExporter

Bulk-export logic is being moved here from `cachecli/main.cpp`.

The library must not write directly to `std::cout` or `std::cerr`.

Progress must be reported through a callback supplied by the caller.

### TextureSelection

This is the next reusable selection abstraction. It should initially provide:

```cpp
All(database)
Range(database, start, count)
ByUuid(database, uuids)
```

The methods should return:

```cpp
std::vector<const CacheEntry*>
```

Pointers are valid only while the database remains open and its entries vector is not modified.

## Existing CLI commands

The CLI currently supports or is intended to support:

```text
scan
list
export
verify
stats
export-png
export-png-all
```

`verify` has been tested against a real Firestorm cache:

```text
16609 entries checked
16609 entries verified
0 problems
50 oversized-body warnings
```

`export-png` has successfully produced a valid 1024×1024 PNG.

Bulk PNG export works. Many entries are expected to be incomplete or undecodable because Firestorm uses progressive JPEG2000 caching. These should be counted as incomplete, not as program errors.

## Immediate work

Complete these tasks in order:

1. Familiarize yourself with the project and goals.  Ask any clarifying questions.

2. Propose next steps for continuing GUI development.

## Current GUI work

### Qt GUI

The Qt 6 GUI in `cachegui_qt` is the supported GUI path. It opens a cache
through `cachelib`, shows entries in a sortable model-backed table, can
preview/export a selected entry as PNG through `TextureExporter`, and tracks
preview status in the table.

The Qt GUI has an `About` diagnostics dialog for beta/support reports. Keep it
Qt-only; reusable cache facts should still come from `cachelib`.

Use `docs/qt-gui-validation.md` for the manual Qt GUI smoke/regression checklist. Build-only validation is acceptable for narrow helper moves; run the GUI checklist after changes to async preview flow, selection behavior, gallery loading, export, cache-open behavior, or rendering.

The Qt GUI also has an early Gallery/Table toggle. Gallery reuses the same sorted proxy model and preview cache as the table. Cached previews appear as icons after they have been decoded by Preview, Try Next Preview, or the gallery's lazy visible-item loader.

Gallery mode hides the manual `Preview` and `Try Next Preview` actions because thumbnails load lazily in the gallery itself. It shows Gallery-only filter and sort combos for common preview states and orders such as newest, largest body, largest image, cache index, and UUID.

Gallery lazy loading uses a separate async thumbnail worker from manual Preview/Try Next. It builds a bounded queue from the visible gallery neighborhood, prioritizes visible tiles near the viewport center before nearby lookahead rows, attempts unknown entries one at a time, caches successful thumbnails, and marks incomplete/undecodable entries without selecting them.

Gallery item selection uses a small `QListView` subclass so clicks on either the UUID/text area or the thumbnail area select the item.

`cachegui_qt/GalleryActivityIndicator.*` contains Qt gallery thumbnail activity label state and text.

`cachegui_qt/GalleryFilterProxyModel.*` contains the Gallery-only preview-state filtering proxy. Table mode should show all entries even when a Gallery filter is selected.

`cachegui_qt/GalleryListView.*` contains the gallery hit-testing view subclass. Keep this kind of Qt-only UI behavior out of `cachelib`.

`cachegui_qt/GalleryPreviewController.*` contains gallery thumbnail queue/search bookkeeping, scheduled-search decisions, attemptable-entry selection, and activity-state packaging. It should not decode images or inspect widgets directly.

`cachegui_qt/GalleryPreviewQueue.*` contains gallery thumbnail queue bookkeeping and progress counters. It should stay UI-adjacent and must not start worker threads or inspect widgets directly.

`cachegui_qt/GalleryPreviewScanner.*` contains Qt gallery visible-row scanning and queue candidate prioritization. It may inspect Qt view/model geometry, but should not own async worker state.

`cachegui_qt/PreviewCache.*` contains Qt GUI preview state and cached pixmaps. It is intentionally GUI-owned because it stores `QPixmap`; reusable decode/export logic must remain in `cachelib`.

`cachegui_qt/PreviewDecodeWorker.*` contains the Qt GUI async preview decode wrapper around `TextureRebuilder` and `J2CDecoder`. Keep reusable reconstruction and decoding behavior in `cachelib`; this wrapper should only package copied request data/results for the GUI worker flow.

Qt preview decode results distinguish rebuild failures from incomplete/undecodable cached texture data so the GUI can show user-friendly incomplete-texture feedback without changing `cachelib` export semantics.

`cachegui_qt/PreviewImage.*` contains Qt-only conversion from decoded RGBA data to `QPixmap`.

`cachegui_qt/PreviewPanel.*` contains Qt-only preview label/pixmap/message display state, styling, and scaling.

`cachegui_qt/PreviewStatus.*` contains Qt preview cache/model notification helpers, including decoded-image-to-cached-pixmap storage.

`cachegui_qt/PreviewWorkerState.*` contains async preview request state and decode-worker startup shared by manual and gallery preview workers.

`cachegui_qt/QtAboutDialog.*` contains the Qt-only About/diagnostics dialog.

`cachegui_qt/QtActionState.*` contains Qt button enable/disable rules for the main window.

`cachegui_qt/QtFileDialogs.*` contains Qt file/folder dialog helpers for browse/export commands.

`cachegui_qt/QtGallerySort.*` contains the Gallery-only sort control options and proxy-model sort application.

`cachegui_qt/QtGalleryStatus.*` contains Gallery-only count/filter status text.

`cachegui_qt/QtHelpers.*` contains Qt boundary helpers for strings, filesystem paths, default cache path discovery, and accepting either the Firestorm profile directory or its nested `texturecache` folder.
Path probing should use non-throwing filesystem checks so odd user-selected
directories report normal open errors instead of surfacing exceptions.

`cachegui_qt/QtSelection.*` contains Qt table/gallery selection synchronization helpers and selected cached-preview lookup.

`cachegui_qt/QtTextureExport.*` contains Qt GUI export filenames, status text, and option defaults around `TextureExporter`; reusable export behavior must remain in `cachelib`.

`cachegui_qt/QtTryNextPreview.*` contains Qt proxy-model navigation and status text for the "Try Next Preview" action.

`cachegui_qt/QtViewMode.*` contains Qt table/gallery stacked-widget and toggle-button state helpers.

`cachegui_qt/TryNextPreviewState.*` contains the Qt GUI "Try Next Preview" row/attempt bookkeeping. It should not inspect Qt models or start preview workers directly.

`cachegui_qt/CacheEntryTableModel.*` contains the Qt model for cache entries, cached display/sort strings, table sorting data roles, preview status text, and generated gallery placeholder icons.

Gallery placeholders are generated in the Qt model for unknown/checking/no-preview/load-failed states so the grid does not appear empty while lazy loading works through visible entries. Unknown/checking placeholders should stay visually quiet during scrolling; terminal no-preview/error labels should remain readable at the configured gallery tile size.

Gallery mode shows a lightweight activity label while it is scanning visible items, refreshing the visible thumbnail queue, or checking thumbnails with queued progress. The main bottom status label remains reserved for explicit user actions such as Preview, Try Next Preview, Export, and cache open results.

`tests/cachelib_tests.cpp` contains the initial CTest-backed cachelib regression
coverage. It uses synthetic `texture.entries` data to verify usable-entry
filtering, raw `cacheIndex` preservation, UUID lookup behavior, and
`TextureSelection` ordering/clamping. It also uses synthetic `texture.cache` and
body files to verify `TextureRebuilder` reads headers from `cacheIndex * 600`,
uses exactly `bodySize` body bytes, trims padded header-only entries, and treats
undersized body files as errors. It also checks `TextureExportState`
load/save behavior for incomplete entries, metadata invalidation, succeeded
entries, and missing state files.

The Qt table must stay model-backed. An earlier `QTableWidget` version locked up when opening a real cache because it created many cell items and used resize-to-contents behavior on the UI thread.

Qt previews should be rendered from decoded RGBA in memory, not by writing PNG and asking Qt to reload it. The static/minimal Qt build may not have the PNG image loader available even when `TextureExporter` successfully writes a valid PNG.

Qt preview decode now runs off the UI thread via `std::async`, with a Qt timer polling for completion. Worker code uses `TextureRebuilder::Rebuild(cacheDirectory, entry, ...)` so it operates on copied entry data and a cache path instead of touching the live GUI-owned `TextureCacheDatabase`.

`Try Next Preview` should scan forward in the current visible/sorted Qt table order, not raw cache-entry order.

Qt preview state is centralized in `PreviewCache`, which tracks unknown/checking/previewable/unavailable/load-failed state plus cached preview pixmaps. The table model reads status from this cache, and reselecting a previewable entry should show the cached pixmap without decoding again.

The Qt cleanup/polish pass has reduced most repeated preview, gallery,
selection, and export mechanics out of `MainWindow`. Treat further cleanup as
opportunistic unless it directly supports a feature or removes obvious
duplication.

Good next low-risk slices:

* Prefer prebuilt shared Qt for fast local development.
* Keep the vcpkg static Qt path available for reproducible/distribution builds, ideally with binary caching in CI.
* Improve Qt gallery UX: consider richer visible loading progress and possibly multiple thumbnail workers if one-worker throughput is not enough.
* Improve `cachegui_qt` preview presentation and Gallery layout behavior based on real-cache validation.
* Continue packaging/deployment work for the Qt GUI.
* Keep `docs/qt-user-guide.md` and `docs/beta-release-checklist.md` aligned with beta behavior.
* Use `scripts/package-qt-shared.ps1` for repeatable shared-Qt package folders from prebuilt Qt developer builds. Pass `-Zip` when preparing a shareable archive.
* Use `scripts/test-qt-package.ps1` to verify shared-Qt package contents,
  archive contents, checksum, and optional short launch smoke.
* Keep shared behavior in `cachelib`; do not move reusable export, decode, or selection logic into either GUI.

## Coding guidelines

* C++17 only
* RAII
* fixed-width integer types for disk formats
* no Firestorm dependencies
* no CLI/UI concerns in `cachelib`
* no raw owning pointers
* avoid silent truncation of sizes or offsets
* use `std::error_code` for expected filesystem failures where appropriate
* preserve current behavior unless deliberately refactoring it
* build and test after changes

Keep this file up-to-date so you know where we left off if the Codex session is lost.
