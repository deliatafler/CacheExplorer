# CacheExplorer Release Notes

## 0.1.0-beta.3

CacheExplorer 0.1.0-beta.3 completes the first physical-Mac validation pass,
repairs macOS package launch behavior, and smooths Gallery loading feedback.

### Changes since beta.2

* Fixed the macOS application bundle's Qt framework runpath so the installed
  app can locate its bundled Qt libraries at launch.
* Ad-hoc signed the fully deployed macOS bundle and expanded package validation
  to check its final signature, app-local framework runpath, deployment target,
  and an actual mounted-DMG startup.
* Validated launch, real-cache Gallery browsing and filtering, and single and
  multi-PNG export on an Apple Silicon Mac running macOS 26.5.2.
* Changed the first macOS cache chooser location to `~/Library/Caches` and
  added discovery for common Second Life and Firestorm cache directories.
* Added measured thumbnail throughput to Gallery loading status and batched
  `Images only` removals to reduce repeated layout churn on large viewports.
* Made standard C++ exception unwinding explicit in MSVC builds, eliminating
  the previous C4530 warnings from the preferred prebuilt-Qt configuration.

### Beta limitations

* Many real viewer cache entries are incomplete or progressively cached and
  will not preview. These are expected no-preview cases, not app failures.
* The Windows installer is unsigned, so Windows may show an unknown-publisher
  or SmartScreen warning.
* The macOS package is ad-hoc signed for integrity but is not Developer ID
  signed or notarized. Gatekeeper therefore requires the per-application
  `Open Anyway` override described in `docs/qt-packaging.md`.
* Static/single-file distribution remains future work.
* CacheExplorer reads and exports cache data; it does not repair or modify the
  viewer cache.

### Packages

The beta.3 release includes:

* Windows x64 installer with app-local Qt and Visual C++ runtime files.
* Windows x64 portable ZIP with the same runtime model.
* Apple Silicon macOS DMG with app-local Qt frameworks.
* Ubuntu 24.04 and 26.04 x86-64 Debian packages.
* `SHA256SUMS.txt` covering all five platform packages.

## 0.1.0-beta.2

CacheExplorer 0.1.0-beta.2 promotes the Windows installer to the recommended
Windows beta package and includes the latest preview-panel improvements.

### Changes since beta.1

* Added a conventional Windows installer with license, Start menu, upgrade, and
  uninstall support. The portable ZIP remains available.
* Added compact selected-texture dimensions, cache completeness, timestamp,
  and one-click UUID copying below the large preview.
* Added automated Windows installer install/launch/uninstall coverage.

### Beta limitations

* Many real viewer cache entries are incomplete or progressively cached and
  will not preview. These are expected no-preview cases, not app failures.
* The Windows installer is unsigned, so Windows may show an unknown-publisher
  or SmartScreen warning.
* The macOS package is unsigned and unnotarized. Physical-Mac validation remains
  deferred and is not a beta.2 release gate.
* Static/single-file distribution remains future work.
* CacheExplorer reads and exports cache data; it does not repair or modify the
  viewer cache.

### Packages

The beta.2 release includes:

* Windows x64 installer with app-local Qt and Visual C++ runtime files.
* Windows x64 portable ZIP with the same runtime model.
* Apple Silicon macOS DMG with app-local Qt frameworks.
* Ubuntu 24.04 and 26.04 x86-64 Debian packages.
* `SHA256SUMS.txt` covering all five platform packages.

## 0.1.0-beta.1

CacheExplorer 0.1.0-beta.1 is the first cross-platform test build candidate of
the Qt GUI.

### Highlights

* Standalone Second Life viewer texture-cache browser and PNG exporter.
* Verified with the official Second Life viewer and Firestorm.
* Qt 6 GUI is the supported beta interface.
* Gallery view lazily loads texture previews while scrolling.
* Table view remains available for debugging cache metadata.
* Preview and PNG export are available for decodable cached textures.
* `About` shows app, Qt, and cache diagnostics for beta reports.
* A new CacheExplorer icon identifies the application and packaged executable.
* CLI remains available for scan, list, verify, stats, and export workflows.

### Beta limitations

* Many real viewer cache entries are incomplete or progressively cached and
  will not preview. These are expected no-preview cases, not app failures.
* Windows beta.1 is distributed as a portable shared-Qt ZIP. Ubuntu uses
  release-specific `.deb` packages for 24.04 and 26.04. Apple Silicon macOS
  uses a DMG containing a native application bundle.
* The current macOS package is unsigned and unnotarized, so Gatekeeper may
  require an explicit right-click and Open during beta testing.
* The macOS DMG passes automated bundle and deployment checks, but physical-Mac
  validation is deferred and is not a beta.1 release gate.
* Static/single-file distribution remains future work.
* CacheExplorer reads and exports cache data; it does not repair or modify the
  viewer cache.
* CacheExplorer is standalone and does not depend on or link against Firestorm.

Report beta problems through
[GitHub Issues](https://github.com/deliatafler/CacheExplorer/issues). Include
the GUI's `About` diagnostics when possible.

### Packages

The beta.1 release included:

* Windows x64 portable ZIP with Qt and Visual C++ runtime files.
* Apple Silicon macOS DMG with app-local Qt frameworks.
* Ubuntu 24.04 and 26.04 x86-64 Debian packages.
* `SHA256SUMS.txt` covering all four platform packages.

Pushing a version tag matching `v*` runs the reusable platform CI workflows and
creates a draft GitHub Release. A manual `Assemble Draft Release` workflow run
builds and checks the same complete asset set without creating a release.

Publish the generated `SHA256SUMS.txt` with the exact packages being shared; its
hashes are intentionally not embedded here so these notes remain valid for
rebuilt beta candidates.
