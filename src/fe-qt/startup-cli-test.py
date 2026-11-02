#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check startup argument handling without opening GUI windows or user config."""
import pathlib
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="zoitechat-qt-cli-") as directory:
    def invoke(*args):
        return subprocess.run([binary, "--cfgdir", directory, *args], capture_output=True, text=True, timeout=10)
    for level in (0, 1, 2):
        result = invoke("--minimize", str(level), "--version")
        assert result.returncode == 0 and "Qt 6" in result.stdout, (level, result)
    for level in (-1, 3, 999):
        result = invoke("--minimize", str(level), "--version")
        assert result.returncode != 0 and "--minimize must be 0, 1, or 2" in result.stderr, (level, result)
    result = invoke("--help")
    assert result.returncode == 0 and "--minimize" in result.stdout, result
