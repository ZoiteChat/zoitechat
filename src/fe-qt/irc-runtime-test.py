#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Real Qt/C-engine IRC, saved-network TLS/SASL and reconnect regression."""
import base64
import argparse
import hashlib
import hmac
import os
from pathlib import Path
import shlex
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time


def run(binary, tls_mode="", recovery="", build=None, bundled_addons=False):
    build = build or binary.parents[2]
    root = Path(__file__).resolve().parents[2]
    transcript, failures = [], []
    finished = threading.Event()
    stopping = threading.Event()
    reject_tls = tls_mode in {"reject", "mismatch"}
    scram = tls_mode.startswith("scram-")
    bad_scram = tls_mode in {"scram-bad-signature", "scram-bad-prefix", "scram-bad-separator"}
    digest = "sha" + tls_mode.removeprefix("scram-") if scram and not bad_scram else "sha256"
    mechanism = "SCRAM-SHA-" + digest[3:] if scram else "EXTERNAL" if tls_mode == "external" else "PLAIN"
    with tempfile.TemporaryDirectory(prefix="zoitechat-qt-irc-runtime-") as temporary:
        folder = Path(temporary)
        config = folder / "config"
        addons = config / "addons"
        addons.mkdir(parents=True)
        system_addons = folder / "empty-system-addons"
        system_addons.mkdir()
        (config / "zoitechat.conf").write_text(
            "gui_slist_skip = 1\ngui_tab_server = 1\nidentd_server = 0\n"
            f"net_throttle = 0\nnet_auto_reconnect = {int(recovery in {'drop', 'cancel'})}\n"
            f"net_auto_reconnectonfail = {int(recovery == 'refused')}\nirc_reconnect_rejoin = 1\n"
            "net_reconnect_delay = 1\nirc_join_delay = 0\nirc_logging = 0\ntext_replay = 0\n"
        )
        compiler = shlex.split(os.environ.get("CC", "cc"))
        includes = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "glib-2.0"], text=True))
        compiled = subprocess.run([*compiler, *includes, "-I" + str(build), "-shared", "-fPIC", "-std=c11",
                        "-I" + str(root / "src/common"), "-I" + str(root / "src/fe-qt"),
                        str(root / "src/fe-qt/irc-runtime-probe.c"), "-o", str(addons / "irc-runtime.so")],
                       capture_output=True, text=True)
        assert compiled.returncode == 0, "IRC probe compilation failed:\n" + compiled.stderr
        tls = None
        if tls_mode:
            key, certificate = folder / "server.key", folder / "server.crt"
            certificate_host = "127.0.0.2" if tls_mode == "mismatch" else "127.0.0.1"
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", "/CN=" + certificate_host, "-addext", "subjectAltName=IP:" + certificate_host,
                            "-keyout", str(key), "-out", str(certificate)], check=True, capture_output=True)
            tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            tls.minimum_version = ssl.TLSVersion.TLSv1_2
            tls.load_cert_chain(certificate, key)
            if tls_mode == "external":
                certs = config / "certs"
                certs.mkdir()
                (certs / "Qt runtime TLS.pem").write_bytes(certificate.read_bytes() + key.read_bytes())
                tls.load_verify_locations(cafile=str(certificate))
                tls.verify_mode = ssl.CERT_REQUIRED
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            if recovery != "refused":
                listener.listen(2)
            listener.settimeout(15)

            def peer():
                try:
                    if recovery == "refused":
                        deadline = time.monotonic() + 10
                        while time.monotonic() < deadline:
                            if report.exists() and "connection_failed=1" in report.read_text():
                                break
                            time.sleep(0.02)
                        else:
                            raise AssertionError("No initial connection failure observed")
                        listener.listen(2)
                    for iteration in ((1,) if reject_tls or recovery in {"cancel", "refused"} else (1, 2)):
                        connection, _ = listener.accept()
                        connection.settimeout(12)
                        if tls:
                            try:
                                connection = tls.wrap_socket(connection, server_side=True)
                            except ssl.SSLError as error:
                                # ZoiteChat verifies certificates during the handshake.
                                # A fatal client alert is the expected rejection path.
                                if reject_tls and error.reason in {
                                    "TLSV1_ALERT_UNKNOWN_CA", "SSLV3_ALERT_BAD_CERTIFICATE",
                                    "SSLV3_ALERT_CERTIFICATE_UNKNOWN",
                                }:
                                    continue
                                raise
                            assert connection.cipher(), "TLS connection has no negotiated cipher"
                            if tls_mode == "external":
                                assert connection.getpeercert(), "EXTERNAL did not present a client certificate"
                        nickname = None
                        user = registered = cap_end = authenticated = False
                        seen = set()
                        scram_step = 0
                        client_first = server_first = nonce = ""
                        with connection, connection.makefile("rb") as stream:
                            def send(text):
                                connection.sendall((text + "\r\n").encode())

                            for raw in stream:
                                line = raw.decode().rstrip("\r\n")
                                transcript.append(f"{iteration}: {line}")
                                words = line.split()
                                if not words:
                                    continue
                                command = words[0].upper()
                                assert not reject_tls, "Invalid TLS sent IRC data"
                                if command == "CAP" and "LS" in words:
                                    send(":local CAP * LS :" + ("sasl=" + mechanism if tls else ""))
                                elif command == "CAP" and "REQ" in words:
                                    assert tls and words[-1].lstrip(":") == "sasl", line
                                    send(f":local CAP {nickname or '*'} ACK :sasl")
                                elif command == "CAP" and "END" in words:
                                    assert not tls or authenticated or ((tls_mode == "sasl-denied" or bad_scram) and "sasl-aborted" in seen), "CAP ended before SASL completed"
                                    cap_end = True
                                elif command == "AUTHENTICATE":
                                    assert tls, "SASL credentials sent on plaintext fixture"
                                    if words[1] == mechanism:
                                        send("AUTHENTICATE +")
                                    elif words[1] == "*":
                                        assert (tls_mode == "sasl-denied" and "sasl-denied" in seen) or (bad_scram and scram_step == 2), "Unexpected SASL abort"
                                        seen.add("sasl-aborted")
                                        if bad_scram:
                                            send(f":local 906 {nickname} :SASL authentication aborted")
                                    elif scram:
                                        payload = b"" if words[1] == "+" else base64.b64decode(words[1], validate=True)
                                        if scram_step == 0:
                                            first = payload.decode()
                                            assert first.startswith("n,,n=runtime-user,r="), "Wrong SCRAM identity or GS2 header"
                                            client_first = first[3:]
                                            nonce = first.split(",r=", 1)[1] + "runtime-server-nonce"
                                            server_first = "r=" + nonce + ",s=" + base64.b64encode(b"runtime-salt").decode() + ",i=4096"
                                            send("AUTHENTICATE " + base64.b64encode(server_first.encode()).decode())
                                        elif scram_step == 1:
                                            final, proof = payload.decode().rsplit(",p=", 1)
                                            assert final == "c=biws,r=" + nonce, "Wrong SCRAM channel binding or nonce"
                                            auth = (client_first + "," + server_first + "," + final).encode()
                                            salted = hashlib.pbkdf2_hmac(digest, b"runtime-password", b"runtime-salt", 4096)
                                            client_key = hmac.digest(salted, b"Client Key", digest)
                                            signature = hmac.digest(hashlib.new(digest, client_key).digest(), auth, digest)
                                            expected = bytes(a ^ b for a, b in zip(client_key, signature))
                                            assert hmac.compare_digest(base64.b64decode(proof, validate=True), expected), "Invalid SCRAM client proof"
                                            server_key = hmac.digest(salted, b"Server Key", digest)
                                            verifier = hmac.digest(server_key, auth, digest)
                                            if tls_mode == "scram-bad-signature":
                                                verifier = bytes([verifier[0] ^ 1]) + verifier[1:]
                                            prefix = "x=" if tls_mode == "scram-bad-prefix" else "v!" if tls_mode == "scram-bad-separator" else "v="
                                            response = prefix + base64.b64encode(verifier).decode()
                                            send("AUTHENTICATE " + base64.b64encode(response.encode()).decode())
                                        else:
                                            assert not bad_scram, "Client accepted invalid SCRAM server verification"
                                            assert words[1] == "+", "Missing SCRAM completion"
                                            authenticated = True
                                            seen.add("sasl")
                                            send(f":local 903 {nickname} :SASL authentication successful")
                                        scram_step += 1
                                    else:
                                        payload = b"" if words[1] == "+" else base64.b64decode(words[1], validate=True)
                                        if tls_mode == "external":
                                            assert words[1] == "+" and payload == b"", "EXTERNAL leaked password or sent an unexpected authorization identity"
                                        else:
                                            assert payload == b"runtime-user\0runtime-user\0runtime-password", "Wrong SASL credentials"
                                        if tls_mode == "sasl-denied":
                                            seen.add("sasl-denied")
                                            send(f":local 904 {nickname} :SASL authentication failed")
                                        else:
                                            authenticated = True
                                            seen.add("sasl")
                                            send(f":local 903 {nickname} :SASL authentication successful")
                                elif command == "NICK":
                                    nickname = words[1].lstrip(":")
                                elif command == "USER":
                                    user = True
                                elif command == "PING":
                                    send("PONG " + " ".join(words[1:]))
                                elif command == "PONG":
                                    assert words[-1].lstrip(":") == "runtime-ping"
                                    seen.add("pong")
                                elif command == "JOIN":
                                    assert registered and words[1] == "#port", line
                                    seen.add("join")
                                    send(f":{nickname}!test@localhost JOIN :#port")
                                    send(f":local 353 {nickname} = #port :@{nickname} @peer")
                                    send(f":local 366 {nickname} #port :End of NAMES")
                                    send(f":local 324 {nickname} #port +nt")
                                    send(f":local 332 {nickname} #port :initial topic")
                                elif command == "MODE" and len(words) > 2 and words[2] == "+m":
                                    assert iteration == 1 and "join" in seen, line
                                    if "mode-denied" not in seen:
                                        seen.add("mode-denied")
                                        send(f":local 482 {nickname} #port :You are not a channel operator")
                                    else:
                                        seen.add("mode")
                                        send(f":{nickname}!test@localhost MODE #port +m")
                                elif command == "TOPIC" and len(words) > 2:
                                    assert iteration == 1 and "mode" in seen, line
                                    topic = line.split(" :", 1)[1]
                                    assert topic == "  \x0304red topic <b>literal</b>  ", repr(topic)
                                    seen.add("topic")
                                    send(f":{nickname}!test@localhost TOPIC #port :{topic}")
                                elif command == "PRIVMSG":
                                    if recovery in {"drop", "cancel"}:
                                        assert iteration == 1 and line == "PRIVMSG #port :drop connection now", line
                                        seen.add("drop")
                                        connection.shutdown(socket.SHUT_RDWR)
                                        break
                                    assert line == "PRIVMSG #port :message through Qt adapter", line
                                    seen.add("message")
                                    send(":peer!test@localhost PRIVMSG #port :reply from real socket")
                                    send(":peer!test@localhost NICK :peer-renamed")
                                    send(":temporary!test@localhost JOIN :#port")
                                    send(":temporary!test@localhost QUIT :gone")
                                    send(":parted!test@localhost JOIN :#port")
                                    send(":parted!test@localhost PART #port :gone")
                                    send(f":local NOTICE {nickname} :users-done")
                                elif command == "PART":
                                    assert iteration == 1 and words[1] == "#port" and "message" in seen, line
                                    seen.add("part")
                                    send(f":{nickname}!test@localhost PART #port :runtime-part")
                                elif command == "QUIT":
                                    seen.add("quit")
                                if nickname and user and cap_end and not registered:
                                    registered = True
                                    send(f":local 001 {nickname} :Welcome")
                                    send(f":local 005 {nickname} CHANTYPES=# PREFIX=(ov)@+ CHANMODES=beI,k,l,imnpst :are supported")
                                    send(f":local 376 {nickname} :End of MOTD")
                                    send("PING :runtime-ping")
                        if reject_tls:
                            assert not seen and not registered, "Invalid TLS registered"
                            continue
                        required = {"join", "pong", "quit"}
                        if recovery in {"drop", "cancel"} and iteration == 1:
                            required = {"join", "pong", "drop"}
                        if tls:
                            required |= {"sasl-denied", "sasl-aborted"} if tls_mode == "sasl-denied" else {"sasl-aborted"} if bad_scram else {"sasl"}
                        if iteration == 1 and not recovery:
                            required |= {"mode-denied", "mode", "topic", "message", "part"}
                        assert required <= seen, f"connection {iteration} missing {required - seen}"
                    if recovery == "cancel":
                        listener.settimeout(2)
                        try:
                            extra, _ = listener.accept()
                        except socket.timeout:
                            pass
                        else:
                            extra.close()
                            raise AssertionError("Canceled reconnect still opened a connection")
                    finished.set()
                except Exception as error:
                    if not stopping.is_set():
                        failures.append(repr(error))

            report = folder / "report.txt"
            worker = threading.Thread(target=peer, daemon=True)
            worker.start()
            environment = {**os.environ, "QT_QPA_PLATFORM": os.environ.get("QT_IRC_PLATFORM", "offscreen"), "ZOITECHAT_LIBDIR": str(system_addons),
                           "QT_IRC_PORT": str(listener.getsockname()[1]), "QT_IRC_REPORT": str(report), "QT_IRC_TLS": tls_mode, "QT_IRC_RECOVERY": recovery}
            if bundled_addons:
                environment.pop("ZOITECHAT_LIBDIR", None)
            if scram or tls_mode in {"trusted", "mismatch", "sasl-denied", "external"}:
                environment["SSL_CERT_FILE"] = str(certificate)
                environment["SSL_CERT_DIR"] = str(system_addons)
            elif tls_mode == "reject":
                environment["SSL_CERT_FILE"] = str(folder / "no-trust-store")
                environment["SSL_CERT_DIR"] = str(system_addons)
            process = subprocess.Popen([str(binary), "--no-auto", "--cfgdir", str(config)], env=environment,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            passed = False
            try:
                deadline = time.monotonic() + 20
                while time.monotonic() < deadline:
                    assert not failures, failures
                    assert process.poll() is None, "Qt process exited early"
                    result = report.read_text() if report.exists() else "not initialized"
                    assert not result.startswith("FAIL"), result
                    if result.startswith("PASS") and finished.is_set():
                        passed = True
                        print(("PASS: automatic reconnect " + recovery if recovery else
                               "PASS: " + tls_mode + " TLS rejected without IRC credentials" if reject_tls else
                               "PASS: " + ("saved-network " + tls_mode + " TLS/SASL " + mechanism + ", " if tls else "") + "real Qt/core registration, NAMES/JOIN/NICK/PART/QUIT membership, rejected+confirmed modes, topic attributes, messages, reconnect/rejoin and disconnect"))
                        break
                    time.sleep(0.02)
                else:
                    raise AssertionError("IRC runtime timeout: " + result)
            finally:
                stopping.set()
                process.terminate()
                try:
                    output, _ = process.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    output, _ = process.communicate(timeout=5)
                if not passed:
                    print("IRC transcript:\n" + "\n".join(transcript))
                    print("Probe report: " + (report.read_text() if report.exists() else "not initialized"))
                    print(output)
                worker.join(timeout=1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("build", nargs="?", type=Path, help="Directory containing the executable's config.h")
    parser.add_argument("--bundled-addons", action="store_true", help="Use the executable's packaged addon directory")
    args = parser.parse_args()
    binary = args.binary.resolve()
    build = args.build.resolve() if args.build else binary.parents[2]
    run(binary, build=build, bundled_addons=args.bundled_addons)
    for recovery in ("drop", "refused", "cancel"):
        run(binary, recovery=recovery, build=build, bundled_addons=args.bundled_addons)
    config = (build / "config.h").read_text()
    if any(line.startswith("#define USE_OPENSSL") for line in config.splitlines()):
        for mode in ("trusted", "allow", "reject", "mismatch", "sasl-denied", "external", "scram-1", "scram-256", "scram-512", "scram-bad-signature", "scram-bad-prefix", "scram-bad-separator"):
            run(binary, mode, build=build, bundled_addons=args.bundled_addons)
    else:
        print("SKIP: TLS/SASL scenarios require a TLS-enabled build")
