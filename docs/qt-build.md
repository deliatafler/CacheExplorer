# Qt GUI build notes

The Qt 6 GUI in `cachegui` is the GUI path for future cross-platform work
and beta testing.

## Fast developer build with prebuilt Qt

Use an official/prebuilt shared Qt installation for normal local iteration.
This avoids rebuilding Qt locally and is the intended contributor path.

The recommended Windows setup is:

* Install the official Qt 6 MSVC 2022 64-bit SDK.
* Use vcpkg for CacheExplorer's non-Qt dependencies only.
* Use `x64-windows-static-md` so OpenJPEG/libpng remain static while the MSVC
  runtime model matches the official Qt SDK.

The helper script locates `QT_DIR`, common `C:\Qt\<version>\msvc2022_64`
installations, or accepts `-QtDir` explicitly:

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

The target remains named `cachegui`, but the user-facing executable is
`CacheExplorer.exe`. The resulting GUI launches when the prebuilt Qt `bin`
directory is on `PATH`. Use the helper script so the raw build output starts
with the matching Qt runtime:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/launch-qt-prebuilt.ps1 `
  -QtDir C:\Qt\6.11.1\msvc2022_64
```

Use `docs/qt-packaging.md` when you want a repeatable shared-Qt package folder
that can be zipped and shared for smoke testing.

To build and run the focused Qt helper tests:

```powershell
cmake --build build-qt-prebuilt --config Release --target cachegui_tests cachelib_tests
ctest --test-dir build-qt-prebuilt -C Release --output-on-failure
```

The `cachegui_tests` CTest registration adds the prebuilt Qt runtime directory
automatically, so this command does not require a manual `PATH` change.

## GitHub Actions CI

`.github/workflows/windows-ci.yml` validates the supported Windows paths on
pushes, pull requests, and manual dispatches. It checks out the vcpkg manifest
baseline, restores a vcpkg binary cache, builds and tests the static core
configuration, then installs the matching prebuilt Qt MSVC 2022 kit to build
the GUI and run focused GUI-helper regression tests.

The workflow creates the same shared-Qt ZIP used for beta testing, verifies its
checksum and a fresh-extraction launch smoke, and uploads the ZIP plus its
`.sha256` file as a short-lived workflow artifact. It intentionally does not
publish GitHub Releases; release publication remains an explicit tagged-release
step after human validation.

Local Windows development currently uses Qt 6.11.1 through the official Qt
installer. CI uses the public Qt 6.8.3 MSVC 2022 package because it is available
through the non-authenticated aqt repository used by GitHub Actions. Both use
the same supported compiler family and the GUI's Qt Widgets API surface; CI does
not require contributors to replace their local SDK.

`.github/workflows/linux-ci.yml` provides early cross-platform coverage on an
Ubuntu hosted runner. It installs the native Qt 6 Widgets development package,
uses vcpkg only for OpenJPEG/libpng, then builds the CLI and Qt GUI and runs
CTest on both Ubuntu 24.04 and 26.04. Each job creates a release-specific native
x86-64 Debian package, validates its desktop launcher, metadata, files,
checksum, and shared-library dependencies, installs it through `apt`, and
performs a headless version launch. The unsigned `.deb` files and checksums are
uploaded as clearly labeled short-lived workflow artifacts. Separate packages
are intentional because Ubuntu release transitions can rename Qt runtime
packages even when their shared-library ABI remains compatible. Real-cache GUI
validation on a Linux desktop remains a separate milestone.

`.github/workflows/macos-ci.yml` provides the corresponding early macOS
coverage on GitHub's Apple Silicon `macos-15` runner. It installs the prebuilt
Qt 6.8.3 macOS SDK, uses the vcpkg `arm64-osx` triplet for OpenJPEG/libpng,
builds the CLI and Qt GUI as an ARM64 `.app` bundle, and runs CTest. It then
uses Qt's deployment support and CPack to produce and inspect an ad-hoc-signed
DMG, which is uploaded as a short-lived workflow artifact. Developer ID
signing, notarization, and real-cache GUI validation on physical Mac hardware
remain separate release milestones.

The generated DMG is suitable for development testing, but macOS Gatekeeper
requires a per-application override because it is not Developer ID signed or
notarized. See `docs/qt-packaging.md` for the current `Privacy & Security` test
procedure, local package commands, and package contents.

### Validated prebuilt-Qt developer setup

* Qt 6.11.1 `msvc2022_64` installed into
  `C:/Qt/6.11.1/msvc2022_64`.
* Fresh configure used `build-qt-prebuilt`, `CMAKE_PREFIX_PATH` pointing at
  that Qt tree, `x64-windows-static-md`, and no `qt-gui` vcpkg manifest feature.
  vcpkg restored only OpenJPEG/libpng/zlib from binary cache.
* Configure completed in about 18 seconds.
* `cmake --build build-qt-prebuilt --config Release --target cachegui`
  completed in about 7 seconds.
* The prebuilt helper disables CacheExplorer's static MSVC runtime setting so
  the app matches the official Qt SDK's dynamic runtime model.

## Static/reproducible Qt build with vcpkg

Use vcpkg-provided static Qt only for reproducible builds and distribution
experiments. This is not the normal contributor path. First-time dependency
setup is slow.

```bash
cmake -S . -B build-qt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DVCPKG_MANIFEST_FEATURES=qt-gui \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON

cmake --build build-qt --config Release --target cachegui
```

The `qt-gui` vcpkg feature requests a minimal target Qt Widgets set, but the
first Windows static vcpkg configure can still take a long time because
host-side Qt tools pull and build a broader dependency graph. For CI, use vcpkg
binary caching so Qt is built once and restored afterward.

## Deployment

For a shared/prebuilt Qt developer build, `windeployqt` identifies the expected
Qt DLL/plugin deployment set:

```bash
windeployqt --dry-run --release build-qt-prebuilt/cachegui/Release/CacheExplorer.exe
```

Run `windeployqt` from a proper Visual Studio developer environment for
packaging so `VCINSTALLDIR` is set.

The repository includes a small wrapper for the common shared-Qt package flow:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.11.1\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared `
  -Zip
```

See `docs/qt-packaging.md` for package contents, smoke testing, and
troubleshooting notes.
