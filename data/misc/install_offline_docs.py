#!/usr/bin/env python3
import os
import pathlib
import shutil
import sys

src = pathlib.Path(sys.argv[1])
dest = pathlib.Path(os.environ['MESON_INSTALL_DESTDIR_PREFIX']) / sys.argv[2]
if not (src / 'index.html').exists():
    sys.exit(0)
if dest.exists():
    shutil.rmtree(dest)
shutil.copytree(src, dest)
