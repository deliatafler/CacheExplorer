# CacheExplorer work checklist

This is the working list for the Qt beta and the next development rounds. Keep
it outcome-focused; move finished work to project history or the post-beta
roadmap rather than letting this become a changelog.

## Before a public beta

- [ ] Run the packaged GUI through the full real-cache checklist on a release
  candidate.
- [ ] Test the shared Qt ZIP on a Windows machine without the developer Qt SDK.
- [ ] Confirm the package version, release notes, and generated checksum match
  the archive being shared.
- [ ] Decide where beta feedback and issue reports should be collected.

## Near-term UX

- [x] Make Gallery the default browsing view.
- [x] Preview Table entries on selection instead of requiring a Preview button.
- [x] Make cache selection discoverable with an immediate folder chooser,
  default-cache shortcut, and remembered last successful location.
- [x] Export multiple selected textures to a chosen folder without overwriting
  existing PNGs.
- [ ] Add a small recent-cache picker if users commonly switch among multiple
  Firestorm profiles.
- [ ] Refine Gallery tile layout and preview-panel presentation from beta
  feedback.

## Reliability and performance

- [ ] Add targeted regression coverage for Qt-facing pure helpers as they are
  extracted; keep cache-format behavior covered in `cachelib` tests.
- [ ] Measure real-cache Gallery throughput before considering a bounded
  multi-worker thumbnail queue.
- [ ] Consider persisting successful thumbnail metadata or images only if
  Gallery startup remains a real bottleneck.

## Cross-platform and distribution

- [ ] Validate the prebuilt-Qt CMake path on macOS and Linux with their native
  Qt SDKs and document any platform-specific prerequisites.
- [ ] Confirm the GitHub Actions Windows workflow builds the core and
  prebuilt-Qt GUI configurations on the hosted runner.
- [ ] Confirm the GitHub Actions workflow publishes a repeatable beta ZIP,
  checksum, and package smoke result after its first successful run.
- [ ] Revisit static Qt or single-file distribution only after the shared-Qt
  beta path has proven itself.

## Post-beta product work

- [ ] Use beta feedback to prioritize search, richer metadata inspection, and
  bulk-export workflow improvements.
- [ ] Reassess Gallery filtering/sorting based on real browsing habits.
- [ ] Keep the Qt-only GUI path; do not reintroduce platform-specific Win32 UI
  code.
