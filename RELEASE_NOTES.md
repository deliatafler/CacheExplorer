# CacheExplorer Release Notes

## 0.1.0-beta

CacheExplorer 0.1.0-beta is the first public test build candidate of the Qt GUI.

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
* Windows is distributed as a portable shared-Qt ZIP. Ubuntu uses
  release-specific `.deb` packages for 24.04 and 26.04. Apple Silicon macOS
  uses a DMG containing a native application bundle.
* The current macOS package is unsigned and unnotarized, so Gatekeeper may
  require an explicit right-click and Open during beta testing.
* Static/single-file distribution remains future work.
* CacheExplorer reads and exports cache data; it does not repair or modify the
  viewer cache.
* CacheExplorer is standalone and does not depend on or link against Firestorm.

Report beta problems through
[GitHub Issues](https://github.com/deliatafler/CacheExplorer/issues). Include
the GUI's `About` diagnostics when possible.

### Packages

The beta release should include:

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
