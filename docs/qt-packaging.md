# Qt GUI packaging notes

The preferred packaging path for day-to-day testing is a shared-Qt folder
created from an official/prebuilt Qt installation. This produces a directory
that can be zipped and shared for smoke testing without rebuilding Qt locally.

For a single static binary or a more reproducible release experiment later, keep
the optional vcpkg static Qt path described in `docs/qt-build.md`.

## Shared Qt package

Prerequisites:

* A Release `cachegui_qt` build configured against a prebuilt shared Qt
  installation. Prefer `scripts/configure-qt-prebuilt.ps1` for this configure.
* `windeployqt.exe` from that same Qt installation, either on `PATH` or supplied
  with `-QtBinDir`.
* A Visual Studio developer environment when producing a package for sharing, so
  Qt deployment can locate the expected MSVC runtime context.

Example from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.11.1\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared `
  -Zip
```

If `windeployqt.exe` is already on `PATH`, omit `-QtBinDir`.

The package directory contains `CacheExplorer.exe`, Qt DLLs and plugin folders
copied by `windeployqt`, `README.md`, `RELEASE_NOTES.md`, and
`docs/qt-user-guide.md`. It also writes `PACKAGE_INFO.txt` with the package
version and build/deployment details for beta support reports. With `-Zip`, the
script also creates
`artifacts/cacheexplorer-qt-shared.zip` unless `-ZipPath` is supplied. It also
writes a `.sha256` checksum file next to the zip unless `-NoChecksum` is used.

If `VCINSTALLDIR` is not set, the script warns that it is not running from a
Visual Studio developer environment. That warning is acceptable for local smoke
testing, but use a developer shell for packages you plan to share.

## Smoke test checklist

Run the packaged executable from inside the package directory, then:

* Open a real Firestorm `texturecache` folder.
* Switch between Table and Gallery views.
* Confirm Gallery thumbnails load while scrolling.
* Preview a texture from Table view.
* Export a previewable texture as PNG.

Use `docs/qt-gui-validation.md` for the fuller GUI regression checklist.

For a repeatable package-content, checksum, and launch smoke test:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-qt-package.ps1 `
  -PackageDir artifacts\cacheexplorer-qt-shared `
  -ZipPath artifacts\cacheexplorer-qt-shared.zip `
  -Launch
```

## Optional static release path

The shared package is the current beta packaging path. Static Qt distribution
experiments can use the vcpkg `qt-gui` feature and `x64-windows-static` triplet
from `docs/qt-build.md`, but contributors should not need to build Qt this way.

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
