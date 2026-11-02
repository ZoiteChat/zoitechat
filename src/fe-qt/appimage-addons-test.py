#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check the real AppImage's default addon autoload in an isolated config."""
import os
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import tempfile
import time

image = Path(sys.argv[1]).resolve()
source = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="zoitechat-appimage-addons-") as temporary:
    config = Path(temporary)
    addons = config / "addons"
    addons.mkdir()
    (config / "zoitechat.conf").write_text("gui_slist_skip = 1\nidentd_server = 0\n")
    subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-shared", "-fPIC",
                    "-I" + str(source / "src/common"), "-I" + str(source / "src/fe-qt"),
                    str(source / "src/fe-qt/appimage-addons-probe.c"), "-o", str(addons / "audit.so")], check=True)
    report = config / "report"
    environment = {**os.environ, "APPIMAGE_EXTRACT_AND_RUN": "1", "QT_QPA_PLATFORM": "xcb",
                   "QT_APPIMAGE_ADDONS_REPORT": str(report)}
    environment.pop("ZOITECHAT_LIBDIR", None)
    environment.pop("OPENSSL_MODULES", None)
    process = subprocess.Popen([str(image), "--no-auto", "--cfgdir", str(config)], env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, start_new_session=True)
    success = False
    try:
        deadline = time.monotonic() + 15
        result = "timeout"
        while time.monotonic() < deadline:
            if report.exists():
                result = report.read_text()
                if result:
                    break
            assert process.poll() is None, "AppImage exited early"
            time.sleep(0.02)
        assert result.startswith("PASS"), result
        success = True
        print(result.strip())
    finally:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            output, _ = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            output, _ = process.communicate(timeout=5)
        if not success:
            print(output)
