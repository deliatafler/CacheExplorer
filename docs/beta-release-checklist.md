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

The beta GUI packages should be built against prebuilt or distribution-native
Qt. The static vcpkg Qt build is optional release engineering coverage, not a
beta gate.

## Packages

Run the `Assemble Draft Release` workflow manually before tagging. It reuses
the Windows, macOS, and Ubuntu CI workflows, verifies all package jobs, gathers
the four platform packages, and creates a unified `SHA256SUMS.txt` without
publishing a GitHub Release.

The assembled assets must contain:

* Windows x64 portable ZIP.
* Apple Silicon macOS DMG.
* Ubuntu 24.04 x86-64 Debian package.
* Ubuntu 26.04 x86-64 Debian package.
* `SHA256SUMS.txt` covering those exact four files.

For a local Windows package check, create the shared-Qt package from a Visual
Studio developer shell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-qt-shared.ps1 `
  -BuildDir build-qt-prebuilt `
  -Configuration Release `
  -QtBinDir C:\Qt\6.11.1\msvc2022_64\bin `
  -OutputDir artifacts\cacheexplorer-qt-shared `
  -Zip
```

The Windows package archive should contain `CacheExplorer.exe`, Qt DLLs, the
required Visual C++ runtime DLLs, Qt plugin folders, `README.md`,
`RELEASE_NOTES.md`, `LICENSE`, the README icon, and `docs/qt-user-guide.md`.
The package script should also include `PACKAGE_INFO.txt` and create a
`.sha256` checksum next to the zip.

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

Run each packaged GUI on its target platform, then complete
`docs/qt-gui-validation.md`. At minimum, complete the full checklist on the
primary Windows package and the core open/browse/preview/export flow on macOS
and Ubuntu.

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
* The unified package checksum file.

## Draft release

After the manual assembly run and platform smoke tests pass, push the intended
`v*` version tag. The tag-triggered workflow builds the packages again and
creates a draft GitHub Release using `RELEASE_NOTES.md`; it does not publish the
release automatically. Confirm the tag, displayed application version, asset
filenames, and `SHA256SUMS.txt` before publishing the draft.

GitHub also applies commit-message CI skip markers to tag-push events. If the
tagged commit contains `[skip ci]`, manually dispatch `Assemble Draft Release`
with the version tag as its ref; the same guarded draft-release step will run.
