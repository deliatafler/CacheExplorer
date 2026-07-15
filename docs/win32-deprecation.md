# Win32 GUI deprecation

The native Win32 GUI target, `cachegui`, is deprecated legacy code.

The supported GUI path for beta and future cross-platform work is the Qt 6 GUI,
`cachegui_qt`. The Win32 target remains buildable for now as a short-term
reference and fallback while the first Qt beta is prepared.

## Policy

* Do not add new user-facing features to `cachegui`.
* Do not duplicate Qt GUI polish work in `cachegui`.
* Keep `cachegui` building unless doing so starts to block beta progress.
* Fix Win32 issues only when they preserve existing behavior, keep the target
  buildable, or reveal a shared `cachelib` problem.
* Revisit removal after the first Qt beta.

## Build toggle

The legacy target is still enabled in the default Windows build for now:

```bash
cmake --build build --config Release
```

To skip it during configure:

```bash
cmake -S . -B build -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DCACHEEXPLORER_BUILD_LEGACY_WIN32_GUI=OFF
```
