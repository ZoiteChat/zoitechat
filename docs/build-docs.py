#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys


def prune_unused_local_assets(output_dir):
    index_path = os.path.join(output_dir, "index.html")

    try:
        with open(index_path, "r", encoding="utf-8") as index_file:
            index_html = index_file.read()
    except OSError:
        return

    if "bootswatch-" not in index_html:
        for bootstrap_version in ("bootstrap2", "bootstrap3"):
            css_dir = os.path.join(
                output_dir, "_static", "css", bootstrap_version
            )
            if not os.path.isdir(css_dir):
                continue

            for filename in os.listdir(css_dir):
                if filename.startswith("bootswatch-") and filename.endswith(".css"):
                    os.remove(os.path.join(css_dir, filename))

    if "jquery.cookie.min.js" not in index_html:
        js_dir = os.path.join(output_dir, "_static", "js")
        for filename in ("jquery.cookie.min.js", "jquery.cookie-1.4.1.min.js"):
            path = os.path.join(js_dir, filename)
            if os.path.isfile(path):
                os.remove(path)


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: build-docs.py SOURCE_DIR OUTPUT_DIR STAMP_FILE")

    source_dir = os.path.abspath(sys.argv[1])
    output_dir = os.path.abspath(sys.argv[2])
    stamp_file = os.path.abspath(sys.argv[3])
    doctree_dir = output_dir + ".doctrees"

    shutil.rmtree(output_dir, ignore_errors=True)
    shutil.rmtree(doctree_dir, ignore_errors=True)
    os.makedirs(output_dir, exist_ok=True)

    subprocess.run(
        [
            sys.executable,
            "-m",
            "sphinx",
            "-b",
            "html",
            "-d",
            doctree_dir,
            source_dir,
            output_dir,
        ],
        check=True,
    )

    prune_unused_local_assets(output_dir)

    with open(stamp_file, "w", encoding="utf-8") as stamp:
        stamp.write("built\n")


if __name__ == "__main__":
    main()
