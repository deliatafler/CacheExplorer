# CacheExplorer Qt user guide

The Qt GUI is the supported beta interface for CacheExplorer.

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
cache immediately. The home button beside the path box opens the usual
Firestorm cache location when it is available. CacheExplorer remembers the last
successfully opened cache path, and you can still edit the path box and click
`Open` for unusual installations. If a cache opens successfully, the status bar
reports the number of valid texture entries and the cache version.

## Browse textures

CacheExplorer opens in `Gallery` for normal browsing. Gallery thumbnails load lazily as you scroll.
Some entries will remain `No preview` or `Load failed`; this is normal for real
viewer caches because many cached JPEG2000 textures are incomplete or
progressively cached.

CacheExplorer remembers terminal `No preview` and `Load failed` states for a
cache between runs, while still checking the entry metadata before reusing that
state.

The Gallery `Show` filter includes `Cached complete`, which shows entries whose
stored header and body bytes cover the advertised image size. This can include
small header-only entries with body size `0`; it is a useful browsing/debugging
hint, not a guarantee that the JPEG2000 decoder will accept every entry. The
other filters focus on unknown, no-preview, and load-failed entries.

Use `Table` for debugging and inspection. The table shows UUID, image size, body
size, raw cache index, timestamp, and preview status. Table sorting is useful
when investigating cache layout or looking for large entries.

## Preview and export

In `Table`, selecting an entry loads its preview into the large preview panel.
Cached previews appear immediately; new selections wait briefly so fast keyboard
or mouse navigation does not start unnecessary decodes. Incomplete or
undecodable entries report their no-preview state in the panel and status bar.
`Try Next Preview` scans forward through the current table order until it finds
a previewable entry or exhausts the search.

In `Gallery`, select a thumbnail tile to show its cached preview in the large
preview panel. Manual preview buttons are hidden because Gallery loads
thumbnails itself.

If a preview is available, click `Export PNG` and choose an output file.
Successful exports report the PNG path, decoded dimensions, and cached byte
count in the status bar. If the cached texture cannot be decoded, export reports
that the cached data is incomplete or undecodable.

## Diagnostics

Click `About` to view the app version, Qt runtime/build versions, and cache
diagnostics. This information is useful in beta bug reports.

## Limitations

* Many cache entries are expected to be incomplete or undecodable.
* CacheExplorer does not repair or modify the viewer cache.
* CacheExplorer does not require Firestorm source code and does not link against
  Firestorm.
