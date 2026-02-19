#!/bin/sh

set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$SCRIPT_DIR"

BUNDLE_DEF="zoitechat.bundle"
APP_NAME="ZoiteChat.app"

HOST_ARCH="$(uname -m 2>/dev/null || echo unknown)"
if [ -z "${TARGET_ARCHES+x}" ]; then
    TARGET_ARCHES="$HOST_ARCH"
fi
UNIVERSAL_BINARIES="${UNIVERSAL_BINARIES:-}"
UNIVERSAL_BUILD_DIRS="${UNIVERSAL_BUILD_DIRS:-../build-macos-arm64 ../build-macos-x86_64}"

if [ "${UNIVERSAL:-0}" = "1" ]; then
    TARGET_ARCHES="arm64 x86_64"
fi

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

# gtk-mac-bundler rebases translation paths using string concatenation on
# ${prefix}. Keep a trailing slash in the bundle metadata to avoid malformed
# paths like ".../x86_64locale/..." when copying locale catalogs.
BUNDLE_PREFIX_XML="${BUNDLE_PREFIX%/}/"

ENCHANT_PREFIX_DEFAULT="${BUNDLE_PREFIX}/opt/enchant"
ENCHANT_PREFIX_PATH="${ENCHANT_PREFIX:-$ENCHANT_PREFIX_DEFAULT}"

perl -0pi -e 's|(<prefix\s+name="default">)[^<]+(</prefix>)|$1'"$BUNDLE_PREFIX_XML"'$2|s' "$BUNDLE_DEF"
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

# GTK module extension differs across package manager builds:
# - Homebrew commonly installs .dylib modules
# - Some environments still ship .so modules
# Detect what exists in the staged prefix and rewrite the bundle definition
# so gtk-mac-bundler does not fail on a missing glob.
GTK_MODULE_EXT="so"
if find "$BUNDLE_PREFIX/lib/gtk-3.0" -type f \( -path '*/immodules/*.dylib' -o -path '*/printbackends/*.dylib' \) -print -quit 2>/dev/null | grep -q . \
    || find "$BUNDLE_PREFIX/lib/gdk-pixbuf-2.0" -type f -path '*/loaders/*.dylib' -print -quit 2>/dev/null | grep -q .; then
    GTK_MODULE_EXT="dylib"
fi
if [ "$GTK_MODULE_EXT" = "dylib" ]; then
    perl -0pi -e 's|(/immodules/)\*\.so|$1*.dylib|g; s|(/printbackends/)\*\.so|$1*.dylib|g; s|(/loaders/)\*\.so|$1*.dylib|g' "$BUNDLE_DEF"
else
    perl -0pi -e 's|(/immodules/)\*\.dylib|$1*.so|g; s|(/printbackends/)\*\.dylib|$1*.so|g; s|(/loaders/)\*\.dylib|$1*.so|g' "$BUNDLE_DEF"
fi

# Some staged builds omit GTK runtime module directories entirely
# (for example stripped-down CI artifacts). Remove bundle entries for
# missing globs so gtk-mac-bundler does not abort.
if ! find "$BUNDLE_PREFIX/lib/gtk-3.0" -type f -path "*/immodules/*.${GTK_MODULE_EXT}" -print -quit 2>/dev/null | grep -q .; then
    perl -0pi -e 's#\n\s*<binary>\s*\$\{prefix\}/lib/\$\{gtkdir\}/\$\{pkg:\$\{gtk\}:gtk_binary_version\}/immodules/\*\.(?:so|dylib)\s*</binary>\n#\n#g' "$BUNDLE_DEF"
fi
if ! find "$BUNDLE_PREFIX/lib/gtk-3.0" -type f -path "*/printbackends/*.${GTK_MODULE_EXT}" -print -quit 2>/dev/null | grep -q .; then
    perl -0pi -e 's#\n\s*<binary>\s*\$\{prefix\}/lib/\$\{gtkdir\}/\$\{pkg:\$\{gtk\}:gtk_binary_version\}/printbackends/\*\.(?:so|dylib)\s*</binary>\n#\n#g' "$BUNDLE_DEF"
fi
if ! find "$BUNDLE_PREFIX/lib/gdk-pixbuf-2.0" -type f -path "*/loaders/*.${GTK_MODULE_EXT}" -print -quit 2>/dev/null | grep -q .; then
    perl -0pi -e 's#\n\s*<binary>\s*\$\{prefix\}/lib/gdk-pixbuf-2\.0/\$\{pkg:gdk-pixbuf-2\.0:gdk_pixbuf_binary_version\}/loaders/\*\.(?:so|dylib)\s*</binary>\n#\n#g' "$BUNDLE_DEF"
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

BIN_PATH="$APP_NAME/Contents/MacOS/ZoiteChat-bin"

if [ "${UNIVERSAL:-0}" = "1" ] && [ -z "$UNIVERSAL_BINARIES" ]; then
    for build_dir in $UNIVERSAL_BUILD_DIRS; do
        candidate="$build_dir/src/fe-gtk/zoitechat"
        if [ -f "$candidate" ]; then
            UNIVERSAL_BINARIES="${UNIVERSAL_BINARIES:+$UNIVERSAL_BINARIES }$candidate"
        fi
    done

    if [ -z "$UNIVERSAL_BINARIES" ]; then
        cat >&2 <<'MSG'
error: UNIVERSAL=1 requested, but no source binaries were found.
Set UNIVERSAL_BINARIES to one or more architecture-specific zoitechat binaries,
or place builds at:
  ../build-macos-arm64/src/fe-gtk/zoitechat
  ../build-macos-x86_64/src/fe-gtk/zoitechat
MSG
        exit 1
    fi
fi

if [ -n "$UNIVERSAL_BINARIES" ]; then
    if ! command -v lipo >/dev/null 2>&1; then
        echo "error: UNIVERSAL_BINARIES requires lipo, but lipo is unavailable" >&2
        exit 1
    fi

    for binary in $UNIVERSAL_BINARIES; do
        if [ ! -f "$binary" ]; then
            echo "error: universal source binary not found: $binary" >&2
            exit 1
        fi
    done

    echo "Creating universal ZoiteChat-bin from: $UNIVERSAL_BINARIES"
    # shellcheck disable=SC2086
    lipo -create $UNIVERSAL_BINARIES -output "$BIN_PATH"
fi

if command -v lipo >/dev/null 2>&1; then
    BIN_ARCHS="$(lipo -archs "$BIN_PATH" 2>/dev/null || true)"
    if [ -z "$BIN_ARCHS" ]; then
        echo "error: unable to detect architectures for $BIN_PATH" >&2
        exit 1
    fi

    for required_arch in $TARGET_ARCHES; do
        if ! echo " $BIN_ARCHS " | grep -q " $required_arch "; then
            cat >&2 <<MSG
error: bundled binary architecture mismatch.
  required: $TARGET_ARCHES
  actual:   $BIN_ARCHS
hint: rebuild ZoiteChat-bin with matching architecture(s) before bundling.
MSG
            exit 1
        fi
    done
fi

if command -v file >/dev/null 2>&1; then
    echo "Bundled binary architecture:"
    file "$BIN_PATH" || true
fi

echo "Compressing bundle"
ARCHIVE_VERSION="$(git describe --tags --always 2>/dev/null || true)"
if [ -z "$ARCHIVE_VERSION" ]; then
    ARCHIVE_VERSION="$VERSION_STRING"
fi
zip -9rXq "./ZoiteChat-$ARCHIVE_VERSION.app.zip" "./$APP_NAME"
