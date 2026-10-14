"""Release failure gates, private-key handling and notarization result checks."""
from contextlib import redirect_stdout
import base64
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import release

FINGERPRINT = "A" * 40
IDENTITY = "Developer ID Application: Test Only (NOTAREALID)"
SUBMISSION = "a8c9438a-b972-455e-aedc-278070349394"
ENVIRONMENT = {
    "MACOS_CERTIFICATE_P12_BASE64": base64.b64encode(b"not-a-real-certificate").decode(),
    "MACOS_CERTIFICATE_PASSWORD": "unit-test-password",
    "MACOS_SIGNING_IDENTITY": IDENTITY,
    "APPLE_API_KEY_P8": "-----BEGIN PRIVATE KEY-----\nunit-test-only\n-----END PRIVATE KEY-----",
    "APPLE_API_KEY_ID": "TESTKEY123",
    "APPLE_API_ISSUER_ID": SUBMISSION,
}


class ReleaseTests(unittest.TestCase):
    def credentials(self):
        with patch.dict(os.environ, ENVIRONMENT, clear=True):
            return release.Credentials.from_environment()

    def test_config_reports_names_and_never_values(self):
        with patch.dict(os.environ, {}, clear=True), self.assertRaisesRegex(RuntimeError, "MACOS_CERTIFICATE_P12_BASE64"):
            release.Credentials.from_environment()

    def test_config_accepts_multiline_certificate_base64(self):
        env = ENVIRONMENT.copy()
        text = env["MACOS_CERTIFICATE_P12_BASE64"]
        env["MACOS_CERTIFICATE_P12_BASE64"] = text[:10] + "\n" + text[10:]
        with patch.dict(os.environ, env, clear=True):
            self.assertEqual(release.Credentials.from_environment().certificate, b"not-a-real-certificate")

    def test_config_rejects_development_certificate(self):
        env = {**ENVIRONMENT, "MACOS_SIGNING_IDENTITY": "Apple Development: Test"}
        with patch.dict(os.environ, env, clear=True), self.assertRaisesRegex(RuntimeError, "Developer ID Application"):
            release.Credentials.from_environment()

    def test_config_rejects_base64_instead_of_plain_p8(self):
        env = {**ENVIRONMENT, "APPLE_API_KEY_P8": "not-the-pem-contents"}
        with patch.dict(os.environ, env, clear=True), self.assertRaisesRegex(RuntimeError, "complete, plain-text"):
            release.Credentials.from_environment()

    def test_execute_does_not_expose_secret_argv_or_stderr(self):
        result = subprocess.CompletedProcess([], 1, stdout="", stderr="secret-password")
        with patch.object(release.subprocess, "run", return_value=result):
            with self.assertRaises(RuntimeError) as error:
                release.execute(["security", "import", "-P", "secret-password"], "Import certificate")
        self.assertNotIn("secret-password", str(error.exception))

    def test_matching_identity_requires_developer_id_and_private_key(self):
        output = f'  1) {FINGERPRINT} "{IDENTITY}"\n'
        self.assertEqual(release.resolve_identity(output, IDENTITY), FINGERPRINT)
        with self.assertRaisesRegex(RuntimeError, "no valid Developer ID"):
            release.resolve_identity("0 valid identities found", IDENTITY)

    def test_private_files_have_restricted_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "private.p8"
            release.write_private(path, b"test-only")
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_keychain_and_private_files_are_cleaned_up_after_signing_failure(self):
        calls = []
        def fake_execute(args, operation, **kwargs):
            calls.append([str(value) for value in args])
            output = ''
            if args[:3] == ["security", "list-keychains", "-d"] and len(args) == 4:
                output = '"/tmp/original.keychain-db"\n'
            if args[:2] == ["security", "create-keychain"]:
                Path(args[-1]).touch()
            if args[:2] == ["security", "find-identity"]:
                output = f'  1) {FINGERPRINT} "{IDENTITY}"\n'
            return subprocess.CompletedProcess(args, 0, output, "")
        temporary_path = None
        with patch.object(release, "execute", side_effect=fake_execute), patch.object(release.urllib.request, "urlopen", side_effect=lambda *args, **kwargs: io.BytesIO(b"public-test-ca")):
            with self.assertRaisesRegex(RuntimeError, "test signing failed"):
                with release.signing_keychain(self.credentials()) as (keychain, api_key, identity):
                    temporary_path = keychain.parent
                    self.assertEqual(identity, FINGERPRINT)
                    self.assertTrue(api_key.is_file())
                    raise RuntimeError("test signing failed")
        self.assertFalse(temporary_path.exists())
        self.assertIn(["security", "list-keychains", "-d", "user", "-s", "/tmp/original.keychain-db"], calls)
        self.assertTrue(any(call[:2] == ["security", "delete-keychain"] for call in calls))

    def run_notary(self, status, returncode=0):
        calls = []
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        app = directory / "ZoiteChat.app"
        app.mkdir()
        output = directory / "release.zip"
        diagnostics = directory / "diagnostics"
        def fake_execute(args, operation, **kwargs):
            calls.append([str(value) for value in args])
            text = ""
            code = 0
            if args[:3] == ["xcrun", "notarytool", "submit"]:
                text = json.dumps({"id": SUBMISSION})
            elif args[:3] == ["xcrun", "notarytool", "wait"]:
                text = json.dumps({"id": SUBMISSION, "status": status})
                code = returncode
            elif args[:3] == ["xcrun", "notarytool", "log"]:
                Path(args[-1]).write_text('{"issues": []}')
            elif args[0] == "ditto":
                Path(args[-1]).write_bytes(b"test-zip")
            return subprocess.CompletedProcess(args, code, text, "")
        with patch.object(release, "execute", side_effect=fake_execute), redirect_stdout(io.StringIO()):
            if status == "Accepted" and not returncode:
                release.notarize(app, output, diagnostics, self.credentials(), directory / "test.p8", "60m")
            else:
                with self.assertRaisesRegex(RuntimeError, "not accepted"):
                    release.notarize(app, output, diagnostics, self.credentials(), directory / "test.p8", "60m")
        return calls, output, diagnostics

    def test_accepted_notarization_staples_and_checks_before_final_zip(self):
        calls, output, diagnostics = self.run_notary("Accepted")
        self.assertTrue(output.is_file())
        staple = next(i for i, call in enumerate(calls) if call[:3] == ["xcrun", "stapler", "staple"])
        assess = next(i for i, call in enumerate(calls) if call[0] == "spctl")
        final_zip = max(i for i, call in enumerate(calls) if call[0] == "ditto")
        self.assertLess(staple, assess)
        self.assertLess(assess, final_zip)
        self.assertEqual(json.loads((diagnostics / "status.json").read_text())["status"], "Accepted")

    def test_rejected_notarization_never_staples_or_publishes(self):
        calls, output, diagnostics = self.run_notary("Invalid")
        self.assertFalse(output.exists())
        self.assertTrue((diagnostics / "notary-log.json").is_file())
        self.assertFalse(any(call[:2] == ["xcrun", "stapler"] for call in calls))

    def test_timeout_retains_submission_id_and_never_publishes(self):
        calls, output, diagnostics = self.run_notary("In Progress", 1)
        self.assertFalse(output.exists())
        self.assertEqual(json.loads((diagnostics / "submission.json").read_text())["id"], SUBMISSION)


if __name__ == "__main__":
    unittest.main()
