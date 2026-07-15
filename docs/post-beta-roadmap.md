# Post-beta roadmap

Use this as a parking lot for ideas that should be considered after the first
Qt beta candidate.

## Gallery and preview throughput

The removed native Win32 prototype had a few useful ideas worth revisiting in
the Qt GUI:

* Consider a bounded multi-worker thumbnail queue if one-at-a-time lazy loading
  is not fast enough on large real caches.
* Consider a "likely complete" or cached-byte-size filter for debugging and for
  users who want to bias Gallery toward entries more likely to preview.

Keep these as Qt/cachelib features if they are implemented. Do not resurrect
platform-specific GUI code for them.

Completed first slice:

* The Qt GUI persists terminal no-preview/load-failed states across runs, keyed
  by cache path plus UUID/cacheIndex/image/body metadata, so Gallery can skip
  rechecking entries already known to be incomplete or undecodable.

Possible follow-up:

* Persist successful thumbnail images or previewable metadata separately if
  Gallery startup throughput becomes a problem.
