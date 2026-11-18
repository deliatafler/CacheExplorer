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

The project direction is Qt 6 for GUI development, releases, and cross-platform
support. The native Win32 GUI prototype has been removed; useful follow-up ideas
from it are parked in `docs/post-release-roadmap.md`.

Experimental Aurora Viewer cache support is being developed separately from
the released Second Life/Firestorm path. See
`docs/aurora-cache-experiment.md`. Aurora's current cache is a keyed, sealed
per-asset store rather than the legacy `texture.entries`/`texture.cache`
layout. Keep format-specific parsing, cryptography, and catalog construction in
`cachelib`; do not put them in the Qt GUI. Preserve the existing legacy backend
until Aurora support has independent tests and real-cache validation.

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

MSVC builds explicitly enable standard C++ exception unwinding with `/EHsc`.
Keep this explicit because some prebuilt-Qt build environments do not populate
CMake's normal MSVC exception flags.

Dependencies:

* OpenJPEG
* libpng
* OpenSSL (Aurora sealed-cache cryptography)
* bcdec 0.985 (vendored MIT-licensed Aurora BC7 block decoder)
* macOS Security framework (Aurora Keychain read access)
* Linux libsecret (Aurora Secret Service read access)

The root `VERSION` file is the single source for the user-facing application
version and package filenames. CMake derives its numeric project version from
the same value. Platform workflows must read this file rather than duplicating
version strings, and tagged releases must use exactly `v<VERSION>`.

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

See `docs/qt-build.md` for detailed Qt GUI build, prebuilt-Qt, static-Qt, and deployment notes. See `docs/qt-packaging.md` for the shared-Qt package helper and smoke-test checklist. See `docs/qt-user-guide.md` for user-facing Qt GUI usage notes, `docs/release-checklist.md` before tagging or sharing a release, and `RELEASE_NOTES.md` for release notes.
Use `docs/work-checklist.md` for the active release and post-release work list.

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
The package helper also copies the x64 Visual C++ runtime DLLs app-locally so
the shared ZIP can launch on Windows systems without Visual Studio installed.
Its redistributable lookup must not assume a particular Visual Studio release
directory because hosted CI images may use newer toolsets.

The preferred Windows end-user package is now a CPack/NSIS installer, while the
portable shared-Qt ZIP remains supported. CMake install rules deploy Qt and the
MSVC runtime app-locally. Windows CI builds both packages and runs a temporary
install/launch/uninstall smoke test with `scripts/test-windows-installer.ps1`;
the smoke script must refuse to overwrite an existing registered installation.

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

### TextureCacheChanges

Compares two successful reads of the same cache and returns the current
`cacheIndex` values for entries that were added or whose cache metadata changed.
Keep this reusable comparison in `cachelib`; the GUI owns only the lifetime of
the previous snapshot and the presentation of the result.

### TextureCacheSource

Defines the backend-neutral read-only catalog and encoded-payload boundary for
viewer cache implementations. `TextureCatalogEntry` uses a stable,
filesystem-safe source key, an optional UUID, cached/expected/payload byte
counts, an optional source index, and a timestamp so formats with one-way keyed
filenames can be represented honestly. `TexturePayloadRequest` is an immutable
read request that remains valid if the UI refreshes or closes its source while
a preview worker is active.

`LegacyTextureCacheSource` is the first adapter. It owns the existing
`TextureCacheDatabase`, maps its entries into the neutral catalog, and uses
`TextureRebuilder` to return JPEG2000 payload bytes. The Qt table, Gallery,
selection, preview workers, and PNG export now consume this neutral boundary.
Legacy database access remains temporarily exposed only for legacy refresh
diagnostics and preview-state compatibility. The CLI still uses
`TextureCacheDatabase` directly.

`ViewerTextureCacheSource` auto-detects the legacy and Aurora layouts and
delegates the neutral source contract to the matching adapter. Its move
operations must rebind the active-adapter pointer; the Qt GUI opens a new
source on a worker thread and moves it into the main window only after the
read completes.

### Aurora cache format experiment

`AuroraCacheFormat` validates Aurora's `AUSL` sealed-file envelope without
decrypting it and parses supported plaintext `AUTX` transcoded texture
metadata. It recognizes the current sealed magic, validates chunk/tag sizes,
accepts `AUTX` versions 3 through 6, and rejects unsupported versions or
malformed BC7 mip layouts clearly.

`AuroraCacheProbe` recognizes a cache root, `store`, direct `store/tex`, or a
user-selected parent containing `AuroraViewerCache/store/tex`. It enumerates
32-character lowercase keyed names and groups `.j2c`, `.part`, and `.bc7`
files. Enumeration derives plaintext sizes from sealed file lengths and does
not open every candidate; envelope validation and authentication occur lazily
when a payload is requested.

`AuroraTextureCacheSource` reads the 32-byte installation key from the Windows
Credential Manager, macOS Keychain, or Linux Secret Service, keeps it in
memory only, derives keyed texture names with HKDF-SHA256/HMAC-SHA256, and
opens `AUSL` files with AES-256-GCM through OpenSSL. Real-cache Gallery and
large-preview validation has passed for Aurora JPEG2000 and BC7 data. It
prefers complete JPEG2000, then BC7, then partial JPEG2000 when choosing a
payload, while catalog metadata retains every observed variant.

`AuroraCacheKeyProvider` is the platform-neutral credential boundary. Native
backends use service `Aurora Viewer cache` and account `disk cache key v1`, the
same identifiers as Aurora's keyring 4.x integration. If the native store is
unavailable or missing the entry, the default provider reads Aurora's exact
32-byte `cache.key` fallback from its platform configuration directory. All
access is read-only. The provider never creates, replaces, or deletes keys.
macOS may present the normal Keychain access prompt. Linux builds require
`libsecret-1-dev`; packages dynamically depend on the distribution's libsecret
runtime. Physical macOS and Ubuntu real-cache validation remains pending.

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

### Bc7Decoder and TextureDecoder

`Bc7Decoder` parses Aurora `AUTX` metadata and decodes ordinary BC7 blocks
through the pinned bcdec 0.985 single-header implementation. Preview requests
choose the first mip no larger than 256 pixels when possible; PNG export uses
the finest cached mip. Textures marked keep-RGBA are rejected clearly because
their payload is not BC7.

`TextureDecoder` is the source-neutral decoder dispatch used by GUI previews
and `TextureExporter`. Keep format selection here rather than branching on
payload format in callers.

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

The Qt GUI has an `About` diagnostics dialog for support reports. Keep it
Qt-only; reusable cache facts should still come from `cachelib`.

Use `docs/qt-gui-validation.md` for the manual Qt GUI smoke/regression checklist. Build-only validation is acceptable for narrow helper moves; run the GUI checklist after changes to async preview flow, selection behavior, gallery loading, export, cache-open behavior, or rendering.

The Qt GUI opens in Gallery mode and has a Gallery/Table toggle. Gallery reuses
the same sorted proxy model and preview cache as the table. Cached previews
appear as icons after they have been decoded by Table selection, Try Next
Preview, or the gallery's lazy visible-item loader.
Gallery tiles flow left to right and wrap to use the available browsing width.
Resizing the Gallery schedules the same visible-thumbnail search used after
scrolling, so newly exposed tiles load without requiring a scroll event.
Selected Gallery tiles use a clear blue outline and light blue fill so single
and extended selections remain visible against the normal background.
`cachegui/GalleryItemDelegate.*` paints the palette-aware tile surface,
thumbnail, hover/selection borders, and middle-elided UUID caption.

`cachegui/QtVisualStyle.*` applies the shared palette-aware Qt Widgets styling
for toolbar bands, compact command controls, Table surfaces, and the framed
preview pane. The main content uses a non-collapsible splitter so the Gallery
or Table and the large preview can be resized without changing view behavior.

`cachegui/QtWindowState.*` persists the main-window geometry and content
splitter state through per-user Qt settings. `MainWindow` restores both after
constructing the layout and saves them on a normal close. Common commands use
platform-native Open, Refresh, Find, and Save As key sequences. Keep icon-only
controls named for accessibility and preserve the intentional tab order across
visible Gallery/Table controls.

The Qt GUI provides a `Find UUID` control that looks up an in-world texture UUID
through `TextureCacheDatabase::Find`, selects the matching entry in either view,
and clears a Gallery-only filter only when that filter hides the result.

Gallery mode hides the Table-only `Try Next Preview` action because thumbnails
load lazily in the gallery itself. Its `Show` control offers `Everything`,
`Images only`, and `Recent changes`; sort options include newest, largest body,
largest image, cache index, and UUID. `Recent changes` is populated when the
currently open cache is refreshed and contains entries added or updated since
the previous successful read of that cache. Opening a different cache clears
the delta and returns the Gallery to `Everything`. `Images only` keeps
unknown/checking entries visible as quiet
placeholders while the existing lazy worker attempts them, retains successful
thumbnails, and removes terminal no-preview/load-failed entries. Each removal
refreshes the visible queue so newly exposed entries are attempted without
scanning the entire cache up front. Table mode continues to show all entries.

Gallery lazy loading uses a separate async thumbnail worker from Table selection
previewing and Try Next. It builds a bounded queue from the visible gallery
neighborhood, prioritizes visible tiles near the viewport center before nearby
lookahead rows, and keeps enough queue capacity for a large desktop viewport
before adding lookahead work. It attempts unknown entries one at a time, caches successful
thumbnails, and marks incomplete/undecodable entries without selecting them.

Gallery item selection uses a small `QListView` subclass so clicks on either the UUID/text area or the thumbnail area select the item.

`cachegui/GalleryActivityIndicator.*` contains Qt gallery thumbnail activity label state and text.

`cachegui/GalleryFilterProxyModel.*` contains the Gallery-only preview-state and
recent-change filtering proxy. Table mode should show all entries regardless of
the selected Gallery filter. Keep preview-state refiltering explicit so
thumbnail updates do not cause an expensive dynamic re-sort of the full model.

`cachegui/GalleryListView.*` contains the gallery hit-testing view subclass. Keep this kind of Qt-only UI behavior out of `cachelib`.

`cachegui/GalleryPreviewController.*` contains gallery thumbnail queue/search bookkeeping, scheduled-search decisions, attemptable-entry selection, and activity-state packaging. It should not decode images or inspect widgets directly.

`cachegui/GalleryPreviewQueue.*` contains gallery thumbnail queue bookkeeping and progress counters. It should stay UI-adjacent and must not start worker threads or inspect widgets directly.

`cachegui/GalleryPreviewScanner.*` contains Qt gallery visible-row scanning and queue candidate prioritization. It may inspect Qt view/model geometry, but should not own async worker state.

`cachegui/PreviewCache.*` contains Qt GUI preview state and cached pixmaps. It is intentionally GUI-owned because it stores `QPixmap`; reusable decode/export logic must remain in `cachelib`.

`cachegui/QtPreviewStateStore.*` contains Qt-only persistence for terminal
preview states. It stores no-preview/load-failed records outside the Firestorm
cache directory and validates UUID/cacheIndex/image/body metadata before
restoring them into `PreviewCache`.

`cachegui/PreviewDecodeWorker.*` contains the Qt GUI async preview decode
wrapper around `TextureDecoder`. It requests a bounded BC7 mip for thumbnails
and large previews. Keep reusable reconstruction and decoding behavior in
`cachelib`; this wrapper should only package copied request data/results for
the GUI worker flow.

Qt preview decode results distinguish rebuild failures from incomplete/undecodable cached texture data so the GUI can show user-friendly incomplete-texture feedback without changing `cachelib` export semantics.

`cachegui/PreviewImage.*` contains Qt-only conversion from decoded RGBA data to `QPixmap`.

`cachegui/PreviewPanel.*` contains Qt-only preview label/pixmap/message display state, styling, and scaling.

`cachegui/PreviewStatus.*` contains Qt preview cache/model notification helpers, including decoded-image-to-cached-pixmap storage.

`cachegui/PreviewWorkerState.*` contains async preview request state and decode-worker startup shared by manual and gallery preview workers.

`cachegui/QtAboutDialog.*` contains the Qt-only About/diagnostics dialog.
It also presents the project license, third-party trademark attribution, and
the current cache session's Gallery thumbnail sample. `cachegui/QtGalleryDiagnostics.*`
formats the checked/previewable/unavailable counts and measured single-worker
decode rate for that report.

`cachegui/resources/cacheexplorer.png` is the transparent cross-platform Qt
application icon and README artwork. The multi-resolution
`cachegui/resources/cacheexplorer.ico` is embedded into the Windows executable
through `cacheexplorer.rc`. Keep the packaged README icon and MIT `LICENSE` in
the shared Qt archive. Packages must also include `THIRD_PARTY_NOTICES.md` and
the bcdec license text.

`cachegui/QtActionState.*` contains Qt button enable/disable rules for the main window.

`cachegui/QtFileDialogs.*` contains Qt file/folder dialog helpers for
browse/export commands. PNG export dialogs start in the last accepted export
directory and remember a new directory only after the user accepts a dialog.

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
On macOS, default discovery checks common Second Life and Firestorm directories
under `~/Library/Caches`, and the first folder chooser starts at that cache root
rather than `~/Library/Application Support`.
Aurora discovery checks its current read-only `cache_location.txt` marker for a
custom cache, then its platform default under local application data on Windows
or the user cache directory on macOS/Linux. Preserve the established
Second Life/Firestorm priority ahead of Aurora when several defaults exist.
It retains up to eight valid recently opened cache paths for the GUI's `Recent`
menu; missing folders are omitted and users can clear the menu from the UI.
Remembered paths are lexically normalized, displayed with platform-native path
separators, and compared by normalized generic path so legacy slash variants
are migrated and deduplicated when settings are read.
When no remembered or discovered cache path exists, the first folder chooser
starts in the platform's local application-data directory rather than the
CacheExplorer executable directory.
The last accepted PNG export directory is also stored in per-user Qt settings.
If it is absent or no longer exists, export starts in Pictures, Documents, or
the user's home directory, in that order.
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

`cachegui/QtTextureDetails.*` formats the selected texture's compact preview
metadata. Cache-byte and completeness facts must continue to come from
`cachelib` helpers. Aurora entries identify complete/partial JPEG2000 and BC7
variants explicitly, including a direct `BC7 only` label.

`cachegui/QtTryNextPreview.*` contains Qt proxy-model navigation and status text for the "Try Next Preview" action.

`cachegui/QtViewMode.*` contains Qt table/gallery stacked-widget and toggle-button state helpers.

`cachegui/TryNextPreviewState.*` contains the Qt GUI "Try Next Preview" row/attempt bookkeeping. It should not inspect Qt models or start preview workers directly.

`cachegui/CacheEntryTableModel.*` contains the Qt model for cache entries, cached display/sort strings, table sorting data roles, preview status text, and generated gallery placeholder icons.
For Aurora, the Table calls the anonymous identifier `Cache key`, displays a
neutral data-byte column, and the window hides legacy-only Body and Cache index
columns. Aurora Gallery controls omit the legacy-only Recent changes filter and
legacy metadata sorts.

Gallery placeholders are generated in the Qt model for unknown/checking/no-preview/load-failed states so the grid does not appear empty while lazy loading works through visible entries. Unknown/checking placeholders should stay visually quiet during scrolling; terminal no-preview/error labels should remain readable at the configured gallery tile size.

Gallery mode shows a lightweight activity label while it is scanning visible
items, refreshing the visible thumbnail queue, or checking thumbnails with
queued progress. The main bottom status label remains reserved for Table
selection previewing, Try Next Preview, Export, and cache open results.
The activity label also reports measured single-worker thumbnail decode
throughput while queued work is active. Measurements reset when a cache is
opened and are retained in About diagnostics after visible work settles. An
initial Windows real-cache sample checked 97 thumbnails at 18.0 per second;
keep the single-worker design unless broader usage measurements demonstrate a
need for bounded concurrency.

`Images only` batches terminal preview-state refiltering over a short interval
instead of reflowing the Gallery after every unavailable thumbnail. Queue work
continues during the batch window, then the visible queue is refreshed once for
the accumulated removals.

`tests/cachelib_tests.cpp` contains the initial CTest-backed cachelib regression
coverage. It uses synthetic `texture.entries` data to verify usable-entry
filtering, raw `cacheIndex` preservation, UUID lookup behavior,
`TextureCacheChanges` added/updated detection, and `TextureSelection`
ordering/clamping. It also uses synthetic `texture.cache` and
body files to verify `TextureRebuilder` reads headers from `cacheIndex * 600`,
uses exactly `bodySize` body bytes, trims padded header-only entries, and treats
undersized body files as errors. It also checks `TextureExportState`
load/save behavior for incomplete entries, metadata invalidation, succeeded
entries, and missing state files. The neutral-source coverage verifies that
`LegacyTextureCacheSource` preserves catalog order and metadata, reconstructs
JPEG2000 payloads, maps open/read failures, clears state on close, and creates
detached payload requests that remain valid after the source closes.
The same suite generates synthetic Aurora `AUSL`, opaque keyed-name, plaintext
`AUTX`, and valid mode-6 BC7 fixtures to verify format bounds, unsupported
versions, cache-root probing, grouped texture-file enumeration, stale-file
handling, BC7 mip/alpha decoding, format dispatch, and candidate priority
without using Aurora code, keys, or real cache data.

`tests/cachegui_tests.cpp` provides focused CTest coverage for GUI-facing
helpers: Gallery filtering including recent changes, gallery queue accounting,
bounded Try Next traversal, gallery status text, neutral catalog model mapping,
texture source-detail text including BC7-only entries, and per-user PNG export
directory persistence. It is built only when the Qt GUI and project tests are
enabled.

The Qt table must stay model-backed. An earlier `QTableWidget` version locked up when opening a real cache because it created many cell items and used resize-to-contents behavior on the UI thread.

Qt previews should be rendered from decoded RGBA in memory, not by writing PNG and asking Qt to reload it. The static/minimal Qt build may not have the PNG image loader available even when `TextureExporter` successfully writes a valid PNG.

The large preview pane keeps a selectable UUID caption for the selected entry,
offers a one-click Copy UUID action, and shows compact decoded-dimension, cache
size/completeness, and timestamp details. Status/error messages remain in the
main status bar and preview panel.

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
* Keep `.github/workflows/linux-ci.yml` building the CLI and Qt GUI against
  native Linux Qt 6 plus vcpkg OpenJPEG/libpng on Ubuntu 24.04 and 26.04. Each
  matrix job validates, installs, and uploads a release-specific unsigned
  x86-64 CPack `.deb` plus checksum so distribution package-name transitions
  do not break installation. The Ubuntu 26.04 package has passed full GUI
  validation against a real cache in a desktop VM.
* Keep `.github/workflows/macos-ci.yml` building and testing the Apple Silicon
  Qt GUI, deploying its app-local Qt frameworks, ad-hoc signing the final app
  bundle, validating the CPack DMG, and uploading that DMG as a short-lived
  test artifact. Physical-Mac validation has passed; Developer ID signing and
  notarization remain future distribution-hardening work.
  The DMG test must verify the deployed app's ad-hoc signature integrity with
  `codesign --verify --deep --strict`, report the binary deployment target, and
  require the app-local `@executable_path/../Frameworks` `LC_RPATH` before it
  launches the packaged executable with `--version`; this does not replace
  Developer ID signing or notarization and will not make Gatekeeper accept a
  downloaded app without a per-application override.
* Keep the platform CI workflows reusable through `workflow_call`.
  `.github/workflows/draft-release.yml` manually assembles their five packages
  and unified checksums without publishing; a pushed `v*` tag may create a
  draft GitHub Release, but never publish it automatically.
* Improve Qt gallery UX: consider richer visible loading progress and possibly multiple thumbnail workers if one-worker throughput is not enough.
* Improve `cachegui` preview presentation and Gallery layout behavior based on real-cache validation.
* Continue packaging/deployment work for the Qt GUI.
* Keep `docs/qt-user-guide.md` and `docs/release-checklist.md` aligned with release behavior.
* Use `scripts/package-qt-shared.ps1` for repeatable shared-Qt package folders
  from prebuilt Qt developer builds. Pass `-Zip` when preparing a shareable
  archive. The package excludes Qt's optional generic TUIO touch plugin and its
  transitive Qt Network dependency; CacheExplorer itself does not use Qt
  networking.
* Use `scripts/test-qt-package.ps1` to verify shared-Qt package contents,
  archive contents, checksum, and optional short launch smoke. Its child process
  uses isolated runtime/plugin paths so an installed Qt SDK cannot mask missing
  app-local package files.
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
