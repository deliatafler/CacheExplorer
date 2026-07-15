# Qt GUI validation checklist

Use this checklist after changes that affect Qt GUI state, selection, preview
workers, gallery loading, export, or cache-open behavior.

## Basic smoke test

1. Launch `build-qt/cachegui_qt/Release/CacheExplorer.exe`.
2. Click `Open` with the default Firestorm texture-cache path.
3. Confirm the table populates and the status bar reports loaded entries.
4. Select a row and click `Preview`.
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
   readable. Newly visible tiles should begin loading before far-off lookahead
   tiles.
9. Confirm `Preview` and `Try Next Preview` are hidden in `Gallery`, while
   `Export PNG`, the Gallery filter/sort controls, and the `Table` toggle remain
   visible.
10. Change the Gallery filter control and confirm the gallery shows only the
    selected preview state. Toggle back to `Table` and confirm all rows return.
11. Change the Gallery sort control and confirm the gallery reorders and resumes
    lazy thumbnail loading without blocking the UI. Slower sorts, especially
    UUID, should update the bottom status bar before the sort completes.
12. Click a Gallery item marked `No preview` or `Load failed` and confirm the
    bottom status bar reports that state.
13. Toggle back to `Table` and confirm columns render normally.
14. Select an already-previewed item in both `Table` and `Gallery`; confirm the
    cached preview appears in the large preview panel without decoding again.
15. If a preview is visible, resize the window and confirm the preview rescales.
16. Click `About` and confirm the details include the app version, Qt runtime,
    and either the open cache path or `Cache: not open`.

## Export smoke test

1. Select a row that has a successful preview in `Table`.
2. Click `Export PNG`.
3. Save to a temporary file.
4. Confirm the status bar reports success and the PNG opens in an image viewer.
5. Repeat from `Gallery` for a previewable texture.

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
