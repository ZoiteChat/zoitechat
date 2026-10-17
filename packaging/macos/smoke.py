#!/usr/bin/env python3
"""Run the relocated bundle with temporary settings and no IRC auto-connect."""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="zoitechat-macos-smoke-") as directory:
        # Include spaces to exercise dyld and GTK module-cache path handling.
        app = Path(directory) / "Relocated App" / "ZoiteChat.app"
        shutil.copytree(args.app.resolve(), app)
        binary = app / "Contents/MacOS/zoitechat"
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
        subprocess.run([str(binary), "--version"], check=True, timeout=30)
        config = Path(directory) / "test-config"
        with (Path(directory) / "startup.log").open("w+") as log:
            process = subprocess.Popen([str(binary), "--no-auto", "--cfgdir", str(config)], stdout=log, stderr=log)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                # Stop the isolated process after checking for early startup failure.
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                    raise RuntimeError("GUI startup process did not terminate")
            else:
                log.seek(0)
                raise RuntimeError(f"GUI exited before smoke interval (code {process.returncode}):\n{log.read()}")
            log.seek(0)
            messages = log.read()
            print(messages)
            for failure in ("Library not loaded:", "Symbol not found:", "cannot open display", "Cannot load bundled HTTPS trust database", "Bundled emoji font is missing", "Cannot relocate", "Error loading plugin:"):
                if failure.lower() in messages.lower():
                    raise RuntimeError(f"Startup failure found: {failure}")
        print("Relocated bundle stayed running for 10 seconds; manual interaction testing is still required.")


if __name__ == "__main__":
    main()
