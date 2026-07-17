#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
input_path="${1:-$repo_root/cachegui/resources/cacheexplorer.png}"
output_path="${2:-$repo_root/cachegui/resources/cacheexplorer.icns}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This helper requires macOS sips and iconutil." >&2
    exit 1
fi

for tool in sips iconutil; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required tool not found: $tool" >&2
        exit 1
    fi
done

if [[ ! -f "$input_path" ]]; then
    echo "Icon source not found: $input_path" >&2
    exit 1
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT
iconset="$work_dir/cacheexplorer.iconset"
mkdir -p "$iconset" "$(dirname "$output_path")"

resize_icon() {
    local size="$1"
    local name="$2"
    sips -z "$size" "$size" "$input_path" --out "$iconset/$name" >/dev/null
}

resize_icon 16 icon_16x16.png
resize_icon 32 icon_16x16@2x.png
resize_icon 32 icon_32x32.png
resize_icon 64 icon_32x32@2x.png
resize_icon 128 icon_128x128.png
resize_icon 256 icon_128x128@2x.png
resize_icon 256 icon_256x256.png
resize_icon 512 icon_256x256@2x.png
resize_icon 512 icon_512x512.png
resize_icon 1024 icon_512x512@2x.png

iconutil -c icns "$iconset" -o "$output_path"
echo "Generated $output_path"
