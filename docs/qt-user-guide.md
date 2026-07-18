# CacheExplorer Qt user guide

The Qt GUI is the supported beta interface for CacheExplorer.

## macOS first launch

The current Apple Silicon beta is ad-hoc signed for bundle integrity but is not
Developer ID signed or notarized. After downloading it, macOS may report that
CacheExplorer is damaged. If you trust the package and its published checksum,
try to open it once, then open `System Settings` > `Privacy & Security`, scroll
to `Security`, and choose `Open Anyway` for CacheExplorer. Authenticate and
confirm `Open`; macOS remembers the exception for later launches.

Do not disable Gatekeeper system-wide.

## Open a cache

CacheExplorer reads compatible Second Life viewer texture caches directly. It is
verified with the official Second Life viewer and Firestorm. You can select
either:

* the `texturecache` folder itself, or
* the parent Firestorm cache/profile folder that contains `texturecache`.

The expected cache files are:

```text
texture.entries
texture.cache
FastCache.cache
0/
1/
...
f/
```

Click `Choose Folder...` to select either folder; the app opens the selected
cache immediately. On first use, when no remembered or automatically discovered
cache path is available, the folder chooser starts in the platform's local
application-data directory. The home button beside the path box opens the first
existing default cache in this order: official Second Life, official Firestorm,
then a self-built Firestorm cache. It is hidden when none of those caches exist.
CacheExplorer remembers the last successfully opened cache path while it still
exists, and you can still edit the path box and click `Open` for unusual
installations. Once the path matches the currently open cache, `Open` changes
to `Refresh`. If a cache opens successfully, the status bar reports the number
of valid texture entries and the cache version.

Click `Refresh` for the currently displayed path to reread the cache. This is
useful while a viewer is running and adding or replacing cached textures.

After you open more than one cache, the `Recent` menu beside the path field
lists up to eight existing cache folders. Choose a path to reopen it immediately
or use `Clear recent caches` to remove the remembered list.

## Browse textures

CacheExplorer opens in `Gallery` for normal browsing. Gallery thumbnails load lazily as you scroll.
Some entries will remain `No preview` or `Load failed`; this is normal for real
viewer caches because many cached JPEG2000 textures are incomplete or
progressively cached.

CacheExplorer remembers terminal `No preview` and `Load failed` states for a
cache between runs, while still checking the entry metadata before reusing that
state.

The Gallery `Show` control offers `Everything` and `Images only`. In `Images
only`, unchecked visible entries briefly remain as quiet placeholders while
CacheExplorer attempts their thumbnails. Successful images stay in the Gallery;
entries that cannot produce a thumbnail are removed, and newly exposed entries
are checked until the visible area is filled or the end of the cache is reached.
This work remains demand-driven rather than scanning the entire cache up front.

Use `Table` for debugging and inspection. The table shows UUID, image size, body
size, raw cache index, timestamp, and preview status. Table sorting is useful
when investigating cache layout or looking for large entries.

Both views support Ctrl-click and Shift-click multi-selection. Selected entries
remain selected when switching between Gallery and Table; the current entry is
still the one shown in the large preview panel.

## Find a texture UUID

Enter an in-world texture UUID in `Find UUID` and click `Find` to select its
cache entry. The lookup accepts the standard hyphenated UUID form or 32
hexadecimal digits. If `Images only` hides the matching entry, CacheExplorer
changes the Gallery to `Everything` so the result can be shown.

## Preview and export

In `Table`, selecting an entry loads its preview into the large preview panel.
Cached previews appear immediately; new selections wait briefly so fast keyboard
or mouse navigation does not start unnecessary decodes. Incomplete or
undecodable entries report their no-preview state in the panel and status bar.
`Try Next Preview` scans forward through the current table order until it finds
a previewable entry or exhausts the search.

In `Gallery`, select a thumbnail tile to show its cached preview in the large
preview panel. `Try Next Preview` is hidden because Gallery loads thumbnails
itself.

Below the large preview, CacheExplorer shows the full selectable UUID, decoded
dimensions when known, cached-size completeness, and the cache timestamp. Use
`Copy UUID` to place the selected texture UUID on the clipboard.

If a preview is available, click `Export PNG` and choose an output file.
Successful exports report the PNG path, decoded dimensions, and cached byte
count in the status bar. If the cached texture cannot be decoded, export reports
that the cached data is incomplete or undecodable.

When multiple entries are selected, the command becomes `Export Selected
PNGs...`. Choose an output folder and CacheExplorer exports the selected entries
in the background using their UUIDs as filenames. Existing PNGs are left in
place; the final status reports exported, incomplete, existing, and failed
counts.

## Diagnostics

Click `About` to view the app version, Qt runtime/build versions, and cache
diagnostics. This information is useful in beta bug reports.

## Limitations

* Many cache entries are expected to be incomplete or undecodable.
* CacheExplorer does not repair or modify the viewer cache.
* CacheExplorer does not require Firestorm source code and does not link against
  Firestorm.
