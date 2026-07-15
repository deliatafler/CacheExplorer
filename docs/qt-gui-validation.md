# Qt GUI validation checklist

Use this checklist after changes that affect Qt GUI state, selection, preview
workers, gallery loading, export, or cache-open behavior.

Before the interactive checks, the packaged app can exercise its normal
cache-open/model-population path unattended:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-qt-package.ps1 `
  -PackageDir artifacts\cacheexplorer-qt-shared `
  -ZipPath artifacts\cacheexplorer-qt-shared.zip `
  -ExtractAndLaunch `
  -SmokeOpenCache C:\Path\To\texturecache
```

This complements, but does not replace, the interactive Gallery, Preview, and
export checks below.

## Basic smoke test

1. Launch the prebuilt Qt GUI with:
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/launch-qt-prebuilt.ps1 `
     -QtDir C:\Qt\6.11.1\msvc2022_64
   ```
2. Open a known-compatible viewer texture cache, such as the default Firestorm
   cache path or an official Second Life viewer cache.
3. Confirm Gallery is the initial view and the status bar reports loaded entries.
4. Switch to `Table`, then select a row and wait briefly for its preview to load.
5. Confirm one of these expected outcomes:
   * A preview appears and the status bar reports image dimensions.
   * The row is marked `No preview` or `Load failed` without freezing.
   * Incomplete/undecodable cached data shows an incomplete-preview message in
     the preview panel and status bar.
6. Click `Try Next Preview`.
7. Confirm scanning follows the current table/gallery sort order and stops on a
   previewable entry or reports no previewable entry in the bounded scan.
8. Toggle `Gallery`, scroll several pages, and confirm placeholders/previews load
   without blocking the UI. The activity label may briefly report visible
   thumbnail loading or refreshing. Waiting/loading placeholders should stay
   visually quiet while scrolling, while no-preview and error labels should be
   readable. The grid should fill rows from left to right and use the available
   browsing width. Newly visible tiles should begin loading before far-off
   lookahead tiles.
9. Confirm `Try Next Preview` is hidden in `Gallery`, while
   `Export PNG`, the Gallery filter/sort controls, and the `Table` toggle remain
   visible.
10. Change the Gallery filter control and confirm the gallery shows the
    selected cache/preview state, the Gallery count updates, and empty filters
    report no matching entries in the status bar. Confirm `Cached complete`
    includes entries whose cached header/body bytes cover the advertised image
    size. Toggle back to `Table` and confirm all rows return.
11. Change the Gallery sort control and confirm the gallery reorders and resumes
    lazy thumbnail loading without blocking the UI. Slower sorts, especially
    UUID, should update the bottom status bar before the sort completes.
12. Click a Gallery item marked `No preview` or `Load failed` and confirm the
    bottom status bar and large preview panel report that state.
13. Toggle back to `Table` and confirm columns render normally.
14. Select an already-previewed item in both `Table` and `Gallery`; confirm the
    cached preview appears in the large preview panel without decoding again.
15. If a preview is visible, resize the window and confirm the preview rescales.
16. Click `About` and confirm the details include the app version, Qt runtime,
    and either the open cache path or `Cache: not open`.
17. Ctrl-click or Shift-click multiple entries in both views, switch views, and
    confirm the selection remains. Confirm the current item still drives the
    large preview panel.

## Export smoke test

1. Select a row that has a successful preview in `Table`.
2. Click `Export PNG`.
3. Save to a temporary file.
4. Confirm the status bar reports success with image dimensions and cached byte
   count, and the PNG opens in an image viewer.
5. Repeat from `Gallery` for a previewable texture.
6. Select multiple entries, click `Export Selected PNGs...`, choose a temporary
   folder, and confirm the UI stays responsive while the final status reports
   exported, incomplete, existing, and failed counts.

## When a build-only check is enough

A build-only check is usually enough for narrow helper moves that do not change
Qt signals, selection, model/proxy use, async worker state, preview cache state,
or widget rendering.

## When GUI validation is required

Run at least the basic smoke test after changes involving:

* `MainWindow` preview or gallery orchestration
* `PreviewCache`, `PreviewPanel`, or `TryNextPreviewState`
* `GalleryPreviewController`, `GalleryPreviewQueue`, or `GalleryPreviewScanner`
* Gallery filtering or sorting behavior
* table/gallery model roles or selection behavior
* cache open/close behavior
* PNG export behavior
* Qt build/deployment options
