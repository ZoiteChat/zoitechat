#!/usr/bin/env python3
"""Compile, test, stage and package ZoiteChat on the host Mac."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

from bundle import ROOT, package, run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build-macos"))
    parser.add_argument("--output", type=Path, default=Path("dist/ZoiteChat.app"))
    parser.add_argument("--build-number", default="1")
    parser.add_argument("--store-experimental", action="store_true", help="Build a separate sandbox test variant without loadable plugins; not submission approval")
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("Build on macOS; cross-compiling from Linux is not supported")
    if args.output.exists():
        parser.error("Output already exists; use a new --output or explicitly remove the old app")
    build = args.build_dir.resolve()
    if build == ROOT or ROOT.is_relative_to(build):
        parser.error("Build directory must not contain the source checkout")
    run("xcrun", "--find", "clang")
    env = os.environ.copy()
    prefixes = [Path(run("brew", "--prefix", name)) for name in ("gettext", "openssl@3", "libarchive", "lua", "gtk+3", "gtk-mac-integration")]
    brew = Path(run("brew", "--prefix"))
    # Keg-only gettext, OpenSSL and libarchive are not necessarily in PATH/pkg-config.
    env["PATH"] = os.pathsep.join([str(p / "bin") for p in prefixes] + [str(brew / "bin"), env.get("PATH", "")])
    env["PKG_CONFIG_PATH"] = os.pathsep.join([str(p / "lib/pkgconfig") for p in prefixes] + [str(brew / "lib/pkgconfig"), str(brew / "share/pkgconfig"), env.get("PKG_CONFIG_PATH", "")])
    # Native dependency bottles determine the true deployment target; the bundle
    # auditor derives the final minimum rather than promising an arbitrary old OS.
    env.setdefault("MACOSX_DEPLOYMENT_TARGET", "11.0")
    backend = run("pkg-config", "--variable=targets", "gtk+-3.0", env=env).split()
    if "quartz" not in backend:
        parser.error(f"GTK must provide the native Quartz backend, got {backend}")
    # Do not silently select an XQuartz GTK stack or mix Intel/ARM Homebrew prefixes.
    machine = platform.machine()
    if machine not in ("arm64", "x86_64"):
        parser.error(f"Unsupported architecture: {machine}")
    options = [
        "--prefix=/", "--libdir=lib", "--buildtype=release", "-Dgtk-frontend=true",
        "-Dtext-frontend=false", "-Dmac-integration=enabled", "-Ddbus=disabled",
        "-Dlibcanberra=disabled", "-Dappindicator=disabled", "-Dinstall-appdata=false",
        "-Dwith-sysinfo=false", "-Dwith-python=false", "-Dwith-perl=false",
        "-Dwith-lua=lua", "-Dwith-fishlim=true", "-Dwith-checksum=true",
        "-Dplugin=" + ("false" if args.store_experimental else "true"),
    ]
    if (build / "meson-private/coredata.dat").exists():
        options.insert(0, "--reconfigure")
    subprocess.run(["meson", "setup", str(build), str(ROOT), *options], env=env, check=True)
    subprocess.run(["meson", "compile", "-C", str(build)], env=env, check=True)
    subprocess.run(["meson", "test", "-C", str(build), "--print-errorlogs"], env=env, check=True)
    stage = build / "macos-stage"
    if stage.exists():
        shutil.rmtree(stage)
    subprocess.run(["meson", "install", "-C", str(build), "--destdir", str(stage)], env=env, check=True)
    package(stage, args.output.resolve(), args.build_number)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError) as error:
        sys.exit(f"macOS build failed: {error}")
