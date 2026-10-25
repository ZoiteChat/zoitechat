#!/usr/bin/env python3
import sys
import urllib.request
from pathlib import Path

URLS = (
    "https://raw.githubusercontent.com/publicsuffix/list/main/public_suffix_list.dat",
    "https://publicsuffix.org/list/public_suffix_list.dat",
)


def parse_rules(text: str):
    rules = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("//"):
            continue
        if " " in line or "\t" in line:
            line = line.split()[0]
        rules.append(line.lower())
    return sorted(set(rules))


def emit_header(path: str, rules):
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("#pragma once\n")
        out.write("static const char * const public_suffix_rules[] = {\n")
        for rule in rules:
            escaped = rule.replace("\\", "\\\\").replace('"', '\\"')
            out.write(f'\t"{escaped}",\n')
        out.write("};\n")
        out.write(
            "static const unsigned int public_suffix_rules_len = sizeof(public_suffix_rules) / sizeof(public_suffix_rules[0]);\n"
        )


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit("usage: gen-public-suffix.py <output> [source]")
    output = Path(sys.argv[1])
    if len(sys.argv) == 3:
        # An explicit source is authoritative, don't go online behind its back.
        rules = parse_rules(Path(sys.argv[2]).read_text(encoding="utf-8"))
        emit_header(str(output), rules)
        return
    sources = [
        Path(__file__).with_name("public_suffix_list.dat"),
        Path("/usr/share/publicsuffix/public_suffix_list.dat"),
        Path("/app/share/publicsuffix/public_suffix_list.dat"),
    ]
    data = None
    for url in URLS:
        try:
            with urllib.request.urlopen(url, timeout=30) as resp:
                data = resp.read().decode("utf-8")
            break
        except Exception:
            pass
    if data is None:
        for source in sources:
            if source.exists():
                data = source.read_text(encoding="utf-8")
                break
    if data is None:
        raise SystemExit("unable to load public suffix list")
    rules = parse_rules(data)
    emit_header(str(output), rules)


if __name__ == "__main__":
    main()
