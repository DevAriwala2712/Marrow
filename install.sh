#!/bin/bash
set -euo pipefail

name=Marrow-macos-universal.tar.gz
base=https://github.com/DevAriwala2712/Marrow/releases/latest/download
bundle_id=io.github.devariwala2712.marrow

fail() {
    printf 'Marrow install: %s\n' "$*" >&2
    exit 1
}

[[ "$(uname -s)" == Darwin ]] || fail "macOS 13 or later is required."
version="$(sw_vers -productVersion)"
[[ "${version%%.*}" -ge 13 ]] || fail "macOS 13 or later is required."
[[ -n "${HOME:-}" && "$HOME" == /* ]] || fail "HOME must be an absolute path."

apps="$HOME/Applications"
app="$apps/Marrow.app"
bin="$HOME/.local/bin"
cli="$bin/marrow-cli"
target="$app/Contents/MacOS/marrow-cli"
lock="$apps/.marrow-install.lock"

mkdir -p "$apps" "$bin"
mkdir "$lock" 2>/dev/null || fail "another installation is using $lock"
work=''
cleanup() {
    if [[ -n "$work" ]]; then rm -rf "$work"; fi
    if [[ -e "$lock/previous.app" && ! -e "$app" ]]; then
        mv "$lock/previous.app" "$app" || {
            printf 'Restore the previous app from %s\n' "$lock/previous.app" >&2
            return
        }
    fi
    rm -rf "$lock"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

if [[ -e "$app" || -L "$app" ]]; then
    [[ -d "$app" && ! -L "$app" ]] || fail "$app already exists."
    existing_id="$(plutil -extract CFBundleIdentifier raw -o - "$app/Contents/Info.plist" 2>/dev/null || true)"
    [[ "$existing_id" == "$bundle_id" ]] || fail "$app belongs to a different application."
fi
if [[ -e "$cli" || -L "$cli" ]]; then
    [[ -L "$cli" && "$(readlink "$cli")" == "$target" ]] || fail "$cli already exists."
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/marrow-install.XXXXXXXX")"
for asset in "$name" "$name.sha256"; do
    curl --fail --silent --show-error --location --retry 2 --proto '=https' --proto-redir '=https' \
        "$base/$asset" --output "$work/$asset"
done
(
    cd "$work"
    shasum -a 256 -c "$name.sha256"
    tar -xzf "$name"
)
ditto "$work/Marrow.app" "$lock/Marrow.app"
codesign --verify --deep --strict "$lock/Marrow.app"
[[ "$(plutil -extract CFBundleIdentifier raw -o - "$lock/Marrow.app/Contents/Info.plist")" == "$bundle_id" ]] ||
    fail "the download has an unexpected bundle identifier."

if [[ -e "$app" ]]; then mv "$app" "$lock/previous.app"; fi
if ! mv "$lock/Marrow.app" "$app"; then
    if [[ -d "$lock/previous.app" ]]; then
        mv "$lock/previous.app" "$app" || fail "restore the previous app from $lock/previous.app"
    fi
    fail "could not move Marrow into $apps."
fi
rm -rf "$lock/previous.app"
if [[ ! -L "$cli" ]]; then ln -s "$target" "$cli"; fi

printf 'Installed Marrow in %s\n' "$app"
printf 'Launch it with: open "%s"\n' "$app"
printf 'Run the CLI with: "%s" --help\n' "$cli"
case ":$PATH:" in
    *":$bin:"*) ;;
    *) printf 'Add %s to PATH to run marrow-cli by name.\n' "$bin" ;;
esac
