# CacheExplorer Qt user guide

The Qt GUI is the supported interface for CacheExplorer.

## macOS first launch

The current Apple Silicon package is ad-hoc signed for bundle integrity but is not
Developer ID signed or notarized. After downloading it, macOS may report that
CacheExplorer is damaged. If you trust the package and its published checksum,
try to open it once, then open `System Settings` > `Privacy & Security`, scroll
to `Security`, and choose `Open Anyway` for CacheExplorer. Authenticate and
confirm `Open`; macOS remembers the exception for later launches.

Do not disable Gatekeeper system-wide.

When opening an experimental Aurora cache for the first time, macOS may also
ask whether CacheExplorer may access Aurora Viewer's Keychain item. Approving
that prompt permits read-only access to the cache encryption key; CacheExplorer
does not create, change, or delete Keychain items.

## Open a cache

CacheExplorer reads compatible Second Life viewer texture caches directly. It is
verified with the official Second Life viewer and Firestorm Viewer. You can select
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

Click `Choose...` to select either folder; the app opens the selected
cache immediately. On first use, when no remembered or automatically discovered
cache path is available, the folder chooser starts in the platform's normal
per-user cache root: local application data on Windows and the user cache
directory on macOS or Linux. The home button beside the path box opens the first existing default
cache in this order: official Second Life, official Firestorm, then a self-built
Firestorm cache, followed by Aurora's current or default cache. Aurora's
`cache_location.txt` marker is honored when the viewer uses a custom cache
location. The button is hidden when none of those caches exist.
CacheExplorer remembers the last successfully opened cache path while it still
exists, and you can still edit the path box and click `Open` for unusual
installations. Once the path matches the currently open cache, `Open` changes
to `Refresh`. If a cache opens successfully, the status bar reports the number
of valid texture entries and the cache version.

Click `Refresh` for the currently displayed path to reread the cache. This is
useful while a viewer is running and adding or replacing cached textures. The
status bar reports how many entries were added or updated since the previous
successful read of that cache, and the Gallery's `Recent changes` option shows
that set. Opening a different cache establishes a new baseline.

After you open more than one cache, the `Recent` menu beside the path field
lists up to eight existing cache folders. Choose a path to reopen it immediately
or use `Clear recent caches` to remove the remembered list.

### Experimental Aurora caches

CacheExplorer can also open Aurora Viewer's cache root, `store` directory, or
direct `store/tex` directory. Aurora support remains experimental because its
format is evolving. Aurora filenames are keyed identifiers and cannot be
reversed into asset UUIDs, so enumerated entries may show an opaque identifier;
`Find UUID` still works when you already know the original UUID.

Aurora encrypts its cache. CacheExplorer reads the existing installation key
from Windows Credential Manager, macOS Keychain, or Secret Service on Linux.
If Aurora could not use the native service, CacheExplorer also recognizes
Aurora's private `cache.key` fallback in its platform configuration directory.
The key stays in memory and is never displayed or copied. On Linux, a desktop
Secret Service such as GNOME Keyring must be available unless Aurora itself is
using that fallback file.

The default Aurora cache roots are:

* `%LOCALAPPDATA%\Aurora\AuroraViewer\cache` on Windows
* `~/Library/Caches/org.Aurora.AuroraViewer` on macOS
* `$XDG_CACHE_HOME/auroraviewer`, normally `~/.cache/auroraviewer`, on Linux

If Aurora uses a custom location, CacheExplorer reads Aurora's current-cache
marker and resolves the nested `AuroraViewerCache` directory automatically.

## Browse textures

CacheExplorer opens in `Gallery` for normal browsing. Gallery thumbnails load lazily as you scroll.
Some entries will remain `No preview` or `Load failed`; this is normal for real
viewer caches because many cached JPEG2000 textures are incomplete or
progressively cached.

CacheExplorer remembers terminal `No preview` and `Load failed` states for a
cache between runs, while still checking the entry metadata before reusing that
state.

The Gallery `Show` control offers `Everything`, `Images only`, and `Recent
changes`. In `Images only`, unchecked visible entries briefly remain as quiet
placeholders while CacheExplorer attempts their thumbnails. Successful images
stay in the Gallery; entries that cannot produce a thumbnail are removed, and
newly exposed entries are checked until the visible area is filled or the end
of the cache is reached. This work remains demand-driven rather than scanning
the entire cache up front. `Recent changes` is populated only after refreshing
the currently open cache and includes entries whose cache metadata was added or
updated since the previous read. Table mode continues to show every entry.
`Recent changes` is currently available for legacy Second Life/Firestorm
caches; Aurora's opaque keyed catalog does not expose the same cache-index
change model, so that option is omitted for Aurora.

Use `Table` for debugging and inspection. The table shows UUID, image size, body
size, raw cache index, timestamp, and preview status. Table sorting is useful
when investigating cache layout or looking for large entries.
For Aurora caches, the table instead labels the opaque identifier as `Cache
key`, shows its cached data size, and omits the legacy-only body and cache-index
columns. Gallery sorting likewise offers only Newest, Oldest, Largest cache
item, and Cache key for Aurora.

Both views support Ctrl-click and Shift-click multi-selection. Selected entries
remain selected when switching between Gallery and Table; the current entry is
still the one shown in the large preview panel.

Drag the divider beside the preview panel to give the browser or the selected
texture more room. CacheExplorer remembers the window position, window size,
and divider position between launches.

The common Open, Refresh, Find, and Save As keyboard shortcuts choose a cache,
refresh the displayed cache, focus UUID search, and export the current
selection respectively. Button tooltips show the platform-specific key
sequence.

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

Below the large preview, CacheExplorer shows the full selectable identifier, decoded
dimensions when known, cached-size completeness, source-format information when
available, and the cache timestamp. Experimental Aurora entries identify
whether the cache contains complete or partial JPEG2000 data, BC7 data, or a
combination; `BC7 only` is the direct way to identify a BC7-only texture. Use
`Copy UUID` to place the selected texture UUID on the clipboard when one is
available. Aurora's enumerated cache keys cannot be reversed into UUIDs, so
that command is hidden for Aurora caches; known UUIDs remain searchable through
`Find UUID`.

If a preview is available, click `Export PNG` and choose an output file.
Successful exports report the PNG path, decoded dimensions, and cached byte
count in the status bar. If the cached texture cannot be decoded, export reports
that the cached data is incomplete or undecodable. CacheExplorer remembers the
last accepted export folder for both single and multiple exports. Before a
folder has been chosen, export starts in the platform's Pictures directory when
available, then falls back to Documents or the home directory.

When multiple entries are selected, the command becomes `Export Selected
PNGs...`. Choose an output folder and CacheExplorer exports the selected entries
in the background using their UUIDs as filenames. Existing PNGs are left in
place; the final status reports exported, incomplete, existing, and failed
counts.

## Diagnostics

Click the information button to view the app version, Qt runtime/build versions,
cache diagnostics, and the current session's Gallery thumbnail counts and
single-worker decode rate. This information is useful in bug reports and
performance comparisons.

## Limitations

* Many cache entries are expected to be incomplete or undecodable.
* CacheExplorer does not repair or modify the viewer cache.
* CacheExplorer does not require Firestorm source code and does not link against
  Firestorm.

## Trademarks

Second Life® is a registered trademark of Linden Research, Inc. Firestorm Viewer
is a trademark of The Phoenix Firestorm Project, Inc. CacheExplorer is not
affiliated with, sponsored by, or endorsed by Linden Research, Inc. or The
Phoenix Firestorm Project, Inc.
