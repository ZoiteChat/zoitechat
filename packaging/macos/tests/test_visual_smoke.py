"""Check that visual-test evidence requires a real IRC handshake and reply."""
import importlib.util
from pathlib import Path
import socket
import threading
import unittest

spec = importlib.util.spec_from_file_location(
    "visual_smoke", Path(__file__).resolve().parents[1] / "visual-smoke.py")
visual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(visual)


class VisualSmokeTests(unittest.TestCase):
    def test_tls_clienthello_is_rejected_without_waiting_for_a_newline(self):
        with visual.IRCServer() as server:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                with socket.create_connection(server.server_address, timeout=3) as client:
                    # A TLS handshake record, with no IRC line terminator.
                    client.sendall(b"\x16\x03\x03\x00\x00")
                    self.assertEqual(client.recv(1), b"")
                self.assertIn("Received TLS", server.error)
                self.assertFalse(server.joined.is_set())
                self.assertFalse(server.pong.is_set())
            finally:
                server.shutdown()
                thread.join(timeout=3)

    def test_registration_join_and_challenge_reply(self):
        with visual.IRCServer() as server:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                with socket.create_connection(server.server_address, timeout=3) as client:
                    stream = client.makefile("rb")

                    def until(marker):
                        lines = []
                        for _ in range(30):
                            line = stream.readline().decode("utf-8").rstrip()
                            self.assertTrue(line, "server closed before expected response")
                            lines.append(line)
                            if marker in line:
                                return lines
                        self.fail("expected response missing: " + marker)

                    client.sendall(b"CAP LS 302\r\nNICK MacTester\r\nUSER ci 0 * :CI\r\n")
                    until(" CAP * LS :")
                    self.assertFalse(server.joined.is_set())
                    client.sendall(b"CAP END\r\n")
                    until(" 376 MacTester ")
                    client.sendall(b"JOIN #ci-smoke\r\n")
                    messages = until("PING :" + server.token)
                    self.assertTrue(any("PRIVMSG #ci-smoke" in line for line in messages))
                    self.assertTrue(server.joined.wait(timeout=1))
                    self.assertFalse(server.pong.is_set())
                    client.sendall(b"PONG :wrong-token\r\nPING :barrier\r\n")
                    until("PONG :barrier")
                    self.assertFalse(server.pong.is_set())
                    client.sendall(("PONG :" + server.token + "\r\n").encode())
                    self.assertTrue(server.pong.wait(timeout=1))
                    client.sendall(b"QUIT\r\n")
                    stream.close()
            finally:
                server.shutdown()
                thread.join(timeout=3)

    def test_early_disconnect_is_not_success(self):
        with visual.IRCServer() as server:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                with socket.create_connection(server.server_address, timeout=3) as client:
                    client.sendall(b"QUIT\r\n")
                self.assertFalse(server.joined.is_set())
                self.assertFalse(server.pong.is_set())
            finally:
                server.shutdown()
                thread.join(timeout=3)


if __name__ == "__main__":
    unittest.main()
