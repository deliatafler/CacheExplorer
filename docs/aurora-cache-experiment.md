# Aurora cache support experiment

This document tracks the experimental work to read Aurora Viewer texture
caches without changing the proven Second Life/Firestorm cache path.

The experiment starts from Aurora Viewer source revision
`994bc53afd5ab024f7e05abbab77f2c8b2dd5325`, inspected on 2026-11-17.
Aurora is under active 0.x development, so its cache format must be treated as
versioned and subject to change.

## Goals

* Keep the existing Second Life/Firestorm reader and GUI behavior unchanged.
* Add read-only browsing and PNG export for Aurora texture caches.
* Reuse the existing JPEG2000 decoder and PNG writer after Aurora data has
  been opened.
* Support anonymous Gallery browsing even when an Aurora filename cannot be
  reversed to its original asset UUID.
* Preserve Find UUID by deriving the keyed Aurora filename from a UUID supplied
  by the user.
* Keep the eventual implementation cross-platform on Windows, macOS, and
  Linux.

## Non-goals

* CacheExplorer must never write to, repair, rename, or remove Aurora cache
  files.
* CacheExplorer will not link against Aurora Viewer or invoke it as a required
  helper.
* The experiment will not copy GPL-licensed Aurora source into the MIT-licensed
  CacheExplorer codebase.
* The one-way keyed filenames will not be presented as recoverable UUIDs.

## Observed Aurora layout

The default Windows texture store is:

```text
%LOCALAPPDATA%\Aurora\AuroraViewer\cache\store\tex
```

Aurora's `directories` crate selects corresponding platform cache roots on
macOS and Linux. A user-selected cache root contains an `AuroraViewerCache`
directory before the `store` directory.

The texture store contains one or more files per keyed texture name:

* `.j2c`: complete JPEG2000 bytes
* `.part`: progressively cached JPEG2000 bytes
* `.bc7`: Aurora's `AUTX` transcoded BC7 mip-chain format

All three are wrapped in Aurora's `AUSL` sealed-file format. The current format
uses AES-256-GCM in 64 KiB chunks, a per-file HKDF-SHA256 key, and an
installation key stored in the operating system credential store. Filenames
are the first 128 bits of an HMAC-SHA256 over the asset UUID under a separate
derived naming key.

The installed Windows cache inspected during research contained 104,523
distinct keyed textures: 18,401 complete J2C files, 85,989 partial files, and
81,848 BC7 files.

## Important limitation

An HMAC filename cannot be reversed to recover the UUID. With the installation
key, CacheExplorer can calculate the filename for a UUID supplied by the user,
but enumeration alone yields only an opaque stable cache identifier.

Aurora Gallery entries therefore need to support an absent UUID. Until Aurora
provides a manifest or interoperability API, anonymous exports should use the
opaque cache identifier as their filename.

## Implementation tranches

### 1. Backend boundary

Introduce a cache-source abstraction in `cachelib` and adapt the current
Second Life/Firestorm implementation to it. The GUI should consume neutral
catalog entries rather than depend directly on legacy `cacheIndex`,
`imageSize`, and `bodySize` fields.

Acceptance criteria:

* All existing cachelib and cachegui tests pass unchanged or with equivalent
  assertions.
* A real Second Life/Firestorm cache behaves exactly as it does in 0.1.0.
* No Aurora dependency, cryptography, or credential access is introduced yet.

Status: complete on the experiment branch. The Qt catalog model, selection,
Gallery queue, asynchronous preview workers, and PNG export use the neutral
source contract. Legacy refresh diagnostics and persisted preview-state
validation retain a narrow compatibility view of `TextureCacheDatabase`.

### 2. Aurora format fixtures and probing

Add synthetic `AUSL`, keyed-name, and `AUTX` fixtures generated solely for
CacheExplorer tests. Add cache-folder probing and enumeration of Aurora texture
files without opening their payloads.

Acceptance criteria:

* Aurora roots and `store/tex` directories are recognized without weakening
  legacy path validation.
* Unknown sealed-file and transcoded-texture versions fail clearly.
* Concurrent removal or replacement of cache files is treated as a normal
  cache miss.

### 3. Windows JPEG2000 proof of concept

Read the Aurora cache installation key through Windows Credential Manager,
open `.j2c` and `.part` files, and feed their plaintext bytes to the existing
OpenJPEG decoder. Keep the key in memory only and never log it.

Acceptance criteria:

* Anonymous Gallery thumbnails and PNG export work against the installed
  Aurora cache.
* Find UUID calculates the keyed name and locates matching files.
* Aurora Viewer can remain open while CacheExplorer reads and refreshes.

### 4. BC7 thumbnails

Parse supported `AUTX` versions and decode a suitable cached mip level to RGBA.
This is expected to provide previews for textures whose JPEG2000 cache is only
partial or no longer present.

### 5. Cross-platform credential providers

Add macOS Keychain and Linux Secret Service access with the same in-memory-only
key handling. Package dependencies and first-access prompts must be documented
and tested on physical or desktop-VM systems.

### 6. Product integration

Polish cache discovery, format-specific metadata, anonymous identifiers,
diagnostics, release documentation, and the full GUI validation matrix. Aurora
support remains experimental until its cache-format compatibility and upgrade
behavior have been tested across multiple Aurora releases.

## Questions for Aurora maintainers

Development should not block on answers, but cooperation could improve the
result substantially:

* Is the `AUSL` v1 sealed-file format intended to remain readable by external
  tools?
* Is there or could there be a read-only UUID-to-keyed-name manifest for the
  texture store?
* Would the project consider a texture-only interoperability key or export
  interface so an external viewer need not read the installation-wide cache
  key?
* How should external tools identify custom cache locations on each platform?
* Are `AUTX` versions expected to retain backward readers or be discarded and
  rebuilt freely?

