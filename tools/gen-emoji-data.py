#!/usr/bin/env python3
#
# gen-emoji-data.py - generate ZoiteChat's bundled emoji catalog
#
# Consumes the pinned data files checked in under data/emoji/ and emits a
# C table that is compiled into the application, so every ZoiteChat package
# ships the same emoji, names, categories and search keywords regardless of
# which GTK runtime or /usr/share/gtk-3.0/emoji data a distribution provides.
#
# Inputs:
#   emoji-test.txt                 Unicode emoji-test data file (catalog,
#                                  CLDR ordering, groups, short names)
#   cldr-annotations-en.xml        CLDR annotations (search keywords)
#   cldr-annotations-derived-en.xml CLDR derived annotations (sequences)
#   gemoji.json                    gemoji database (:shortcode: aliases, tags)
#
# Usage:
#   gen-emoji-data.py EMOJI_TEST ANNOTATIONS DERIVED_ANNOTATIONS GEMOJI \
#                     VERSIONS OUTPUT
#
# Only fully-qualified sequences are emitted, so everything the picker
# inserts is a well-formed emoji presentation sequence.  Skin-tone variants
# are folded into per-base tone sets: the picker shows the neutral emoji and
# swaps in the modified sequence for the user's preferred tone.  Sequences
# that mix two different skin tones are not emitted.

import json
import re
import sys
import xml.etree.ElementTree as ET

# Must match the EmojiGroup enum in src/common/emoji-data.h.  "Component"
# entries (bare tone swatches, hair styles) are not meaningful to insert.
GROUPS = [
    "Smileys & Emotion",
    "People & Body",
    "Animals & Nature",
    "Food & Drink",
    "Travel & Places",
    "Activities",
    "Objects",
    "Symbols",
    "Flags",
]
SKIPPED_GROUPS = {"Component"}

# Order must match the EmojiTone enum (minus EMOJI_TONE_NONE).
TONE_DESCRIPTORS = [
    "light skin tone",
    "medium-light skin tone",
    "medium skin tone",
    "medium-dark skin tone",
    "dark skin tone",
]
TONE_MODIFIERS = set(range(0x1F3FB, 0x1F3FF + 1))
VS16 = 0xFE0F

DATA_LINE = re.compile(
    r"^(?P<cps>[0-9A-F ]+?)\s*;\s*(?P<status>[\w-]+)\s*#\s*(?P<emoji>\S+)"
    r"\s+E\d+\.\d+\s+(?P<name>.+)$"
)


def parse_emoji_test(path):
    """Yield (group, codepoints, string, name) for fully-qualified entries."""
    group = None
    unicode_version = None
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            line = line.rstrip("\n")
            if line.startswith("# group:"):
                group = line.split(":", 1)[1].strip()
                continue
            if line.startswith("# Version:"):
                unicode_version = line.split(":", 1)[1].strip()
                continue
            if not line or line.startswith("#"):
                continue
            m = DATA_LINE.match(line)
            if not m:
                continue
            if m.group("status") != "fully-qualified":
                continue
            if group in SKIPPED_GROUPS:
                continue
            if group not in GROUPS:
                sys.exit("gen-emoji-data.py: unknown emoji group %r; "
                         "update GROUPS and the EmojiGroup enum" % group)
            cps = [int(cp, 16) for cp in m.group("cps").split()]
            yield group, cps, "".join(map(chr, cps)), m.group("name")
    if unicode_version is None:
        sys.exit("gen-emoji-data.py: no '# Version:' header in emoji-test.txt")
    parse_emoji_test.version = unicode_version


def parse_annotations(path, keywords, names):
    root = ET.parse(path).getroot()
    for node in root.iter("annotation"):
        cp = node.attrib["cp"]
        if node.attrib.get("type") == "tts":
            names[cp] = node.text.strip()
        else:
            keywords[cp] = [k.strip() for k in node.text.split("|")]


def parse_versions(path):
    versions = {}
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                key, value = line.split("=", 1)
                versions[key.strip()] = value.strip()
    for key in ("unicode-emoji", "cldr", "gemoji"):
        if key not in versions:
            sys.exit("gen-emoji-data.py: %r missing from %s" % (key, path))
    return versions


def strip_vs16(text):
    return text.replace(chr(VS16), "")


def split_name(name):
    """Split 'kiss: woman, man, light skin tone' into a base name and the
    list of tone descriptors it contains."""
    if ": " not in name:
        return name, []
    head, qualifier = name.split(": ", 1)
    parts = [p.strip() for p in qualifier.split(",")]
    tones = [p for p in parts if p in TONE_DESCRIPTORS]
    rest = [p for p in parts if p not in TONE_DESCRIPTORS]
    base = head if not rest else head + ": " + ", ".join(rest)
    return base, tones


def c_escape(text):
    """Escape text as the body of a C string literal.  Non-ASCII bytes use
    three-digit octal escapes, which cannot swallow a following character
    the way hex escapes can."""
    out = []
    for byte in text.encode("utf-8"):
        if byte in (0x22, 0x5C):  # '"' and '\\'
            out.append("\\" + chr(byte))
        elif 0x20 <= byte < 0x7F:
            out.append(chr(byte))
        else:
            out.append("\\%03o" % byte)
    return "".join(out)


def build_search(name, keyword_list, aliases, tags):
    tokens = []
    for source in ([name] + keyword_list + tags):
        tokens.extend(source.casefold().split())
    tokens.extend(alias.casefold() for alias in aliases)
    seen = {}
    for token in tokens:
        token = token.strip('",')
        if token and token not in seen:
            seen[token] = True
    return " ".join(seen)


def main():
    if len(sys.argv) != 7:
        sys.exit("usage: gen-emoji-data.py EMOJI_TEST ANNOTATIONS "
                 "DERIVED_ANNOTATIONS GEMOJI VERSIONS OUTPUT")
    (emoji_test, ann_path, derived_path, gemoji_path,
     versions_path, output) = sys.argv[1:]

    versions = parse_versions(versions_path)
    cldr_version = versions["cldr"]

    keywords = {}
    cldr_names = {}
    parse_annotations(ann_path, keywords, cldr_names)
    parse_annotations(derived_path, keywords, cldr_names)

    aliases = {}
    tags = {}
    with open(gemoji_path, encoding="utf-8") as fp:
        for item in json.load(fp):
            if "emoji" not in item:
                continue
            key = strip_vs16(item["emoji"])
            aliases[key] = item.get("aliases", [])
            tags[key] = item.get("tags", [])

    entries = []           # base emoji, in CLDR order
    by_name = {}           # base name -> index into entries
    tone_sets = []         # list of [seq or None] * 5
    skipped_mixed = 0

    for group, cps, string, name in parse_emoji_test(emoji_test):
        tones_in_seq = {cp for cp in cps if cp in TONE_MODIFIERS}
        base_name, tone_descs = split_name(name)

        if not tones_in_seq:
            key = strip_vs16(string)
            entry = {
                "sequence": string,
                "name": name,
                "keywords": keywords.get(key, keywords.get(string, [])),
                "aliases": aliases.get(key, []),
                "tags": tags.get(key, []),
                "group": GROUPS.index(group),
                "tone_set": -1,
            }
            by_name[name] = len(entries)
            entries.append(entry)
            continue

        # Tone-modified sequence: attach to its base if it uses one uniform
        # tone; sequences mixing two tones are intentionally not bundled.
        if len(tones_in_seq) != 1 or len(tone_descs) != 1:
            skipped_mixed += 1
            continue
        if base_name not in by_name:
            sys.exit("gen-emoji-data.py: no base emoji %r for %r"
                     % (base_name, name))
        entry = entries[by_name[base_name]]
        if entry["tone_set"] == -1:
            entry["tone_set"] = len(tone_sets)
            tone_sets.append([None] * len(TONE_DESCRIPTORS))
        tone_sets[entry["tone_set"]][TONE_DESCRIPTORS.index(tone_descs[0])] = string

    for i, tone_set in enumerate(tone_sets):
        if None in tone_set:
            sys.exit("gen-emoji-data.py: incomplete tone set %d" % i)

    unicode_version = parse_emoji_test.version
    if unicode_version != versions["unicode-emoji"]:
        sys.exit("gen-emoji-data.py: emoji-test.txt is Emoji %s but VERSIONS "
                 "pins %s" % (unicode_version, versions["unicode-emoji"]))

    with open(output, "w", encoding="utf-8", newline="\n") as out:
        out.write("/*\n"
                  " * Generated by tools/gen-emoji-data.py -- do not edit.\n"
                  " *\n"
                  " * Sources (pinned copies in data/emoji/):\n"
                  " *   Unicode emoji-test.txt, Emoji %s\n"
                  " *   CLDR annotations (en), release %s\n"
                  " *   gemoji %s shortcode aliases\n"
                  " */\n\n" % (unicode_version, cldr_version,
                               versions["gemoji"]))

        out.write("static const EmojiToneSet emoji_tone_sets_table[] =\n{\n")
        for tone_set in tone_sets:
            out.write("\t{ { %s } },\n"
                      % ", ".join('"%s"' % c_escape(seq) for seq in tone_set))
        out.write("};\n\n")

        out.write("static const EmojiEntry emoji_entries_table[] =\n{\n")
        for entry in entries:
            search = build_search(entry["name"], entry["keywords"],
                                  entry["aliases"], entry["tags"])
            out.write('\t{ "%s", "%s", "%s", "%s", %d, %d },\n' % (
                c_escape(entry["sequence"]),
                c_escape(entry["name"]),
                c_escape(search),
                c_escape(" ".join(entry["aliases"])),
                entry["group"],
                entry["tone_set"],
            ))
        out.write("};\n\n")

        out.write('static const char emoji_data_unicode_version_str[] = "%s";\n'
                  % c_escape(unicode_version))
        out.write('static const char emoji_data_cldr_version_str[] = "%s";\n'
                  % c_escape(cldr_version))

    sys.stderr.write("gen-emoji-data.py: %d emoji, %d tone sets, "
                     "%d mixed-tone sequences skipped (Emoji %s, CLDR %s)\n"
                     % (len(entries), len(tone_sets), skipped_mixed,
                        unicode_version, cldr_version))


if __name__ == "__main__":
    main()
