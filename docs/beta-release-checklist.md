# Beta release checklist

Use this checklist before tagging or sharing a beta package.

## Version and scope

* Confirm `CMakeLists.txt` has the intended numeric project version and
  user-facing `CACHEEXPLORER_DISPLAY_VERSION` beta version.
* Confirm the CLI usage output and Qt `About` dialog show that same version.
* Confirm the Qt GUI is the beta-facing UI.
* Confirm known limitations are documented in `README.md` and
  `docs/qt-user-guide.md`.

## Builds

Run:

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build-qt-prebuilt --config Release --target cachegui
```

The beta GUI package should be built from the prebuilt-Qt configuration. The
static vcpkg Qt build is optional release engineering coverage, not a beta gate.

## Package

From a Visual Studio developer shell, create the shared-Qt package:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.11.1\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared `
  -Zip
```

The package archive should contain `CacheExplorer.exe`, Qt DLLs, the required
Visual C++ runtime DLLs, Qt plugin folders, `README.md`, `RELEASE_NOTES.md`,
`LICENSE`, the README icon, and `docs/qt-user-guide.md`. The package script
should also include
`PACKAGE_INFO.txt` and create a `.sha256` checksum next to the zip.

Run the package smoke script:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-qt-package.ps1 `
  -PackageDir artifacts\cacheexplorer-qt-shared `
  -ZipPath artifacts\cacheexplorer-qt-shared.zip `
  -ExtractAndLaunch `
  -SmokeOpenCache C:\Path\To\texturecache
```

The smoke script isolates the packaged process from developer Qt runtime and
plugin paths. This complements, but does not replace, one launch on a separate
Windows machine without the Qt SDK before public distribution.

## GUI smoke test

Run the packaged `CacheExplorer.exe`, then complete
`docs/qt-gui-validation.md`.

Minimum beta acceptance:

* Opens a real compatible viewer texture cache without freezing.
* Table and Gallery views both work.
* Gallery thumbnails load while scrolling.
* Preview/export works for known-good entries.
* Incomplete or undecodable textures are reported as ordinary no-preview cases.
* `About` shows app, Qt, and cache diagnostics.

## Release notes

Before publishing, write short notes that include:

* This is a beta.
* Qt GUI is the supported interface.
* The Qt GUI is the only GUI target.
* Many viewer cache entries may not preview because they are incomplete.
* The app is standalone and does not depend on Firestorm.
* The package checksum.
