# Win32 GUI deprecation

The native Win32 GUI target, `cachegui`, is deprecated legacy code.

The supported GUI path for beta and future cross-platform work is the Qt 6 GUI,
`cachegui_qt`. The Win32 target remains buildable for now as a short-term
reference and fallback after the first Qt beta candidate, but it is opt-in.

## Policy

* Do not add new user-facing features to `cachegui`.
* Do not duplicate Qt GUI polish work in `cachegui`.
* Keep `cachegui` buildable unless doing so starts to block beta progress.
* Fix Win32 issues only when they preserve existing behavior, keep the target
  buildable, or reveal a shared `cachelib` problem.
* Revisit removal after the first Qt beta.

## Build toggle

The legacy target is disabled by default. Normal builds focus on `cachelib`,
`cachecli`, and tests:

```bash
cmake --build build --config Release
```

To build the legacy Win32 GUI during configure:

```bash
cmake -S . -B build -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DCACHEEXPLORER_BUILD_LEGACY_WIN32_GUI=ON
```
