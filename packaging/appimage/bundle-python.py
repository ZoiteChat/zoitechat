#!/usr/bin/env python3
"""Copy the private interpreter, stdlib and declared plugin dependencies."""

import importlib.metadata
from pathlib import Path
import shutil
import sys
import sysconfig


def main():
    destination = Path(sys.argv[1]).resolve() / "usr"
    version = f"{sys.version_info.major}.{sys.version_info.minor}"
    stdlib = Path(sysconfig.get_path("stdlib"))
    target_stdlib = destination / "lib" / f"python{version}"
    target_stdlib.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(
        stdlib, target_stdlib,
        ignore=shutil.ignore_patterns("site-packages", "__pycache__", "*.a", "test", "tests"),
    )
    (destination / "bin").mkdir(parents=True, exist_ok=True)
    shutil.copy2(sys.executable, destination / "bin" / "python3")
    libdir = Path(sysconfig.get_config_var("LIBDIR"))
    library = sysconfig.get_config_var("LDLIBRARY")
    for source in libdir.glob(f"{library}*"):
        # Dereference the build prefix's symlinks so none point outside AppDir.
        shutil.copy2(source, destination / "lib" / source.name)

    site = target_stdlib / "site-packages"
    site.mkdir()
    for name in ("cffi", "pycparser"):
        distribution = importlib.metadata.distribution(name)
        for entry in distribution.files:
            # Wheel metadata and the native backend belong to the runtime;
            # scripts installed outside site-packages belong to the build.
            if ".." in entry.parts or entry.suffix == ".pyc":
                continue
            source = Path(distribution.locate_file(entry))
            target = site / entry
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    # _tkinter also needs Tcl/Tk's scripts, not just its ELF dependencies.
    for name in ("tcl8.6", "tk8.6"):
        shutil.copytree(Path("/usr/share/tcltk") / name,
                        destination / "share/tcltk" / name)


if __name__ == "__main__":
    main()
