# Post-beta roadmap

Use this as a parking lot for ideas that should be considered after the first
Qt beta candidate.

## Gallery and preview throughput

The removed native Win32 prototype had a few useful ideas worth revisiting in
the Qt GUI:

* Consider a bounded multi-worker thumbnail queue if one-at-a-time lazy loading
  is not fast enough on large real caches.
Keep these as Qt/cachelib features if they are implemented. Do not resurrect
platform-specific GUI code for them.

Completed slices:

* The Qt GUI persists terminal no-preview/load-failed states across runs, keyed
  by cache path plus UUID/cacheIndex/image/body metadata, so Gallery can skip
  rechecking entries already known to be incomplete or undecodable.
* The Gallery `Show` control offers a demand-driven `Images only` mode that
  keeps successful thumbnails and removes unavailable visible entries without
  scanning the full cache up front.

Possible follow-up:

* Persist successful thumbnail images or previewable metadata separately if
  Gallery startup throughput becomes a problem.
* Batch or debounce `Images only` removals if large Gallery viewports still
  reflow too often during beta use.
