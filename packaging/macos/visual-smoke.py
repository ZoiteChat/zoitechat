#!/usr/bin/env python3
"""Exercise a relocated Mac bundle against loopback IRC and capture its window."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import secrets
import shutil
import socketserver
import subprocess
import tempfile
import threading
import time


class IRCServer(socketserver.ThreadingTCPServer):
    daemon_threads = True

    def __init__(self):
        self.joined = threading.Event()
        self.pong = threading.Event()
        self.error: str | None = None
        self.token = secrets.token_hex(12)
        self.transcript: list[str] = []
        super().__init__(("127.0.0.1", 0), IRCHandler)


class IRCHandler(socketserver.StreamRequestHandler):
    def handle(self):
        self.connection.settimeout(60)
        nick, user, registered, negotiating = "", False, False, False

        def send(line):
            self.server.transcript.append("S: " + line)
            self.wfile.write((line + "\r\n").encode("utf-8"))
            self.wfile.flush()

        try:
            # A TLS ClientHello is binary and need not contain a newline. Do
            # not leave the fixture waiting for an IRC line until its timeout.
            if self.rfile.peek(1).startswith(b"\x16"):
                self.server.error = "Received TLS on the plaintext loopback IRC fixture; launch with SERVER -insecure"
                self.server.transcript.append("Error: " + self.server.error)
                return
            while raw := self.rfile.readline(8192):
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                self.server.transcript.append("C: " + line)
                parts = line.split()
                if not parts:
                    continue
                command = parts[0].upper()
                if command == "CAP" and len(parts) > 1:
                    if parts[1].upper() == "LS":
                        negotiating = True
                        send(":ci.local CAP * LS :")
                    elif parts[1].upper() == "END":
                        negotiating = False
                    elif parts[1].upper() == "REQ":
                        send(":ci.local CAP * NAK :" + line.partition(" :")[2])
                elif command == "NICK" and len(parts) > 1:
                    nick = parts[1].lstrip(":")
                elif command == "USER":
                    user = True
                elif command == "PING" and len(parts) > 1:
                    send("PONG :" + parts[-1].lstrip(":"))
                elif command == "PONG" and parts[-1].lstrip(":") == self.server.token:
                    self.server.pong.set()
                elif command == "JOIN" and registered:
                    channel = parts[1].lstrip(":").split(",")[0]
                    send(f":{nick}!ci@localhost JOIN :{channel}")
                    send(f":ci.local 332 {nick} {channel} :ZoiteChat native macOS visual check")
                    send(f":ci.local 353 {nick} = {channel} :@CiGuide {nick}")
                    send(f":ci.local 366 {nick} {channel} :End of names")
                    for message in (
                        "ZoiteChat is running on macOS " + platform.machine() + ".",
                        "Connected to the temporary IRC server and joined this channel.",
                        "Unicode rendering: café — 日本語 — 👋",
                        "This screenshot is captured from the running application window.",
                    ):
                        send(f":CiGuide!ci@localhost PRIVMSG {channel} :{message}")
                    send("PING :" + self.server.token)
                    self.server.joined.set()
                elif command == "QUIT":
                    return
                if nick and user and not registered and not negotiating:
                    registered = True
                    send(f":ci.local 001 {nick} :Welcome to the ZoiteChat CI test server")
                    send(f":ci.local 005 {nick} CHANTYPES=# PREFIX=(ov)@+ NETWORK=ZoiteChat-CI :are supported")
                    send(f":ci.local 376 {nick} :End of MOTD")
        except (OSError, ValueError) as error:
            self.server.transcript.append("Connection closed: " + str(error))


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def check(app: Path, output: Path):
    report = {"status": "failed", "architecture": platform.machine(),
              "source_commit": os.environ.get("GITHUB_SHA"),
              "checks": [], "limitations": "Loopback IRC and window capture; no TLS, input, notifications or stability coverage."}
    with IRCServer() as server, tempfile.TemporaryDirectory(prefix="ZoiteChat visual check ") as temporary:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            directory = Path(temporary)
            capture = directory / "capture-window"
            subprocess.run(["xcrun", "swiftc", str(Path(__file__).with_name("capture-window.swift")),
                            "-o", str(capture)], check=True, timeout=120)
            relocated = directory / "Relocated App/ZoiteChat.app"
            shutil.copytree(app.resolve(), relocated)
            subprocess.run(["codesign", "--verify", "--deep", "--strict", str(relocated)], check=True, timeout=30)
            report["checks"].append("relocated bundle signature verified")
            config = directory / "config"
            config.mkdir()
            (config / "zoitechat.conf").write_text(
                "gui_slist_skip = 1\ngui_win_width = 1100\ngui_win_height = 700\n"
                "gui_win_left = 60\ngui_win_top = 60\n"
                "irc_nick1 = MacTester\nirc_user_name = MacTester\nirc_real_name = Visual check\n"
                "net_proxy_type = 0\nidentd_server = 0\n", encoding="utf-8")
            binary = relocated / "Contents/MacOS/zoitechat"
            with (output / "startup.log").open("w", encoding="utf-8") as log:
                # SERVER defaults to TLS even for an irc:// URL in this branch.
                # Explicit plaintext is restricted to our loopback-only fixture.
                command = f"SERVER -insecure -noproxy irc://127.0.0.1:{server.server_address[1]}/ci-smoke"
                process = subprocess.Popen([str(binary), "--no-auto", "--cfgdir", str(config),
                                            "--command", command],
                                           stdout=log, stderr=log)
                try:
                    deadline = time.monotonic() + 60
                    while not (server.joined.is_set() and server.pong.is_set()):
                        if server.error:
                            raise RuntimeError(server.error)
                        if process.poll() is not None:
                            raise RuntimeError(f"Application exited with code {process.returncode}; see startup.log")
                        if time.monotonic() >= deadline:
                            raise RuntimeError("Timed out waiting for channel join and PING/PONG; see irc.log")
                        time.sleep(0.2)
                    report["checks"] += ["connected to loopback IRC", "joined channel", "answered server PING"]
                    time.sleep(1)
                    window = subprocess.check_output([str(capture), str(process.pid), str(output / "zoitechat.png")],
                                                     text=True, timeout=40)
                    report["window"] = json.loads(window)
                    if process.poll() is not None:
                        raise RuntimeError("Application exited during capture")
                    report["checks"].append("captured a nonblank application window")
                    report["status"] = "passed"
                finally:
                    stop(process)
        except Exception as error:
            report["error"] = str(error)
            raise
        finally:
            server.shutdown()
            thread.join(timeout=5)
            (output / "irc.log").write_text("\n".join(server.transcript) + "\n", encoding="utf-8")
            (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("--output", type=Path, default=Path("dist/visual-check"))
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("This check must run on macOS with a logged-in desktop")
    args.output.mkdir(parents=True, exist_ok=False)
    check(args.app, args.output.resolve())
    print("Visual check passed; screenshot and evidence saved to " + str(args.output))


if __name__ == "__main__":
    main()
