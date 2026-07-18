<p align="center">
  <img src="cachegui/resources/cacheexplorer.png" alt="CacheExplorer icon" width="144">
</p>

# CacheExplorer

CacheExplorer is a standalone browser and PNG exporter for Second Life® viewer
texture caches. It is intended as a modern, open-source replacement for the
defunct SLCacheViewer. It is verified with the official Second Life viewer and
Firestorm Viewer.

CacheExplorer does not link against Firestorm and does not require Firestorm
source code. Firestorm source was used only to understand the texture-cache file
format.

## Status

CacheExplorer is in active cross-platform beta testing. The Qt 6 GUI is the app
path for future development and cross-platform work.

Many real cache entries are expected to be incomplete or undecodable because
viewers use progressive JPEG2000 texture caching. CacheExplorer treats those
entries as ordinary no-preview cases, not application failures.

Beta feedback and bug reports are collected through
[GitHub Issues](https://github.com/deliatafler/CacheExplorer/issues). For GUI
problems, include the diagnostics shown by `About` when possible.

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

cmake --build build-qt-prebuilt --config Release --target cachegui
```

The target name is `cachegui`; the built GUI executable is
`build-qt-prebuilt/cachegui/Release/CacheExplorer.exe`.
Launch the raw prebuilt build through the helper so Windows can find the Qt
runtime DLLs:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/launch-qt-prebuilt.ps1 `
  -QtDir C:\Qt\6.11.1\msvc2022_64
```

See `docs/qt-build.md` for deployment details and the optional static Qt path.

## Packages

Cross-platform beta candidates are produced by GitHub Actions:

* Windows x64 installer and portable ZIP with app-local Qt and Visual C++
  runtime files.
* Apple Silicon macOS DMG with a native application bundle and app-local Qt
  frameworks. The current test package is ad-hoc signed for integrity but is
  not Developer ID signed or notarized, so Gatekeeper requires a manual
  per-application override described in `docs/qt-packaging.md`.
* Ubuntu 24.04 and 26.04 x86-64 Debian packages using each release's native Qt
  runtime dependencies.

Version tags assemble these platform packages and a unified checksum file into
a draft GitHub Release. See `docs/qt-packaging.md` for package-specific build,
installation, and smoke-test details.

### Windows packages

The Windows installer is the recommended package for normal use. It installs
CacheExplorer under Program Files, creates a Start menu shortcut, and registers
a standard uninstaller. The portable ZIP remains available for removable or
no-install use.

Build the installer from a configured Release build with NSIS 3.03 or newer:

```powershell
cpack --config build-qt-prebuilt/CPackConfig.cmake `
  -C Release -G NSIS -B artifacts
```

See `docs/qt-packaging.md` for installer lifecycle smoke testing and unsigned
beta caveats.

For a shared-Qt test package:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.11.1\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared `
  -Zip
```

The package includes `CacheExplorer.exe`, Qt and Visual C++ runtime files, the
README, release notes, Qt user guide, and `PACKAGE_INFO.txt` with version/build
details. See
`docs/qt-packaging.md` for package contents and smoke testing. Use
`scripts/test-qt-package.ps1` for repeatable package-content, checksum, and
launch smoke checks.

## Validation

Use `docs/qt-gui-validation.md` after GUI behavior changes. Build-only
validation is usually enough for documentation-only and narrow helper changes.
Use `docs/beta-release-checklist.md` before tagging or sharing a beta package.
See `RELEASE_NOTES.md` for draft beta release notes.

## License

CacheExplorer is available under the [MIT License](LICENSE).

## Trademarks

Second Life® is a registered trademark of Linden Research, Inc. Firestorm Viewer
is a trademark of The Phoenix Firestorm Project, Inc. CacheExplorer is not
affiliated with, sponsored by, or endorsed by Linden Research, Inc. or The
Phoenix Firestorm Project, Inc.
