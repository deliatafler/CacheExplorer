#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <CacheExplorer.deb>" >&2
    exit 2
fi

package_path="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
if [[ ! -f "$package_path" ]]; then
    echo "Debian package not found: $package_path" >&2
    exit 1
fi

for tool in dpkg-deb desktop-file-validate ldd; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required tool not found: $tool" >&2
        exit 1
    fi
done

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT
dpkg-deb --extract "$package_path" "$work_dir/root"

required_paths=(
    "$work_dir/root/usr/bin/CacheExplorer"
    "$work_dir/root/usr/share/applications/io.github.deliatafler.CacheExplorer.desktop"
    "$work_dir/root/usr/share/pixmaps/cacheexplorer.png"
    "$work_dir/root/usr/share/doc/cacheexplorer/README.md"
    "$work_dir/root/usr/share/doc/cacheexplorer/RELEASE_NOTES.md"
    "$work_dir/root/usr/share/doc/cacheexplorer/LICENSE"
    "$work_dir/root/usr/share/doc/cacheexplorer/qt-user-guide.md"
)

for path in "${required_paths[@]}"; do
    if [[ ! -e "$path" ]]; then
        echo "Required package path is missing: $path" >&2
        exit 1
    fi
done

desktop-file-validate \
    "$work_dir/root/usr/share/applications/io.github.deliatafler.CacheExplorer.desktop"

package_name="$(dpkg-deb --field "$package_path" Package)"
package_arch="$(dpkg-deb --field "$package_path" Architecture)"
package_dependencies="$(dpkg-deb --field "$package_path" Depends)"

if [[ "$package_name" != "cacheexplorer" ]]; then
    echo "Unexpected Debian package name: $package_name" >&2
    exit 1
fi
if [[ "$package_arch" != "amd64" ]]; then
    echo "Unexpected Debian package architecture: $package_arch" >&2
    exit 1
fi
if ! grep -q "qt6-qpa-plugins" <<<"$package_dependencies"; then
    echo "The package does not declare its Qt platform-plugin dependency." >&2
    exit 1
fi

linkage="$(ldd "$work_dir/root/usr/bin/CacheExplorer")"
echo "$linkage"
if grep -q "not found" <<<"$linkage"; then
    echo "The packaged executable has unresolved shared-library dependencies." >&2
    exit 1
fi
if grep -q "libQt6Network" <<<"$linkage"; then
    echo "Unexpected Qt Network dependency found." >&2
    exit 1
fi

echo "Debian package validation passed: $package_path"
