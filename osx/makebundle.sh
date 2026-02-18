#!/bin/sh

set -eu

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

# shellcheck disable=SC2086
$BUNDLER_CMD "$BUNDLE_DEF"

if [ ! -d "$APP_NAME" ]; then
    echo "error: bundler finished but $APP_NAME was not created" >&2
    exit 1
fi

echo "Compressing bundle"
zip -9rXq "./ZoiteChat-$(git describe --tags).app.zip" "./$APP_NAME"
