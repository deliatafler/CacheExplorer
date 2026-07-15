# CacheExplorer

CacheExplorer is a standalone browser and PNG exporter for Second Life viewer
texture caches. It is intended as a modern, open-source replacement for the
defunct SLCacheViewer. It is verified with the official Second Life viewer and
Firestorm.

CacheExplorer does not link against Firestorm and does not require Firestorm
source code. Firestorm source was used only to understand the texture-cache file
format.

## Status

CacheExplorer is approaching a first beta. The Qt 6 GUI is the app path for
future development, beta testing, and cross-platform work.

Many real cache entries are expected to be incomplete or undecodable because
viewers use progressive JPEG2000 texture caching. CacheExplorer treats those
entries as ordinary no-preview cases, not application failures.

## Features

* Open a compatible viewer `texturecache` folder directly.
* Browse cache entries in Table or Gallery view.
* Lazy-load Gallery thumbnails while scrolling.
* Preview decodable textures.
* Export one or many selected textures as PNG.
* Show basic app, Qt, and cache diagnostics from the GUI.
* Use the CLI for scan, list, verify, stats, and export workflows.

See `docs/qt-user-guide.md` for Qt GUI usage and limitations.

## Build

The project uses C++17, CMake, MSVC, and vcpkg manifest mode on Windows.
For the Qt GUI, the intended contributor path is an official/prebuilt Qt SDK;
contributors should not need to build Qt from source.

### Core CLI

```bash
cmake -S . -B build -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

This default build includes the core CLI/library path and cachelib regression
tests.

### Qt GUI with prebuilt shared Qt

This is the supported beta GUI path. It is also the preferred developer path
because it avoids rebuilding Qt locally.

The helper script configures the recommended build: official/prebuilt Qt,
static vcpkg libraries for OpenJPEG/libpng, and the dynamic MSVC runtime to
match the official Qt SDK.

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

cmake --build build-qt-prebuilt --config Release --target cachegui_qt
```

The target name is `cachegui_qt`; the built GUI executable is
`build-qt-prebuilt/cachegui_qt/Release/CacheExplorer.exe`.
Launch the raw prebuilt build through the helper so Windows can find the Qt
runtime DLLs:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/launch-qt-prebuilt.ps1 `
  -QtDir C:\Qt\6.11.1\msvc2022_64
```

See `docs/qt-build.md` for deployment details and the optional static Qt path.

## Packaging

For a shared-Qt test package:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.11.1\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared `
  -Zip
```

The package includes `CacheExplorer.exe`, Qt runtime files, the README, release
notes, Qt user guide, and `PACKAGE_INFO.txt` with version/build details. See
`docs/qt-packaging.md` for package contents and smoke testing. Use
`scripts/test-qt-package.ps1` for repeatable package-content, checksum, and
launch smoke checks.

## Validation

Use `docs/qt-gui-validation.md` after GUI behavior changes. Build-only
validation is usually enough for documentation-only and narrow helper changes.
Use `docs/beta-release-checklist.md` before tagging or sharing a beta package.
See `RELEASE_NOTES.md` for draft beta release notes.
