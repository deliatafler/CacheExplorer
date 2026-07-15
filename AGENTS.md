# CacheExplorer Development Notes

## Project purpose

CacheExplorer is a standalone, open-source browser and extractor for compatible
Second Life viewer texture caches. It is intended as a modern replacement for
SLCacheViewer. It is verified with the official Second Life viewer and
Firestorm.

The project must not link against or depend on Firestorm itself. Firestorm source code was used only to understand the cache format.

## Architecture

The project has three current targets:

* `cachelib`: reusable cache-reading, reconstruction, decoding, and export library
* `cachecli`: thin command-line frontend
* `cachegui`: Qt 6 cross-platform GUI frontend

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
* official/prebuilt Qt SDK for the Qt GUI
* `x64-windows-static` vcpkg triplet for the core CLI/library path
* `x64-windows-static-md` vcpkg triplet plus dynamic MSVC runtime for the
  preferred prebuilt-Qt GUI path

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
Use `docs/work-checklist.md` for the active beta and post-beta work list.

Primary Qt GUI build with a prebuilt shared Qt installation. This is the preferred developer path because it avoids rebuilding Qt locally and matches the official Qt SDK runtime model:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/configure-qt-prebuilt.ps1 `
  -QtDir C:\Qt\6.11.1\msvc2022_64 `
  -Build
```

Equivalent CMake command:

```bash
cmake -S . -B build-qt-prebuilt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON \
  -DCACHEEXPLORER_STATIC_MSVC_RUNTIME=OFF

cmake --build build-qt-prebuilt --config Release --target cachegui
```

The CMake target remains `cachegui`; its user-facing executable name is
`CacheExplorer.exe`.

Launch raw prebuilt Qt build outputs through `scripts/launch-qt-prebuilt.ps1`
or package them with `scripts/package-qt-shared.ps1`; otherwise Windows will
not find Qt DLLs such as `Qt6Widgets.dll` unless the Qt `bin` directory is
already on `PATH`.

Optional Qt GUI build with vcpkg-provided static Qt. This is useful for reproducible builds and distribution experiments, but first-time dependency setup is slow and it is not the primary contributor path:

```bash
cmake -S . -B build-qt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DVCPKG_MANIFEST_FEATURES=qt-gui \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON

cmake --build build-qt --config Release --target cachegui
```

The `qt-gui` vcpkg feature requests a minimal target Qt Widgets set, but the first Windows static vcpkg configure can still take a long time because host-side Qt tools pull and build a broader dependency graph.

## Viewer texture-cache format

The format below was documented from Firestorm source and is verified against
the official Second Life viewer as well. Keep viewer-specific compatibility
claims limited to viewers that have been tested.

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
Also exposes cached-byte helpers so UI code can ask whether the header/body
bytes cover an entry's advertised image size without duplicating cache-format
math.

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

The Qt 6 GUI in `cachegui` is the supported GUI path. It opens a cache
through `cachelib`, shows entries in a sortable model-backed table, can preview
and export selected entries as PNG through `TextureExporter`, and tracks preview
status in the table.

The Qt GUI has an `About` diagnostics dialog for beta/support reports. Keep it
Qt-only; reusable cache facts should still come from `cachelib`.

Use `docs/qt-gui-validation.md` for the manual Qt GUI smoke/regression checklist. Build-only validation is acceptable for narrow helper moves; run the GUI checklist after changes to async preview flow, selection behavior, gallery loading, export, cache-open behavior, or rendering.

The Qt GUI opens in Gallery mode and has a Gallery/Table toggle. Gallery reuses
the same sorted proxy model and preview cache as the table. Cached previews
appear as icons after they have been decoded by Table selection, Try Next
Preview, or the gallery's lazy visible-item loader.
Gallery tiles flow left to right and wrap to use the available browsing width.
Resizing the Gallery schedules the same visible-thumbnail search used after
scrolling, so newly exposed tiles load without requiring a scroll event.

The Qt GUI provides a `Find UUID` control that looks up an in-world texture UUID
through `TextureCacheDatabase::Find`, selects the matching entry in either view,
and clears a Gallery-only filter only when that filter hides the result.

Gallery mode hides the Table-only `Try Next Preview` action because thumbnails
load lazily in the gallery itself. It shows Gallery-only filter and sort combos
for unknown/no-preview/load-failed states, cached-complete entries, and orders
such as newest, largest body, largest image, cache index, and UUID. `Cached
complete` means the meaningful header bytes plus body bytes cover the advertised
image size; it can include header-only entries with body size `0` and is not a
decode guarantee. Do not expose a Gallery `Previewable` filter unless it has a
clearer workflow than merely showing entries already decoded during this
session.

Gallery lazy loading uses a separate async thumbnail worker from Table selection
previewing and Try Next. It builds a bounded queue from the visible gallery
neighborhood, prioritizes visible tiles near the viewport center before nearby
lookahead rows, attempts unknown entries one at a time, caches successful
thumbnails, and marks incomplete/undecodable entries without selecting them.

Gallery item selection uses a small `QListView` subclass so clicks on either the UUID/text area or the thumbnail area select the item.

`cachegui/GalleryActivityIndicator.*` contains Qt gallery thumbnail activity label state and text.

`cachegui/GalleryFilterProxyModel.*` contains the Gallery-only preview-state filtering proxy. Table mode should show all entries even when a Gallery filter is selected.

`cachegui/GalleryListView.*` contains the gallery hit-testing view subclass. Keep this kind of Qt-only UI behavior out of `cachelib`.

`cachegui/GalleryPreviewController.*` contains gallery thumbnail queue/search bookkeeping, scheduled-search decisions, attemptable-entry selection, and activity-state packaging. It should not decode images or inspect widgets directly.

`cachegui/GalleryPreviewQueue.*` contains gallery thumbnail queue bookkeeping and progress counters. It should stay UI-adjacent and must not start worker threads or inspect widgets directly.

`cachegui/GalleryPreviewScanner.*` contains Qt gallery visible-row scanning and queue candidate prioritization. It may inspect Qt view/model geometry, but should not own async worker state.

`cachegui/PreviewCache.*` contains Qt GUI preview state and cached pixmaps. It is intentionally GUI-owned because it stores `QPixmap`; reusable decode/export logic must remain in `cachelib`.

`cachegui/QtPreviewStateStore.*` contains Qt-only persistence for terminal
preview states. It stores no-preview/load-failed records outside the Firestorm
cache directory and validates UUID/cacheIndex/image/body metadata before
restoring them into `PreviewCache`.

`cachegui/PreviewDecodeWorker.*` contains the Qt GUI async preview decode wrapper around `TextureRebuilder` and `J2CDecoder`. Keep reusable reconstruction and decoding behavior in `cachelib`; this wrapper should only package copied request data/results for the GUI worker flow.

Qt preview decode results distinguish rebuild failures from incomplete/undecodable cached texture data so the GUI can show user-friendly incomplete-texture feedback without changing `cachelib` export semantics.

`cachegui/PreviewImage.*` contains Qt-only conversion from decoded RGBA data to `QPixmap`.

`cachegui/PreviewPanel.*` contains Qt-only preview label/pixmap/message display state, styling, and scaling.

`cachegui/PreviewStatus.*` contains Qt preview cache/model notification helpers, including decoded-image-to-cached-pixmap storage.

`cachegui/PreviewWorkerState.*` contains async preview request state and decode-worker startup shared by manual and gallery preview workers.

`cachegui/QtAboutDialog.*` contains the Qt-only About/diagnostics dialog.

`cachegui/QtActionState.*` contains Qt button enable/disable rules for the main window.

`cachegui/QtFileDialogs.*` contains Qt file/folder dialog helpers for browse/export commands.

`cachegui/QtGallerySort.*` contains the Gallery-only sort control options and proxy-model sort application.

`cachegui/QtGalleryStatus.*` contains Gallery-only count/filter status text.

`cachegui/QtHelpers.*` contains Qt boundary helpers for strings, filesystem
paths, default cache path discovery, and accepting either a viewer profile
directory or its nested `texturecache` folder.
It also persists the last successfully opened cache path through Qt settings and
exposes the default-cache shortcut state. On Windows, the default shortcut
chooses the first existing cache in this order: official Second Life,
official Firestorm, then self-built Firestorm. Keep this per-user UI state
outside the viewer cache directory.
It retains up to eight valid recently opened cache paths for the GUI's `Recent`
menu; missing folders are omitted and users can clear the menu from the UI.
Opening an already open cache path intentionally rereads the cache, supporting
testing and normal browsing while a compatible viewer is actively writing
texture entries.
Path probing should use non-throwing filesystem checks so odd user-selected
directories report normal open errors instead of surfacing exceptions.

`cachegui/QtSelection.*` contains shared Qt table/gallery selection helpers,
selected-entry collection, and selected cached-preview lookup. Table and Gallery
share one extended-selection model so multi-selection survives view changes;
the current selection remains the preview target.

`cachegui/QtTextureExport.*` contains Qt GUI export filenames, status text,
and option defaults around `TextureExporter`; reusable export behavior must
remain in `cachelib`. Multiple selected entries export asynchronously to a
chosen folder without overwriting existing PNGs.

`cachegui/QtTryNextPreview.*` contains Qt proxy-model navigation and status text for the "Try Next Preview" action.

`cachegui/QtViewMode.*` contains Qt table/gallery stacked-widget and toggle-button state helpers.

`cachegui/TryNextPreviewState.*` contains the Qt GUI "Try Next Preview" row/attempt bookkeeping. It should not inspect Qt models or start preview workers directly.

`cachegui/CacheEntryTableModel.*` contains the Qt model for cache entries, cached display/sort strings, table sorting data roles, preview status text, and generated gallery placeholder icons.

Gallery placeholders are generated in the Qt model for unknown/checking/no-preview/load-failed states so the grid does not appear empty while lazy loading works through visible entries. Unknown/checking placeholders should stay visually quiet during scrolling; terminal no-preview/error labels should remain readable at the configured gallery tile size.

Gallery mode shows a lightweight activity label while it is scanning visible
items, refreshing the visible thumbnail queue, or checking thumbnails with
queued progress. The main bottom status label remains reserved for Table
selection previewing, Try Next Preview, Export, and cache open results.

`tests/cachelib_tests.cpp` contains the initial CTest-backed cachelib regression
coverage. It uses synthetic `texture.entries` data to verify usable-entry
filtering, raw `cacheIndex` preservation, UUID lookup behavior, and
`TextureSelection` ordering/clamping. It also uses synthetic `texture.cache` and
body files to verify `TextureRebuilder` reads headers from `cacheIndex * 600`,
uses exactly `bodySize` body bytes, trims padded header-only entries, and treats
undersized body files as errors. It also checks `TextureExportState`
load/save behavior for incomplete entries, metadata invalidation, succeeded
entries, and missing state files.

`cachegui/cachegui_tests.cpp` provides focused CTest coverage for GUI-facing
pure helpers: gallery queue accounting, bounded Try Next traversal, and gallery
status text. It is built only when the Qt GUI and project tests are enabled.

The Qt table must stay model-backed. An earlier `QTableWidget` version locked up when opening a real cache because it created many cell items and used resize-to-contents behavior on the UI thread.

Qt previews should be rendered from decoded RGBA in memory, not by writing PNG and asking Qt to reload it. The static/minimal Qt build may not have the PNG image loader available even when `TextureExporter` successfully writes a valid PNG.

Qt preview decode now runs off the UI thread via `std::async`, with a Qt timer polling for completion. Worker code uses `TextureRebuilder::Rebuild(cacheDirectory, entry, ...)` so it operates on copied entry data and a cache path instead of touching the live GUI-owned `TextureCacheDatabase`. Table selection previewing uses a short debounce; a finished stale request may populate the cache but must not replace the currently selected entry's panel.

`Try Next Preview` should scan forward in the current visible/sorted Qt table order, not raw cache-entry order.

Qt preview state is centralized in `PreviewCache`, which tracks unknown/checking/previewable/unavailable/load-failed state plus cached preview pixmaps. The table model reads status from this cache, and reselecting a previewable entry should show the cached pixmap without decoding again.

The Qt cleanup/polish pass has reduced most repeated preview, gallery,
selection, and export mechanics out of `MainWindow`. Treat further cleanup as
opportunistic unless it directly supports a feature or removes obvious
duplication.

Good next low-risk slices:

* Prefer official/prebuilt shared Qt for normal local development and contributor builds.
* Keep the vcpkg static Qt path available only for reproducible/distribution experiments, ideally with binary caching in CI.
* Improve Qt gallery UX: consider richer visible loading progress and possibly multiple thumbnail workers if one-worker throughput is not enough.
* Improve `cachegui` preview presentation and Gallery layout behavior based on real-cache validation.
* Continue packaging/deployment work for the Qt GUI.
* Keep `docs/qt-user-guide.md` and `docs/beta-release-checklist.md` aligned with beta behavior.
* Use `scripts/package-qt-shared.ps1` for repeatable shared-Qt package folders from prebuilt Qt developer builds. Pass `-Zip` when preparing a shareable archive.
* Use `scripts/test-qt-package.ps1` to verify shared-Qt package contents,
  archive contents, checksum, and optional short launch smoke.
* `CacheExplorer.exe --smoke-open <cache-folder>` opens a cache through the
  normal Qt main-window path, verifies the entry model was populated, and exits
  with a success/failure code. `scripts/test-qt-package.ps1 -ExtractAndLaunch
  -SmokeOpenCache <cache-folder>` runs it from a fresh package extraction.
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
