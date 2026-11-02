# Experimental Qt 6 frontend

`zoitechat-qt` is a separate Qt 6 Widgets application using ZoiteChat's existing
C core, IRC commands, configuration, networks and plugin API. The GTK frontend
remains available and is still the default build.

Imported from [SeraphinaDX/hexchat-ng](https://github.com/SeraphinaDX/hexchat-ng)
at commit `91b9a7f579a9328abf60713a8d473ef8c24e092f`. The imported code retains
its GPL-2.0-or-later license notices. The integration adapts core headers, plugin
symbols, application identity, paths and icons to ZoiteChat.

## Build

Install Meson, Ninja, a C17/C++17 compiler, Qt 6.2 or newer (Core, Gui, Widgets
and resource compiler), Pango/Cairo 1.44 or newer, GLib/GIO development tools
(including `glib-genmarshal`), gettext, libarchive, OpenSSL and the normal
ZoiteChat build dependencies. Enchant 2 enables optional spell checking.

For a Qt-only build without plugins:

```sh
meson setup build-qt -Dqt-frontend=true -Dgtk-frontend=false -Dplugin=false
meson compile -C build-qt
./build-qt/src/fe-qt/zoitechat-qt
```

Leave `gtk-frontend` enabled to build both applications. C++ and Qt dependencies
are only required when `qt-frontend` is enabled. To disable spell checking, use
`-Dqt-spellchecking=disabled`.

To build with plugins, enable `-Dplugin=true` and select the existing `with-*`
options for the dependencies you have installed. FiSHLiM's key manager uses GTK
widgets; disable it with `-Dwith-fishlim=false` for a Qt-only installation.

The build generates URL rules using the public suffix list. For offline builds,
supply `-Dpublic-suffix-list=/path/to/public_suffix_list.dat`.

## Use

The application shares ZoiteChat's normal configuration. Use
`--cfgdir /path/to/config` for an isolated profile and `--no-auto` to disable
automatic connections. Use **ZoiteChat > Network List** to manage saved networks,
or enter `/server` and other existing IRC commands in the input box.

The frontend includes channel and private conversations, network management,
IRC formatting, nick lists, completion, preferences, themes and palette editing,
search, notifications, tray integration, DCC transfers, add-on management,
custom menus and keybindings. See [FEATURE-PARITY.md](FEATURE-PARITY.md) for the
imported HexChat feature inventory.

ZoiteChat-specific typing indicators and interactive reply metadata are not yet
displayed; messages appear as ordinary text. Qt does not persist the GTK theme
when saving preferences. Native desktop, tray and platform packaging behavior
still require testing on each target desktop. Existing GTK packaging scripts
continue to package the GTK application; this change adds a Meson-installed Qt
executable and desktop launcher.

## Tests

```sh
meson test -C build-qt --print-errorlogs
```

Qt UI tests use the offscreen platform. Runtime tests require plugins and local
socket access; scripting tests exercise the scripting modules enabled in the
build. To run without local socket access:

```sh
meson test -C build-qt --no-suite runtime --print-errorlogs
```

The runtime fixtures create isolated temporary profiles and probe plugins. The
[testing notes](TESTING.md) describe the imported regression coverage and manual
checks. `.github/workflows/qt-build.yml` builds and tests the Qt frontend on Linux.
