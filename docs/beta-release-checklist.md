# Beta release checklist

Use this checklist before tagging or sharing a beta package.

## Version and scope

* Confirm `CMakeLists.txt` has the intended numeric project version and
  user-facing `CACHEEXPLORER_DISPLAY_VERSION` beta version.
* Confirm the CLI usage output and Qt `About` dialog show that same version.
* Confirm the Qt GUI is the beta-facing UI and the Win32 GUI remains documented
  as deprecated legacy.
* Confirm known limitations are documented in `README.md` and
  `docs/qt-user-guide.md`.

## Builds

Run:

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build-qt --config Release --target cachegui_qt
cmake --build build-qt-prebuilt --config Release --target cachegui_qt
```

For the deprecated Win32 opt-out path, at least once before beta:

```bash
cmake -S . -B build-cli-only -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DCACHEEXPLORER_BUILD_LEGACY_WIN32_GUI=OFF

cmake --build build-cli-only --config Release
```

## Package

From a Visual Studio developer shell, create the shared-Qt package:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.8.3\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared `
  -Zip
```

The package archive should contain `CacheExplorer.exe`, Qt DLLs, Qt plugin
folders, `README.md`, `RELEASE_NOTES.md`, and `docs/qt-user-guide.md`. The
package script should also include `PACKAGE_INFO.txt` and create a `.sha256`
checksum next to the zip.

Run the package smoke script:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-qt-package.ps1 `
  -PackageDir artifacts\cacheexplorer-qt-shared `
  -ZipPath artifacts\cacheexplorer-qt-shared.zip `
  -Launch
```

## GUI smoke test

Run the packaged `CacheExplorer.exe`, then complete
`docs/qt-gui-validation.md`.

Minimum beta acceptance:

* Opens a real Firestorm texture cache without freezing.
* Table and Gallery views both work.
* Gallery thumbnails load while scrolling.
* Preview/export works for known-good entries.
* Incomplete or undecodable textures are reported as ordinary no-preview cases.
* `About` shows app, Qt, and cache diagnostics.

## Release notes

Before publishing, write short notes that include:

* This is a beta.
* Qt GUI is the supported interface.
* Win32 GUI is deprecated legacy.
* Many Firestorm cache entries may not preview because they are incomplete.
* The app is standalone and does not depend on Firestorm.
* The package checksum.
