# Qt GUI packaging notes

The preferred fast packaging path for day-to-day testing is a shared-Qt folder
created from a prebuilt Qt installation. This produces a directory that can be
zipped and shared for smoke testing without rebuilding Qt locally.

For a single static binary or a more reproducible release experiment, keep using
the vcpkg static Qt path described in `docs/qt-build.md`.

## Shared Qt package

Prerequisites:

* A Release `cachegui_qt` build configured against a prebuilt shared Qt
  installation.
* `windeployqt.exe` from that same Qt installation, either on `PATH` or supplied
  with `-QtBinDir`.
* A Visual Studio developer environment when producing a package for sharing, so
  Qt deployment can locate the expected MSVC runtime context.

Example from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.8.3\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared
```

If `windeployqt.exe` is already on `PATH`, omit `-QtBinDir`.

The package directory contains `CacheExplorer.exe` plus the Qt DLLs and plugin
folders copied by `windeployqt`. Zip the package directory after a smoke test if
you want to share it.

## Smoke test checklist

Run the packaged executable from inside the package directory, then:

* Open a real Firestorm `texturecache` folder.
* Switch between Table and Gallery views.
* Confirm Gallery thumbnails load while scrolling.
* Preview a texture from Table view.
* Export a previewable texture as PNG.

Use `docs/qt-gui-validation.md` for the fuller GUI regression checklist.

## Static release path

The shared package is convenient for development and test sharing, but it is not
the final static-binary story. Static Qt distribution experiments should use the
vcpkg `qt-gui` feature and `x64-windows-static` triplet from `docs/qt-build.md`.

Do not mix a dynamic official Qt runtime with a static-vcpkg dependency graph
for a final release package unless the runtime model is deliberately made
consistent.

## Troubleshooting

If the app reports a missing Qt platform plugin, rerun `windeployqt` from the Qt
installation used during CMake configure.

If the package launches only on the build machine, produce it from a Visual
Studio developer environment and verify the MSVC runtime deployment choice.

If the wrong Qt version is deployed, inspect `CMAKE_PREFIX_PATH` or `Qt6_DIR` in
the CMake cache for the build directory being packaged.
