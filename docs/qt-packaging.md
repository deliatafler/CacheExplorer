# Qt GUI packaging notes

The preferred packaging path for day-to-day testing is a shared-Qt folder
created from an official/prebuilt Qt installation. This produces a directory
that can be zipped and shared for smoke testing without rebuilding Qt locally.

For a single static binary or a more reproducible release experiment later, keep
the optional vcpkg static Qt path described in `docs/qt-build.md`.

## macOS application and DMG

macOS builds produce a native `CacheExplorer.app` bundle. The repository keeps
the approved 1024-pixel PNG as the portable icon source and generates the
platform-specific `.icns` file on macOS before CMake configure:

```bash
bash scripts/generate-macos-icon.sh
```

Configure against a prebuilt Qt SDK and vcpkg's native Apple Silicon triplet,
then build and test:

```bash
cmake -S . -B build-macos \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON \
  -DCACHEEXPLORER_STATIC_MSVC_RUNTIME=OFF

cmake --build build-macos --parallel 3
ctest --test-dir build-macos --output-on-failure
```

Create and validate the disk image:

```bash
cpack --config build-macos/CPackConfig.cmake -G DragNDrop -B artifacts
bash scripts/test-qt-macos-package.sh \
  artifacts/CacheExplorer-0.1.0-beta.2-macOS-arm64.dmg
```

The DMG contains `CacheExplorer.app`, its app-local Qt frameworks and Cocoa
platform plugin, plus `README.md`, `RELEASE_NOTES.md`, `LICENSE`, and the Qt
user guide. The package test mounts the image read-only, verifies those files,
checks bundle metadata and ARM64 architecture, confirms the executable resolves
Qt through bundled `@rpath` frameworks, and rejects an unexpected Qt Network
framework. It asserts that the executable's `LC_RPATH` includes the app-local
`@executable_path/../Frameworks` directory, verifies the complete app bundle's
ad-hoc code-signature integrity with `codesign --verify --deep --strict`,
reports its deployment target, and starts the packaged executable with
`--version` to exercise the dynamic loader, Qt runtime, and Cocoa platform
plugin.

After Qt deployment, CacheExplorer explicitly ad-hoc signs the complete app
bundle before CPack creates the DMG. This preserves bundle integrity after
`macdeployqt` adds the app-local frameworks and plugins. The package is not
signed with an Apple-issued Developer ID certificate or notarized, so it is a
CI and physical-Mac test artifact rather than a polished public macOS release.
Developer ID signing and Apple notarization require project-owned Apple
credentials and should be added as a separate protected release workflow step.

After downloading the current DMG, macOS Gatekeeper may report that
CacheExplorer is damaged even when its checksum and code-signature integrity
are valid. To authorize a trusted test build using Apple's supported override:

1. Try to open CacheExplorer once and dismiss the warning.
2. Open `System Settings`, select `Privacy & Security`, and scroll to
   `Security`.
3. Click `Open Anyway` for CacheExplorer, authenticate, and confirm `Open`.

The override is intentionally per application. Do not disable Gatekeeper
system-wide.

## Ubuntu Debian package

Linux builds use the distribution's shared Qt runtime and produce a native
x86-64 `.deb`. CI currently emits separate Ubuntu 24.04 and 26.04 packages so
each package carries dependency names generated against that release's own Qt
repositories. OpenJPEG and libpng remain statically linked from vcpkg. CPack
derives the executable's normal shared-library dependencies and explicitly
adds `qt6-qpa-plugins`, which provides the dynamically loaded Qt XCB platform
plugin needed on a typical Ubuntu desktop.

Configure, build, test, and package on Ubuntu:

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DCACHEEXPLORER_BUILD_QT_GUI=ON \
  -DCACHEEXPLORER_STATIC_MSVC_RUNTIME=OFF \
  -DCACHEEXPLORER_LINUX_PACKAGE_TAG=Ubuntu-26.04

cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
cpack --config build-linux/CPackConfig.cmake -G DEB -B artifacts
```

Validate and install the resulting package:

```bash
bash scripts/test-qt-linux-package.sh \
  artifacts/CacheExplorer-0.1.0-beta.2-Ubuntu-26.04-x86_64.deb

sudo apt install ./artifacts/CacheExplorer-0.1.0-beta.2-Ubuntu-26.04-x86_64.deb
```

The package installs the executable in `/usr/bin`, a desktop launcher in
`/usr/share/applications`, the icon in `/usr/share/pixmaps`, and project/user
documentation in `/usr/share/doc/cacheexplorer`. It relies on Ubuntu's normal
package manager for Qt runtime libraries rather than copying a second Qt stack
into the application package.

## Shared Qt package

### Windows installer

The recommended Windows end-user package is an unsigned CPack/NSIS installer.
It installs the application under Program Files, adds a Start menu shortcut,
and registers CacheExplorer in Windows Apps & features with a standard
uninstaller. The portable ZIP remains available alongside it.

Install NSIS 3.03 or newer, configure and build the preferred prebuilt-Qt
Release target, then run:

```powershell
cpack --config build-qt-prebuilt/CPackConfig.cmake `
  -C Release `
  -G NSIS `
  -B artifacts
```

CMake's install rules run Qt's deployment helper, omit translations and the
unused generic/network plugin types, and place the required Visual C++ runtime
DLLs app-locally. The generated installer and `.sha256` checksum are written to
`artifacts`.

On a machine without an existing CacheExplorer installation, exercise the
complete install/launch/uninstall lifecycle with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-windows-installer.ps1 `
  -InstallerPath artifacts\CacheExplorer-0.1.0-beta.2-Windows-x64-Setup.exe
```

The beta installer is unsigned, so Windows may show an unknown-publisher or
SmartScreen warning. Code signing is a later release-hardening item and should
use protected project credentials in the release workflow.

### Portable ZIP

Prerequisites:

* A Release `cachegui` build configured against a prebuilt shared Qt
  installation. Prefer `scripts/configure-qt-prebuilt.ps1` for this configure.
* `windeployqt.exe` from that same Qt installation, either on `PATH` or supplied
  with `-QtBinDir`.
* The x64 Visual C++ redistributable installed with Visual Studio or the C++
  Build Tools. The script discovers installed Visual Studio versions
  automatically; use `-VCRedistDir` only when it lives in an unusual location.

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
copied by `windeployqt`, app-local Visual C++ runtime DLLs, `README.md`,
`RELEASE_NOTES.md`, `LICENSE`, the CacheExplorer icon used by the packaged
README, and `docs/qt-user-guide.md`. It also writes
`PACKAGE_INFO.txt` with the package version and build/deployment details for
beta support reports. With `-Zip`, the
script also creates
`artifacts/cacheexplorer-qt-shared.zip` unless `-ZipPath` is supplied. It also
writes a `.sha256` checksum file next to the zip unless `-NoChecksum` is used.

The package excludes Qt's generic TUIO touch plugin. CacheExplorer does not use
networking, and that optional plugin would otherwise pull `Qt6Network.dll` plus
network-information and TLS plugins into the package transitively.

## Smoke test checklist

Run the packaged executable from inside the package directory, then:

* Open a real compatible viewer `texturecache` folder.
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
  -ExtractAndLaunch
```

`-ExtractAndLaunch` expands the ZIP into a newly created temporary directory,
checks the deployed files again, launches the extracted executable briefly, and
removes that temporary directory. It is the preferred beta-release smoke check
because it avoids relying on the build tree or local Qt installation. The child
process receives an isolated `PATH` containing only the extracted package and
Windows system directories, with Qt plugin/import overrides cleared, so an
installed Qt SDK cannot satisfy missing package runtime files accidentally.

To also exercise the packaged GUI's cache-open/model-population path against a
real cache, add `-SmokeOpenCache` with the cache directory:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-qt-package.ps1 `
  -PackageDir artifacts\cacheexplorer-qt-shared `
  -ZipPath artifacts\cacheexplorer-qt-shared.zip `
  -ExtractAndLaunch `
  -SmokeOpenCache C:\Path\To\texturecache
```

This uses the GUI-only `--smoke-open` switch, which opens the cache through the
normal main-window path, verifies the entry model was populated, and exits.
The isolated launch is strong automated deployment coverage, but the final beta
check on a separate Windows machine without a developer Qt SDK is still useful.

For developer builds that have not been packaged with `windeployqt`, launch
through `scripts/launch-qt-prebuilt.ps1` so the official Qt `bin` directory is
on `PATH`.

## Optional static release path

The shared package is the current beta packaging path. Static Qt distribution
experiments can use the vcpkg `qt-gui` feature and `x64-windows-static` triplet
from `docs/qt-build.md`, but contributors should not need to build Qt this way.

Do not mix a dynamic official Qt runtime with a static-vcpkg dependency graph
for a final release package unless the runtime model is deliberately made
consistent.

## Troubleshooting

### Ubuntu local-package notice

APT may finish a successful local `.deb` installation with a notice like:

```text
Download is performed unsandboxed as root ... couldn't be accessed by user '_apt'.
```

This happens when the `_apt` sandbox user cannot traverse a private home or
Downloads directory. APT falls back to reading that already-local package as
root; it does not indicate a broken CacheExplorer package, failed dependency
check, or incomplete installation. To avoid the notice without relaxing home
directory permissions, copy the package to `/tmp` first:

```bash
cp ~/Downloads/CacheExplorer-0.1.0-beta.2-Ubuntu-26.04-x86_64.deb \
  /tmp/cacheexplorer.deb
sudo apt install /tmp/cacheexplorer.deb
rm /tmp/cacheexplorer.deb
```

### Windows deployment

If the app reports a missing Qt platform plugin, rerun `windeployqt` from the Qt
installation used during CMake configure.

If the package launches only on the build machine, confirm that the package
contains the Visual C++ runtime DLLs and that `PACKAGE_INFO.txt` identifies the
expected runtime directory.

If the wrong Qt version is deployed, inspect `CMAKE_PREFIX_PATH` or `Qt6_DIR` in
the CMake cache for the build directory being packaged.
