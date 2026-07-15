# CacheExplorer Qt user guide

The Qt GUI is the supported beta interface for CacheExplorer.

## Open a cache

CacheExplorer reads Firestorm texture caches directly. You can select either:

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

Click `Open` to load the cache shown in the path box. If a cache opens
successfully, the status bar reports the number of valid texture entries and the
cache version.

## Browse textures

Use `Gallery` for normal browsing. Gallery thumbnails load lazily as you scroll.
Some entries will remain `No preview` or `Load failed`; this is normal for real
Firestorm caches because many cached JPEG2000 textures are incomplete or
progressively cached.

Use `Table` for debugging and inspection. The table shows UUID, image size, body
size, raw cache index, timestamp, and preview status. Table sorting is useful
when investigating cache layout or looking for large entries.

## Preview and export

In `Table`, select an entry and click `Preview` to decode it into the large
preview panel. `Try Next Preview` scans forward through the current table order
until it finds a previewable entry or exhausts the search.

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
* CacheExplorer does not repair or modify the Firestorm cache.
* CacheExplorer does not require Firestorm source code and does not link against
  Firestorm.
