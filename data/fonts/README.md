# Bundled emoji font

`NotoColorEmoji.ttf` reports Noto Color Emoji version 2.051 and covers
Unicode Emoji 17.0. It is vendored from Debian's
`fonts-noto-color-emoji` 2.051 package, built from the corresponding
Google Noto Emoji release.

The font is bundled only with Windows builds, where it is installed
inside the application and registered only for the ZoiteChat process.
It is not installed as a global operating-system font.

Linux (and other Unix) builds do not install this file. They rely on
the emoji font provided by the operating system instead — typically the
distribution's Noto Color Emoji package (`fonts-noto-color-emoji` on
Debian/Ubuntu, `google-noto-color-emoji-fonts` on Fedora).

Upstream release: https://github.com/googlefonts/noto-emoji/releases/tag/v2.051

Vendored file SHA-256:
`dac5c27651082d6c53dab5081f50d2022ddf6877d730fa919cf6a4fc2af22de0`

The font is distributed under the SIL Open Font License 1.1. See
`OFL.txt`.
