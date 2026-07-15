# CacheExplorer Release Notes

## 0.1.0 beta draft

CacheExplorer 0.1.0 beta is the first planned public test build of the Qt GUI.

### Highlights

* Standalone Firestorm texture-cache browser and PNG exporter.
* Qt 6 GUI is the supported beta interface.
* Gallery view lazily loads texture previews while scrolling.
* Table view remains available for debugging cache metadata.
* Preview and PNG export are available for decodable cached textures.
* `About` shows app, Qt, and cache diagnostics for beta reports.
* CLI remains available for scan, list, verify, stats, and export workflows.

### Beta limitations

* Many real Firestorm cache entries are incomplete or progressively cached and
  will not preview. These are expected no-preview cases, not app failures.
* The shared Qt zip is the practical beta distribution path. Static/single-file
  distribution remains future work.
* The native Win32 GUI is deprecated legacy code and is not the supported beta
  interface.
* CacheExplorer reads and exports cache data; it does not repair or modify the
  Firestorm cache.
* CacheExplorer is standalone and does not depend on or link against Firestorm.

### Packaging

The beta package should include:

* `CacheExplorer.exe`
* Qt runtime DLLs and plugin folders from `windeployqt`
* a `.zip` archive
* a `.sha256` checksum file for the archive

Generate the shared Qt package from a Visual Studio developer shell using
`scripts/package-qt-shared.ps1 -Zip`.
