#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys


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

    with open(stamp_file, "w", encoding="utf-8") as stamp:
        stamp.write("built\n")


if __name__ == "__main__":
    main()
