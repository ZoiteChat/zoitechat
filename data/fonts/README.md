# Bundled emoji font

`NotoColorEmoji.ttf` reports Noto Color Emoji version 2.051 and covers
Unicode Emoji 17.0. It is vendored from Debian's
`fonts-noto-color-emoji` 2.051 package, built from the corresponding
Google Noto Emoji release.

Every GTK build installs this exact file: Windows (installer and portable
build files), AppImage, Flatpak and native Meson/RPM builds. It is loaded
into a private Pango/FreeType font map as `ZoiteChat Emoji`; an older system
Noto font cannot replace it. No font is installed globally on Windows.

Chat windows and the emoji picker share this map. Fontconfig still loads
system fonts and configuration for ordinary text, and the configured text
font, size and style remain in effect. In particular, plain digits, `#`
and `*` are not assigned emoji metrics.

Windows uses the same FreeType backend rather than GDI registration or an
OS-version-specific picker. The dependency bundle already ships FreeType,
Fontconfig and PangoFT2. This rendering path does not require Windows' native
color-font support. Actual Windows 7 execution still needs testing on that OS.

Meson requires PangoFT2 >= 1.44 and Fontconfig >= 2.13. Normal installations
load `share/fonts/zoitechat/NotoColorEmoji.ttf` beneath the configured data
directory; Windows resolves it relative to the executable. AppImage sets
`ZOITECHAT_EMOJI_FONT` to its own copy. For an uninstalled development build:

```sh
ZOITECHAT_EMOJI_FONT="$PWD/data/fonts/NotoColorEmoji.ttf" build/src/fe-gtk/zoitechat
```

A missing/unloadable file logs a warning and retains system-font fallback.
CI verifies the installed copy and tests catalog/font coverage; AppImage and
Windows also run the interactive picker regression tests. Native themes,
DPI and different Pango/Cairo versions can still affect pixel-level output.

Upstream release: https://github.com/googlefonts/noto-emoji/releases/tag/v2.051

Vendored file SHA-256:
`dac5c27651082d6c53dab5081f50d2022ddf6877d730fa919cf6a4fc2af22de0`

The font is distributed under the SIL Open Font License 1.1. See
`OFL.txt`.
