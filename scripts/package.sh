#!/bin/bash
set -euo pipefail

build_dir="${1:-build}"
output_dir="${2:-dist}"
name=Marrow-macos-universal.tar.gz
stage="$(mktemp -d "${TMPDIR:-/tmp}/marrow-package.XXXXXXXX")"
trap 'rm -rf "$stage"' EXIT

for binary in "$build_dir/Marrow.app/Contents/MacOS/Marrow" "$build_dir/marrow-cli"; do
    architectures="$(lipo -archs "$binary")"
    for arch in arm64 x86_64; do
        if [[ " $architectures " != *" $arch "* ]]; then
            printf '%s is missing the %s slice.
' "$binary" "$arch" >&2
            exit 1
        fi
    done
done

mkdir -p "$output_dir"
ditto "$build_dir/Marrow.app" "$stage/Marrow.app"
contents="$stage/Marrow.app/Contents"
install -m 755 "$build_dir/marrow-cli" "$contents/MacOS/marrow-cli"
notices="$contents/Resources/ThirdPartyLicenses"
mkdir -p "$notices"
cp "$build_dir/_deps/glfw-src/LICENSE.md" "$notices/GLFW.txt"
cp "$build_dir/_deps/imgui-src/LICENSE.txt" "$notices/Dear-ImGui.txt"
cp "$build_dir/_deps/nlohmann_json-src/LICENSE.MIT" "$notices/nlohmann-json.txt"

codesign --force --sign - --timestamp=none "$contents/MacOS/marrow-cli"
codesign --force --sign - --timestamp=none "$stage/Marrow.app"
codesign --verify --deep --strict "$stage/Marrow.app"

COPYFILE_DISABLE=1 tar -czf "$stage/$name" -C "$stage" Marrow.app
(
    cd "$stage"
    shasum -a 256 "$name" > "$name.sha256"
)
mv "$stage/$name" "$stage/$name.sha256" "$output_dir/"
printf 'Created %s and its SHA-256 checksum in %s\n' "$name" "$output_dir"
