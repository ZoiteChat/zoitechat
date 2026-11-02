#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Real-core Qt DCC loopback test. Requires a plugin-enabled Linux Qt build."""
import argparse
import os
from pathlib import Path
import shlex
import socket
import struct
import subprocess
import tempfile
import threading
import time


def run(binary):
    root = Path(__file__).resolve().parents[2]
    payload = bytes(range(256)) * 257
    failures, events, listeners, transcript = [], {}, [], []
    passed = False
    stop = threading.Event()
    lock = threading.Lock()

    def listener():
        sock = socket.socket()
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("127.0.0.1", 0))
        sock.listen(4)
        sock.settimeout(12)
        listeners.append(sock)
        return sock

    def task(name, function):
        event = events.setdefault(name, threading.Event())
        def execute():
            try:
                function()
                event.set()
            except Exception as error:
                if not stop.is_set():
                    failures.append(f"{name}: {error}")
        threading.Thread(target=execute, daemon=True).start()

    def receive_file(connection, offset=0):
        connection.settimeout(10)
        received = bytearray(payload[:offset])
        with connection:
            while len(received) < len(payload):
                chunk = connection.recv(8192)
                if not chunk:
                    break
                received.extend(chunk)
                connection.sendall(struct.pack("!I", len(received)))
        assert bytes(received) == payload, f"outbound payload mismatch ({len(received)} bytes)"

    def send_file(connection, offset=0):
        connection.settimeout(10)
        with connection:
            connection.sendall(payload[offset:])
            buffered = bytearray()
            acknowledged = offset
            while acknowledged < len(payload):
                chunk = connection.recv(4096)
                if not chunk:
                    break
                buffered.extend(chunk)
                while len(buffered) >= 4:
                    acknowledged = struct.unpack("!I", buffered[:4])[0]
                    del buffered[:4]
            assert acknowledged == len(payload), f"receiver acknowledged {acknowledged} bytes"

    with tempfile.TemporaryDirectory(prefix="zoitechat-qt-dcc-runtime-") as temporary:
        folder = Path(temporary)
        config, downloads = folder / "config", folder / "downloads"
        addons = config / "addons"
        addons.mkdir(parents=True)
        downloads.mkdir()
        system_addons = folder / "empty-system-addons"
        system_addons.mkdir()
        outgoing, passive_outgoing = folder / "outgoing.bin", folder / "outgoing-passive.bin"
        resume_outgoing, abort_outgoing = folder / "outgoing-resume.bin", folder / "outgoing-abort.bin"
        resume_outgoing.write_bytes(payload)
        abort_outgoing.write_bytes(payload)
        outgoing.write_bytes(payload)
        passive_outgoing.write_bytes(payload)
        (downloads / "resume.bin").write_bytes(payload[:4096])
        (config / "zoitechat.conf").write_text(
            f"dcc_dir = {downloads}\ndcc_completed_dir = \nidentd_server = 0\ndcc_ip = 127.0.0.1\ndcc_ip_from_server = 0\n"
            "dcc_auto_chat = 1\ndcc_auto_recv = 0\ndcc_auto_resume = 0\ndcc_save_nick = 0\ndcc_remove = 0\n"
            "dcc_blocksize = 4096\ndcc_fast_send = 1\nnet_throttle = 0\n"
            "gui_autoopen_recv = 0\ngui_autoopen_send = 0\ngui_autoopen_chat = 0\n"
            "gui_slist_skip = 1\ngui_tab_server = 1\n"
        )
        compiler = shlex.split(os.environ.get("CC", "cc"))
        includes = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "gio-2.0"], text=True))
        subprocess.run([*compiler, *includes, "-I" + str(binary.parents[2]), "-shared", "-fPIC", "-std=c11", "-I" + str(root / "src/common"),
                        "-I" + str(root / "src/fe-qt"), str(root / "src/fe-qt/dcc-runtime-probe.c"),
                        "-o", str(addons / "dcc-runtime.so")], check=True, capture_output=True, text=True)
        irc = listener()
        normal_receive, resume_receive, incoming_chat, abort_receive = listener(), listener(), listener(), listener()
        send_resume_ports = {}
        irc_connection = [None]
        nickname = ["qt-test"]
        def line(text):
            with lock:
                irc_connection[0].sendall((text + "\r\n").encode())
        def ctcp(text, sender="peer"):
            line(f":{sender}!test@localhost PRIVMSG {nickname[0]} :\x01{text}\x01")
        def abort_file():
            connection, _ = abort_receive.accept()
            with connection:
                connection.settimeout(10)
                connection.sendall(payload[:1024])
                while connection.recv(4096):
                    pass
        def exchange_chat(direct, suffix=b""):
            with direct:
                direct.settimeout(10)
                with direct.makefile("rb") as messages:
                    assert messages.readline().rstrip(b"\r\n") == b"from-client" + suffix, "wrong DCC chat payload"
                    direct.sendall(b"from-peer" + suffix + b"\n")
                    stop.wait(10)
        def peer():
            connection, _ = irc.accept()
            connection.settimeout(12)
            irc_connection[0] = connection
            user, nick, registered = False, False, False
            with connection, connection.makefile("rb") as stream:
                for raw in stream:
                    text = raw.decode(errors="replace").rstrip("\r\n")
                    transcript.append(text)
                    words = text.split()
                    if not words:
                        continue
                    if words[0] == "CAP" and "LS" in words:
                        line(":local CAP * LS :")
                    elif words[0] == "NICK":
                        nickname[0] = words[1].lstrip(":")
                        nick = True
                    elif words[0] == "USER":
                        user = True
                    elif words[0] == "PING":
                        line("PONG " + " ".join(words[1:]))
                    if nick and user and not registered:
                        registered = True
                        line(f":local 001 {nickname[0]} :Welcome")
                        line(f":local 005 {nickname[0]} CHANTYPES=# NICKLEN=30 :are supported")
                        line(f":local 376 {nickname[0]} :End of MOTD")
                        ctcp(f'DCC SEND "receive file.bin" 2130706433 {normal_receive.getsockname()[1]} {len(payload)}')
                        ctcp(f'DCC SEND resume.bin 2130706433 {resume_receive.getsockname()[1]} {len(payload)}')
                        ctcp(f"DCC SEND passive.bin 199 0 {len(payload)} 4242")
                        ctcp(f"DCC SEND active-abort.bin 2130706433 {abort_receive.getsockname()[1]} {len(payload)}")
                        task("active abort", abort_file)
                        ctcp(f"DCC SEND decline.bin 2130706433 {normal_receive.getsockname()[1]} 1")
                        ctcp(f"DCC CHAT chat 2130706433 {incoming_chat.getsockname()[1]}", "peer-incoming")
                        task("incoming chat", lambda: exchange_chat(incoming_chat.accept()[0], b"-incoming"))
                        task("normal receive", lambda: send_file(normal_receive.accept()[0]))
                    if " :\x01DCC " not in text:
                        continue
                    data = shlex.split(text.split(" :\x01", 1)[1].rstrip("\x01"))
                    if data[1] == "SEND":
                        name, port = Path(data[2]).name, int(data[4])
                        if name == "outgoing.bin":
                            task("normal send", lambda port=port: receive_file(socket.create_connection(("127.0.0.1", port), timeout=10)))
                        elif name == "outgoing-resume.bin":
                            send_resume_ports[name] = port
                            ctcp(f"DCC RESUME {name} {port} 4096")
                        elif name == "outgoing-passive.bin":
                            assert port == 0, "outgoing passive offer must not advertise a listen port"
                            incoming = listener()
                            ctcp(f"DCC SEND {name} 2130706433 {incoming.getsockname()[1]} {len(payload)} {data[6]}")
                            task("passive send", lambda: receive_file(incoming.accept()[0]))
                        elif name == "passive.bin":
                            assert port > 0 and data[6] == "4242", "passive response must preserve token"
                            task("passive receive", lambda port=port: send_file(socket.create_connection(("127.0.0.1", port), timeout=10)))
                    elif data[1] == "RESUME":
                        assert data[2] == "resume.bin" and int(data[4]) == 4096, "incorrect resume offset"
                        ctcp(f"DCC ACCEPT resume.bin {data[3]} {data[4]}")
                        task("resume receive", lambda: send_file(resume_receive.accept()[0], 4096))
                    elif data[1] == "ACCEPT":
                        assert data[2] == "outgoing-resume.bin" and int(data[4]) == 4096
                        port = send_resume_ports[data[2]]
                        task("resume send", lambda port=port: receive_file(socket.create_connection(("127.0.0.1", port), timeout=10), 4096))
                    elif data[1] == "CHAT":
                        port = int(data[4])
                        if port == 0:
                            passive_chat = listener()
                            ctcp(f"DCC CHAT chat 2130706433 {passive_chat.getsockname()[1]} {data[5]}", "peer-passive")
                            task("passive chat", lambda: exchange_chat(passive_chat.accept()[0], b"-passive"))
                        else:
                            task("chat", lambda port=port: exchange_chat(socket.create_connection(("127.0.0.1", port), timeout=10)))
        task("IRC peer", peer)
        report = folder / "report.txt"
        environment = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "ZOITECHAT_LIBDIR": str(system_addons), "QT_DCC_PORT": str(irc.getsockname()[1]),
                       "QT_DCC_SEND": str(outgoing), "QT_DCC_RECEIVE_DEST": str(downloads / "chosen destination file.bin"), "QT_DCC_PASSIVE_SEND": str(passive_outgoing), "QT_DCC_RESUME_SEND": str(resume_outgoing), "QT_DCC_ABORT_SEND": str(abort_outgoing), "QT_DCC_REPORT": str(report), "QT_DCC_TRACE": str(folder / "trace.txt")}
        process = subprocess.Popen([str(binary), "--no-auto", "--cfgdir", str(config)], env=environment,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            deadline = time.monotonic() + 18
            while time.monotonic() < deadline:
                if failures:
                    raise AssertionError("; ".join(failures))
                if process.poll() is not None:
                    raise AssertionError(f"Qt process exited early: {process.communicate()[0]}")
                if report.exists():
                    result = report.read_text()
                    assert result.startswith("PASS"), result
                    required = ["normal send", "passive send", "normal receive", "passive receive", "resume receive", "resume send", "active abort"]
                    if all(events.get(name) and events[name].is_set() for name in required):
                        for name in ["chosen destination file.bin", "resume.bin", "passive.bin"]:
                            assert (downloads / name).read_bytes() == payload, f"download mismatch: {name}"
                        assert (downloads / "active-abort.bin").read_bytes() == payload[:1024], "active abort did not preserve the expected partial file"
                        assert not (downloads / "receive file.bin").exists(), "explicit receive destination was ignored"
                        assert not (downloads / "decline.bin").exists(), "declined offer created a file"
                        passed = True
                        print("PASS: real Qt/core normal+passive transfers, explicit destination, send+receive resume, normal+passive+incoming chat, queued+active abort/decline/remove/stale-ID checks")
                        break
                time.sleep(0.02)
            else:
                raise AssertionError("Timed out waiting for DCC runtime report")
        finally:
            stop.set()
            process.terminate()
            output, _ = process.communicate(timeout=5)
            for sock in listeners:
                sock.close()
            if not passed:
                print("IRC transcript:\n" + "\n".join(transcript))
                trace = folder / "trace.txt"
                print("Probe trace:\n" + (trace.read_text() if trace.exists() else "not initialized"))
                print(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="plugin-enabled build-tree zoitechat-qt executable")
    args = parser.parse_args()
    run(args.binary.resolve())
