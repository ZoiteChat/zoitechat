#!/usr/bin/env python3

import os
import shutil
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: install-docs.py STAMP_FILE SOURCE_DIR DATADIR")

    _, source_dir, datadir = sys.argv[1:]
    source_dir = os.path.abspath(source_dir)

    if not os.path.isfile(sys.argv[1]):
        raise SystemExit("documentation build did not produce its stamp file")
    if not os.path.isfile(os.path.join(source_dir, "index.html")):
        raise SystemExit("documentation build did not produce index.html")

    if os.path.isabs(datadir):
        destdir = os.environ.get("DESTDIR", "")
        if destdir:
            data_root = os.path.join(destdir, datadir.lstrip("/\\"))
        else:
            data_root = datadir
    else:
        data_root = os.path.join(os.environ["MESON_INSTALL_DESTDIR_PREFIX"], datadir)

    destination = os.path.join(data_root, "doc", "zoitechat", "html")
    shutil.rmtree(destination, ignore_errors=True)
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    shutil.copytree(source_dir, destination)


if __name__ == "__main__":
    main()
