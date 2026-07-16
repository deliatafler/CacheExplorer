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
* The shared Qt zip is the practical beta distribution path. Static/single-file
  distribution remains future work.
* CacheExplorer reads and exports cache data; it does not repair or modify the
  viewer cache.
* CacheExplorer is standalone and does not depend on or link against Firestorm.

Report beta problems through
[GitHub Issues](https://github.com/deliatafler/CacheExplorer/issues). Include
the GUI's `About` diagnostics when possible.

### Packaging

The beta package should include:

* `CacheExplorer.exe`
* Qt runtime DLLs and plugin folders from `windeployqt`
* `README.md`, `RELEASE_NOTES.md`, `LICENSE`, and `docs/qt-user-guide.md`
* `PACKAGE_INFO.txt` with version and build/deployment details
* a `.zip` archive
* a `.sha256` checksum file for the archive

Generate the shared Qt package from a Visual Studio developer shell using
`scripts/package-qt-shared.ps1 -Zip`.

Each beta archive has a matching `.sha256` file generated alongside it. Publish
that checksum with the exact archive being shared; it is intentionally not
embedded here so these notes remain valid for rebuilt beta candidates.
