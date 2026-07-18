# CacheExplorer work checklist

This is the working list for the Qt beta and the next development rounds. Keep
it outcome-focused; move finished work to project history or the post-beta
roadmap rather than letting this become a changelog.

## Before a public beta

- [x] Add an open-source license and application icon.
- [x] Run the packaged GUI through the full real-cache checklist on a release
  candidate.
- [x] Test the shared Qt ZIP on a Windows machine without the developer Qt SDK.
- [x] Confirm the package version, release notes, and generated checksum match
  the archive being shared.
- [x] Collect beta feedback and issue reports through GitHub Issues, with the
  GUI's About diagnostics included when possible.

## Near-term UX

- [x] Make Gallery the default browsing view.
- [x] Preview Table entries on selection instead of requiring a Preview button.
- [x] Make cache selection discoverable with an immediate folder chooser,
  default-cache shortcut, and remembered last successful location.
- [x] Export multiple selected textures to a chosen folder without overwriting
  existing PNGs.
- [x] Add a small recent-cache picker if users commonly switch among multiple
  Firestorm profiles.
- [x] Start the first-run cache folder picker in the platform's viewer-cache
  root (`%LOCALAPPDATA%` on Windows and `~/Library/Caches` on macOS) when no
  remembered or discovered cache path is available.
- [ ] Refine Gallery tile layout and preview-panel presentation from beta
  feedback.

## Reliability and performance

- [x] Add targeted regression coverage for Qt-facing pure helpers as they are
  extracted; keep cache-format behavior covered in `cachelib` tests.
- [x] Make MSVC C++ exception unwinding explicit across all targets and keep
  the preferred prebuilt-Qt Release build free of C4530 warnings.
- [x] Add lightweight single-worker thumbnail throughput measurement to the
  existing Gallery activity display.
- [x] Batch `Images only` terminal-state removals so large viewports do not
  reflow after every unavailable thumbnail.
- [ ] Measure real-cache Gallery throughput before considering a bounded
  multi-worker thumbnail queue.
- [ ] Consider persisting successful thumbnail metadata or images only if
  Gallery startup remains a real bottleneck.

## Cross-platform and distribution

- [x] Validate the prebuilt-Qt CMake path on macOS and Linux with their native
  Qt SDKs and document any platform-specific prerequisites.
- [x] Build the core and prebuilt-Qt GUI configurations on the GitHub Actions
  Windows hosted runner.
- [x] Produce a repeatable beta ZIP and checksum in GitHub Actions, with a
  fresh-extraction package smoke test.
- [x] Produce, ad-hoc sign, and inspect an Apple Silicon `.app`/DMG artifact in
  GitHub Actions, including app-local Qt runpath and mounted-DMG launch checks.
- [x] Validate the ad-hoc-signed Apple Silicon package against a real cache on
  physical Mac hardware after fixing the missing Qt framework `LC_RPATH`.
  Launch, responsive Gallery scrolling/resizing, `Images only`, and single and
  multi-PNG export all passed on macOS 26.5.2.
- [x] Produce, install, and inspect release-specific Ubuntu 24.04 and 26.04
  x86-64 `.deb` artifacts in GitHub Actions.
- [x] Validate the Ubuntu 26.04 package and all GUI features against a real
  cache in an Ubuntu desktop VM.
- [x] Assemble all five platform packages and unified checksums through a
  manual release-workflow validation run.
- [x] Create draft GitHub Releases from explicit `v*` tags after all platform
  validation passes.
- [x] Add a conventional CPack/NSIS Windows installer while retaining the
  portable ZIP, including Start menu and uninstall support plus a CI
  install/launch/uninstall smoke test.
- [ ] Evaluate per-user installation, upgrade behavior, and code signing after
  beta feedback; retain native macOS DMG and Linux DEB packages rather than
  requiring Qt Installer Framework solely for visual consistency.
- [ ] Revisit static Qt or single-file distribution only after the shared-Qt
  beta path has proven itself.

## Post-beta product work

- [ ] Use beta feedback to prioritize search, richer metadata inspection, and
  bulk-export workflow improvements.
- [ ] Reassess Gallery filtering/sorting based on real browsing habits.
- [ ] Keep the Qt-only GUI path; do not reintroduce platform-specific Win32 UI
  code.
