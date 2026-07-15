# CacheExplorer

CacheExplorer is a standalone browser and PNG exporter for the Firestorm Viewer
texture cache. It is intended as a modern, open-source replacement for the
defunct SLCacheViewer.

CacheExplorer does not link against Firestorm and does not require Firestorm
source code. Firestorm source was used only to understand the texture-cache file
format.

## Status

CacheExplorer is approaching a first beta. The Qt 6 GUI is the primary app path
for future development, beta testing, and cross-platform work.

The native Win32 GUI is deprecated legacy code. It is still kept buildable as a
short-term reference/fallback, but new GUI features and user-facing polish should
target the Qt GUI.

Many real cache entries are expected to be incomplete or undecodable because
Firestorm uses progressive JPEG2000 texture caching. CacheExplorer should treat
those entries as ordinary no-preview cases, not application failures.

## Features

* Open a Firestorm `texturecache` folder directly.
* Browse cache entries in Table or Gallery view.
* Lazy-load Gallery thumbnails while scrolling.
* Preview decodable textures.
* Export decodable textures as PNG.
* Show basic app, Qt, and cache diagnostics from the GUI.
* Use the CLI for scan, list, verify, stats, and export workflows.

See `docs/qt-user-guide.md` for Qt GUI usage and limitations.

## Build

The project uses C++17, CMake, MSVC, and vcpkg manifest mode on Windows.

### Core CLI

```bash
cmake -S . -B build -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

This default build currently also builds the deprecated Win32 GUI and the
cachelib regression tests. To build only the core CLI/library path, add
`-DCACHEEXPLORER_BUILD_LEGACY_WIN32_GUI=OFF`.

### Qt GUI with prebuilt shared Qt

This is the supported beta GUI path. It is also the preferred developer path
because it avoids rebuilding Qt locally.

```bash
cmake -S . -B build-qt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON

cmake --build build-qt --config Release --target cachegui_qt
```

The target name is `cachegui_qt`; the built GUI executable is
`build-qt/cachegui_qt/Release/CacheExplorer.exe`.

See `docs/qt-build.md` for static Qt and deployment details.

## Packaging

For a shared-Qt test package:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.8.3\msvc2022_64\bin `
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
