# Bundled emoji data

These are pinned copies of the upstream data files that
`tools/gen-emoji-data.py` compiles into ZoiteChat's emoji picker catalog
at build time. Bundling them means every ZoiteChat package exposes the
same emoji, names, categories and search keywords, independent of the
GTK runtime version or the emoji data a distribution installs under
`/usr/share/gtk-3.0/emoji`.

Nothing here affects what is sent over IRC: the picker inserts plain
Unicode sequences. Official builds register the bundled Noto Color Emoji
font privately for consistent rendering, while preserving platform font
fallback if registration is unavailable.

| File | Source | Version |
| ---- | ------ | ------- |
| `emoji-test.txt` | [Unicode emoji data](https://unicode.org/Public/emoji/17.0/emoji-test.txt) (mirrored at [unicode-org/unicodetools](https://github.com/unicode-org/unicodetools/blob/main/unicodetools/data/emoji/17.0/emoji-test.txt)) | Emoji 17.0 |
| `cldr-annotations-en.xml` | [unicode-org/cldr](https://github.com/unicode-org/cldr/blob/release-48/common/annotations/en.xml) `common/annotations/en.xml` | CLDR release 48 |
| `cldr-annotations-derived-en.xml` | [unicode-org/cldr](https://github.com/unicode-org/cldr/blob/release-48/common/annotationsDerived/en.xml) `common/annotationsDerived/en.xml` | CLDR release 48 |
| `gemoji.json` | [github/gemoji](https://github.com/github/gemoji/blob/v4.1.0/db/emoji.json) `db/emoji.json` | v4.1.0 |

The pinned versions are recorded in `VERSIONS`.

## Licenses

- `emoji-test.txt`, `cldr-annotations-en.xml`,
  `cldr-annotations-derived-en.xml`: © Unicode, Inc., distributed under
  the [Unicode License v3](https://www.unicode.org/license.txt).
- `gemoji.json`: from the gemoji project, MIT licensed,
  © GitHub, Inc.
