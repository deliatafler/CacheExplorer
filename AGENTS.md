# CacheExplorer Development Notes

## Project purpose

CacheExplorer is a standalone, open-source browser and extractor for the Firestorm Viewer texture cache. It is intended as a modern replacement for SLCacheViewer.

The project must not link against or depend on Firestorm itself. Firestorm source code was used only to understand the cache format.

## Architecture

The project has three current targets:

* `cachelib`: reusable cache-reading, reconstruction, decoding, and export library
* `cachecli`: thin command-line frontend
* `cachegui`: native Win32 GUI frontend

The GUI depends directly on `cachelib`. The GUI must not wrap or invoke the CLI.

Business logic belongs in `cachelib`. CLI-specific argument parsing and console presentation belong in `cachecli`.

Future GUI work should consider whether moving to a cross-platform framework would make sense so contributors can build and run the GUI on macOS and Linux as well as Windows. Treat this as a future evaluation item, not a reason to rewrite the current Win32 GUI prematurely.

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

Good next low-risk slices:

* Move GUI control IDs and custom window-message IDs into a small header.
* Move `AppState` and message payload structs into a GUI state header.
* Split preview/gathered-preview-index behavior from `main.cpp` while keeping UI drawing in the GUI target.
* After the native GUI is easier to reason about, evaluate a cross-platform GUI framework as a separate design decision.

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
