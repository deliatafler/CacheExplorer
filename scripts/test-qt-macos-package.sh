#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
    echo "Usage: $0 <CacheExplorer.dmg> [expected-bundle-version] [expected-display-version]" >&2
    exit 2
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This package test must run on macOS." >&2
    exit 1
fi

dmg_path="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
expected_version="${2:-0.1.0}"
expected_display_version="${3:-}"

if [[ ! -f "$dmg_path" ]]; then
    echo "DMG not found: $dmg_path" >&2
    exit 1
fi

for tool in hdiutil plutil file otool codesign vtool; do
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

deployment_info="$(vtool -show-build "$executable")"
echo "$deployment_info"

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

rpaths="$(otool -l "$executable" | awk '
    $1 == "cmd" && $2 == "LC_RPATH" {
        getline
        getline
        print $2
    }
')"
if ! grep -Fxq "@executable_path/../Frameworks" <<<"$rpaths"; then
    echo "Executable is missing the app-local Qt framework runpath." >&2
    echo "Detected runpaths: ${rpaths:-<none>}" >&2
    exit 1
fi

version_output="$("$executable" --version 2>&1)"
echo "$version_output"
if ! grep -Fq "CacheExplorer" <<<"$version_output"; then
    echo "Packaged application did not report its name at startup." >&2
    exit 1
fi
if [[ -n "$expected_display_version" ]] &&
   ! grep -Fq "$expected_display_version" <<<"$version_output"; then
    echo "Unexpected application display version: $version_output" >&2
    exit 1
fi

echo "macOS DMG package validation passed: $dmg_path"
