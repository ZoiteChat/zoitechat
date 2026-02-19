#!/bin/sh

set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$SCRIPT_DIR"

BUNDLE_DEF="zoitechat.bundle"
APP_NAME="ZoiteChat.app"

# Expected prefixes for macOS GTK dependencies:
# - Homebrew: /opt/homebrew (Apple Silicon) or /usr/local (Intel)
# - MacPorts: /opt/local
# Required runtime stack includes GTK3, Pango, GDK-Pixbuf and enchant.

if command -v gtk-mac-bundler >/dev/null 2>&1; then
    BUNDLER_CMD="gtk-mac-bundler"
elif command -v python3 >/dev/null 2>&1 && python3 -c 'import gtk_mac_bundler' >/dev/null 2>&1; then
    BUNDLER_CMD="python3 -m gtk_mac_bundler"
else
    cat >&2 <<'MSG'
error: gtk-mac-bundler not found.
Install one of the following before running osx/makebundle.sh:
  - executable: gtk-mac-bundler
  - python module: gtk_mac_bundler (invoked via: python3 -m gtk_mac_bundler)
MSG
    exit 1
fi

rm -rf "$APP_NAME"
rm -f ./*.app.zip

# Enchant packaging changed between releases/package managers:
# - some installs provide share/enchant
# - others provide share/enchant-2
# - some have no share-level config dir at all
# Keep the bundle definition in sync with what's actually available so
# gtk-mac-bundler doesn't fail on a missing source path.

# Resolve package-manager prefix dynamically so Intel (/usr/local) and
# Apple Silicon (/opt/homebrew) hosts both bundle correctly.
BUNDLE_PREFIX="${BUNDLE_PREFIX:-}"
if [ -z "$BUNDLE_PREFIX" ] && command -v brew >/dev/null 2>&1; then
    BUNDLE_PREFIX="$(brew --prefix 2>/dev/null || true)"
fi
if [ -z "$BUNDLE_PREFIX" ]; then
    BUNDLE_PREFIX="/usr/local"
fi

ENCHANT_PREFIX_DEFAULT="${BUNDLE_PREFIX}/opt/enchant"
ENCHANT_PREFIX_PATH="${ENCHANT_PREFIX:-$ENCHANT_PREFIX_DEFAULT}"

perl -0pi -e 's|(<prefix\s+name="default">)[^<]+(</prefix>)|$1'"$BUNDLE_PREFIX"'$2|s' "$BUNDLE_DEF"
perl -0pi -e 's|(<prefix\s+name="enchant">)[^<]+(</prefix>)|$1'"$ENCHANT_PREFIX_PATH"'$2|s' "$BUNDLE_DEF"

if command -v brew >/dev/null 2>&1; then
    BREW_ENCHANT_PREFIX="$(brew --prefix enchant 2>/dev/null || true)"
    if [ -n "$BREW_ENCHANT_PREFIX" ]; then
        ENCHANT_PREFIX_PATH="$BREW_ENCHANT_PREFIX"
        perl -0pi -e 's|(<prefix\s+name="enchant">)[^<]+(</prefix>)|$1'"$ENCHANT_PREFIX_PATH"'$2|s' "$BUNDLE_DEF"
    fi
fi

if [ -n "$ENCHANT_PREFIX_PATH" ]; then
    if [ -d "$ENCHANT_PREFIX_PATH/share/enchant" ]; then
        perl -0pi -e 's|(<data>\s*)\$\{prefix:enchant\}/share/enchant(?:-2)?(\s*</data>)|$1\$\{prefix:enchant\}/share/enchant$2|s' "$BUNDLE_DEF"
    elif [ -d "$ENCHANT_PREFIX_PATH/share/enchant-2" ]; then
        perl -0pi -e 's|(<data>\s*)\$\{prefix:enchant\}/share/enchant(?:-2)?(\s*</data>)|$1\$\{prefix:enchant\}/share/enchant-2$2|s' "$BUNDLE_DEF"
    else
        perl -0pi -e 's|\n\s*<data>\s*\$\{prefix:enchant\}/share/enchant(?:-2)?\s*</data>\n|\n|s' "$BUNDLE_DEF"
    fi
fi


# Keep Info.plist generation deterministic by always rendering from template
# using a single, explicit version source.
VERSION_STRING="${VERSION:-$(sed -n "s/^  version: '\([^']*\)',$/\1/p" ../meson.build | head -n1)}"
if [ -z "$VERSION_STRING" ]; then
    echo "error: unable to determine VERSION_STRING for Info.plist" >&2
    exit 1
fi

TMP_PLIST="Info.plist.tmp"
LC_ALL=C sed "s/@VERSION@/$VERSION_STRING/g" Info.plist.in > "$TMP_PLIST"
mv -f "$TMP_PLIST" Info.plist

# shellcheck disable=SC2086
$BUNDLER_CMD "$BUNDLE_DEF"

if [ ! -d "$APP_NAME" ]; then
    echo "error: bundler finished but $APP_NAME was not created" >&2
    exit 1
fi

if command -v file >/dev/null 2>&1; then
    echo "Bundled binary architecture:"
    file "$APP_NAME/Contents/MacOS/ZoiteChat-bin" || true
fi

echo "Compressing bundle"
zip -9rXq "./ZoiteChat-$(git describe --tags).app.zip" "./$APP_NAME"
