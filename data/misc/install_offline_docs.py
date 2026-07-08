#!/usr/bin/env python3
"""Meson install helper: copy the built offline docs into the install tree.

Silently skips when the docs were not built (offline-docs=auto without
network); Help->Contents then falls back to the online manual.
"""

import os
import pathlib
import shutil
import sys

src = pathlib.Path(sys.argv[1])
dest_arg = pathlib.PurePath(sys.argv[2])

if not (src / "index.html").is_file():
    print("offline documentation was not built; skipping install")
    sys.exit(0)

if dest_arg.is_absolute():
    # An absolute offline-docs-dir lands outside the prefix; honour DESTDIR
    destdir = os.environ.get("DESTDIR", "")
    dest = pathlib.Path(destdir + str(dest_arg)) if destdir else pathlib.Path(dest_arg)
else:
    dest = pathlib.Path(os.environ["MESON_INSTALL_DESTDIR_PREFIX"]) / dest_arg

if dest.exists():
    shutil.rmtree(dest)
dest.parent.mkdir(parents=True, exist_ok=True)
shutil.copytree(src, dest)
print(f"Installed offline documentation to {dest}")
