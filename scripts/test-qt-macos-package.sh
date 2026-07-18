#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 <CacheExplorer.dmg> [expected-version]" >&2
    exit 2
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This package test must run on macOS." >&2
    exit 1
fi

dmg_path="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
expected_version="${2:-0.1.0}"

if [[ ! -f "$dmg_path" ]]; then
    echo "DMG not found: $dmg_path" >&2
    exit 1
fi

for tool in hdiutil plutil file otool codesign; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required tool not found: $tool" >&2
        exit 1
    fi
done

mount_dir="$(mktemp -d)"
mounted=false
cleanup() {
    if [[ "$mounted" == true ]]; then
        hdiutil detach "$mount_dir" -quiet || true
    fi
    rmdir "$mount_dir" 2>/dev/null || true
}
trap cleanup EXIT

hdiutil attach "$dmg_path" -nobrowse -readonly -mountpoint "$mount_dir" >/dev/null
mounted=true

app="$mount_dir/CacheExplorer.app"
executable="$app/Contents/MacOS/CacheExplorer"
info_plist="$app/Contents/Info.plist"

required_paths=(
    "$app"
    "$executable"
    "$info_plist"
    "$app/Contents/Resources/cacheexplorer.icns"
    "$app/Contents/Frameworks/QtCore.framework"
    "$app/Contents/Frameworks/QtGui.framework"
    "$app/Contents/Frameworks/QtWidgets.framework"
    "$app/Contents/PlugIns/platforms/libqcocoa.dylib"
    "$mount_dir/README.md"
    "$mount_dir/RELEASE_NOTES.md"
    "$mount_dir/LICENSE"
    "$mount_dir/docs/qt-user-guide.md"
)

for path in "${required_paths[@]}"; do
    if [[ ! -e "$path" ]]; then
        echo "Required package path is missing: $path" >&2
        exit 1
    fi
done

if [[ -e "$app/Contents/Frameworks/QtNetwork.framework" ]]; then
    echo "Unexpected QtNetwork.framework was deployed." >&2
    exit 1
fi

bundle_id="$(plutil -extract CFBundleIdentifier raw "$info_plist")"
bundle_version="$(plutil -extract CFBundleShortVersionString raw "$info_plist")"
if [[ "$bundle_id" != "io.github.deliatafler.CacheExplorer" ]]; then
    echo "Unexpected bundle identifier: $bundle_id" >&2
    exit 1
fi
if [[ "$bundle_version" != "$expected_version" ]]; then
    echo "Unexpected bundle version: $bundle_version" >&2
    exit 1
fi

architecture="$(file "$executable")"
echo "$architecture"
grep -q "arm64" <<<"$architecture"

codesign --verify --deep --strict --verbose=2 "$app"

dependencies="$(otool -L "$executable")"
for framework in QtCore QtGui QtWidgets; do
    if ! grep -q "@rpath/$framework.framework" <<<"$dependencies"; then
        echo "Executable does not use the bundled $framework framework." >&2
        exit 1
    fi
done
if grep -Eq '/Users/runner/work|/Qt/[0-9]' <<<"$dependencies"; then
    echo "Executable contains a developer-machine Qt dependency path." >&2
    exit 1
fi

echo "macOS DMG package validation passed: $dmg_path"
