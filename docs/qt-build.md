# Qt GUI build notes

The Qt 6 GUI in `cachegui_qt` is the GUI path for future cross-platform work
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
  -QtDir C:\Qt\6.8.3\msvc2022_64 `
  -Build
```

Equivalent CMake command:

```bash
cmake -S . -B build-qt-prebuilt -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON \
  -DCACHEEXPLORER_STATIC_MSVC_RUNTIME=OFF

cmake --build build-qt-prebuilt --config Release --target cachegui_qt
```

The target remains named `cachegui_qt`, but the user-facing executable is
`CacheExplorer.exe`. The resulting GUI launches when the prebuilt Qt `bin`
directory is on `PATH`.
Use `docs/qt-packaging.md` when you want a repeatable shared-Qt package folder
that can be zipped and shared for smoke testing.

### Validated prebuilt-Qt developer setup

* Qt 6.8.3 `win64_msvc2022_64` installed with `aqtinstall` into
  `.qt-prebuilt-test/Qt/6.8.3/msvc2022_64`.
* Fresh configure used `build-qt-prebuilt`, `CMAKE_PREFIX_PATH` pointing at
  that Qt tree, `x64-windows-static-md`, and no `qt-gui` vcpkg manifest feature.
  vcpkg restored only OpenJPEG/libpng/zlib from binary cache.
* Configure completed in about 18 seconds.
* `cmake --build build-qt-prebuilt --config Release --target cachegui_qt`
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

cmake --build build-qt --config Release --target cachegui_qt
```

The `qt-gui` vcpkg feature requests a minimal target Qt Widgets set, but the
first Windows static vcpkg configure can still take a long time because
host-side Qt tools pull and build a broader dependency graph. For CI, use vcpkg
binary caching so Qt is built once and restored afterward.

## Deployment

For a shared/prebuilt Qt developer build, `windeployqt` identifies the expected
Qt DLL/plugin deployment set:

```bash
windeployqt --dry-run --release build-qt-prebuilt/cachegui_qt/Release/CacheExplorer.exe
```

Run `windeployqt` from a proper Visual Studio developer environment for
packaging so `VCINSTALLDIR` is set.

The repository includes a small wrapper for the common shared-Qt package flow:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.8.3\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared `
  -Zip
```

See `docs/qt-packaging.md` for package contents, smoke testing, and
troubleshooting notes.
