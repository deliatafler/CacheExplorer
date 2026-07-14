# Qt GUI validation checklist

Use this checklist after changes that affect Qt GUI state, selection, preview
workers, gallery loading, export, or cache-open behavior.

## Basic smoke test

1. Launch `build-qt/cachegui_qt/Release/cachegui_qt.exe`.
2. Click `Open` with the default Firestorm texture-cache path.
3. Confirm the table populates and the status bar reports loaded entries.
4. Select a row and click `Preview`.
5. Confirm one of these expected outcomes:
   * A preview appears and the status bar reports image dimensions.
   * The row is marked `No preview` or `Load failed` without freezing.
6. Click `Try Next Preview`.
7. Confirm scanning follows the current table/gallery sort order and stops on a
   previewable entry or reports no previewable entry in the bounded scan.
8. Toggle `Gallery`, scroll several pages, and confirm placeholders/previews load
   without blocking the UI.
9. Toggle back to `Table` and confirm columns render normally.
10. If a preview is visible, resize the window and confirm the preview rescales.

## Export smoke test

1. Select a row that has a successful preview.
2. Click `Export PNG`.
3. Save to a temporary file.
4. Confirm the status bar reports success and the PNG opens in an image viewer.

## When a build-only check is enough

A build-only check is usually enough for narrow helper moves that do not change
Qt signals, selection, model/proxy use, async worker state, preview cache state,
or widget rendering.

## When GUI validation is required

Run at least the basic smoke test after changes involving:

* `MainWindow` preview or gallery orchestration
* `PreviewCache`, `PreviewPanel`, or `TryNextPreviewState`
* `GalleryPreviewQueue` or `GalleryPreviewScanner`
* table/gallery model roles or selection behavior
* cache open/close behavior
* PNG export behavior
* Qt build/deployment options
