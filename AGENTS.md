# CacheExplorer Development Notes

## Project purpose

CacheExplorer is a standalone, open-source browser and extractor for the Firestorm Viewer texture cache. It is intended as a modern replacement for SLCacheViewer.

The project must not link against or depend on Firestorm itself. Firestorm source code was used only to understand the cache format.

## Architecture

The project has four current targets:

* `cachelib`: reusable cache-reading, reconstruction, decoding, and export library
* `cachecli`: thin command-line frontend
* `cachegui`: native Win32 GUI frontend
* `cachegui_qt`: experimental Qt 6 cross-platform GUI spike

GUI targets depend directly on `cachelib`. A GUI must not wrap or invoke the CLI.

Business logic belongs in `cachelib`. CLI-specific argument parsing and console presentation belong in `cachecli`.

The cross-platform GUI direction is being evaluated with Qt 6 in `cachegui_qt`. Keep the Win32 GUI working while the Qt spike proves build, packaging, and UX tradeoffs.

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

Experimental Qt GUI build with a prebuilt shared Qt installation. This is the preferred developer path because it avoids rebuilding Qt locally:

```bash
cmake -S . -B build-qt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON

cmake --build build-qt --config Release --target cachegui_qt
```

Validated prebuilt-Qt developer build:

* Qt 6.8.3 `win64_msvc2022_64` installed with `aqtinstall` into `.qt-prebuilt-test/Qt/6.8.3/msvc2022_64`.
* Fresh configure used `build-qt-prebuilt`, `CMAKE_PREFIX_PATH` pointing at that Qt tree, `x64-windows-static`, and no `qt-gui` vcpkg manifest feature. vcpkg restored only OpenJPEG/libpng/zlib from binary cache.
* Configure completed in about 18 seconds; `cmake --build build-qt-prebuilt --config Release --target cachegui_qt` completed in about 7 seconds.
* The resulting GUI launches when the prebuilt Qt `bin` directory is on `PATH`.
* Link warnings are expected for this developer path right now: official shared Qt uses the dynamic MSVC runtime, while the project/vcpkg static triplet uses the static runtime (`LNK4098` and related runtime import warnings). This is acceptable for developer validation, but packaging should either use the static Qt/vcpkg path or a consistent dynamic-runtime configuration.
* `windeployqt --dry-run --release build-qt-prebuilt/cachegui_qt/Release/cachegui_qt.exe` identifies the expected Qt DLL/plugin deployment set. Run it from a proper Visual Studio developer environment for packaging so `VCINSTALLDIR` is set.

Experimental Qt GUI build with vcpkg-provided static Qt. This is useful for reproducible builds and distribution experiments, but first-time dependency setup is slow:

```bash
cmake -S . -B build-qt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DVCPKG_MANIFEST_FEATURES=qt-gui \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON

cmake --build build-qt --config Release --target cachegui_qt
```

The `qt-gui` vcpkg feature requests a minimal target Qt Widgets set, but the first Windows static vcpkg configure can still take a long time because host-side Qt tools pull and build a broader dependency graph. For CI, use vcpkg binary caching so Qt is built once and restored afterward.

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

The first GUI cleanup step extracted standalone Win32 utility helpers into `cachegui/GuiUtils.*` without changing behavior.

The Qt 6 spike adds `cachegui_qt`, an optional target that opens a cache through `cachelib`, shows entries in a sortable model-backed table, can preview/export a selected entry as PNG through `TextureExporter`, and tracks preview status in the table.

The Qt GUI also has an early Gallery/Table toggle. Gallery reuses the same sorted proxy model and preview cache as the table. Cached previews appear as icons after they have been decoded by Preview, Try Next Preview, or the gallery's lazy visible-item loader.

Gallery lazy loading uses a separate async thumbnail worker from manual Preview/Try Next. It builds a bounded queue from the visible gallery neighborhood, attempts unknown entries one at a time, caches successful thumbnails, and marks incomplete/undecodable entries without selecting them.

Gallery item selection uses a small `QListView` subclass so clicks on either the UUID/text area or the thumbnail area select the item.

`cachegui_qt/GalleryListView.*` contains the gallery hit-testing view subclass. Keep this kind of Qt-only UI behavior out of `cachelib`.

`cachegui_qt/PreviewCache.*` contains Qt GUI preview state and cached pixmaps. It is intentionally GUI-owned because it stores `QPixmap`; reusable decode/export logic must remain in `cachelib`.

`cachegui_qt/PreviewDecodeWorker.*` contains the Qt GUI async preview decode wrapper around `TextureRebuilder` and `J2CDecoder`. Keep reusable reconstruction and decoding behavior in `cachelib`; this wrapper should only package copied request data/results for the GUI worker flow.

`cachegui_qt/PreviewImage.*` contains Qt-only conversion from decoded RGBA data to `QPixmap`.

`cachegui_qt/CacheEntryTableModel.*` contains the Qt model for cache entries, table sorting data roles, preview status text, and generated gallery placeholder icons.

Gallery placeholders are generated in the Qt model for unknown/checking/no-preview/load-failed states so the grid does not appear empty while lazy loading works through visible entries.

Gallery mode shows a lightweight activity label while it is scanning visible items, refreshing the visible thumbnail queue, or checking thumbnails with queued progress. The main bottom status label remains reserved for explicit user actions such as Preview, Try Next Preview, Export, and cache open results.

The Qt table must stay model-backed. An earlier `QTableWidget` version locked up when opening a real cache because it created many cell items and used resize-to-contents behavior on the UI thread.

Qt previews should be rendered from decoded RGBA in memory, not by writing PNG and asking Qt to reload it. The static/minimal Qt build may not have the PNG image loader available even when `TextureExporter` successfully writes a valid PNG.

Qt preview decode now runs off the UI thread via `std::async`, with a Qt timer polling for completion. Worker code uses `TextureRebuilder::Rebuild(cacheDirectory, entry, ...)` so it operates on copied entry data and a cache path instead of touching the live GUI-owned `TextureCacheDatabase`.

`Try Next Preview` should scan forward in the current visible/sorted Qt table order, not raw cache-entry order.

Qt preview state is centralized in `PreviewCache`, which tracks unknown/checking/previewable/unavailable/load-failed state plus cached preview pixmaps. The table model reads status from this cache, and reselecting a previewable entry should show the cached pixmap without decoding again.

Good next low-risk slices:

* Prefer prebuilt shared Qt for fast local development.
* Keep the vcpkg static Qt path available for reproducible/distribution builds, ideally with binary caching in CI.
* Improve Qt gallery thumbnail scheduling and UX: consider richer visible loading progress and possibly multiple thumbnail workers if one-worker throughput is not enough.
* If Qt remains the path, improve `cachegui_qt` preview scaling, incomplete-texture feedback, and the bounded "Try Next Preview" workflow.
* If Win32 remains active, move GUI control IDs and custom window-message IDs into a small header.
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
