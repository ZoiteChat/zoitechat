#!/usr/bin/env python3
"""Developer ID sign, notarize and staple a built app using CI environment secrets."""
from __future__ import annotations

import argparse
import base64
import binascii
from contextlib import contextmanager
from dataclasses import dataclass
import json
import os
from pathlib import Path
import platform
import re
import secrets
import shlex
import subprocess
import sys
import tempfile
import urllib.request
import uuid

from bundle import sign

REQUIRED = (
    "MACOS_CERTIFICATE_P12_BASE64", "MACOS_CERTIFICATE_PASSWORD",
    "MACOS_SIGNING_IDENTITY", "APPLE_API_KEY_P8", "APPLE_API_KEY_ID", "APPLE_API_ISSUER_ID",
)


@dataclass(repr=False)
class Credentials:
    certificate: bytes
    password: str
    identity: str
    api_key: str
    key_id: str
    issuer: str

    @classmethod
    def from_environment(cls) -> Credentials:
        missing = [name for name in REQUIRED if not os.environ.get(name, "").strip()]
        if missing:
            raise RuntimeError("Missing release settings: " + ", ".join(missing))
        try:
            certificate = base64.b64decode("".join(os.environ[REQUIRED[0]].split()), validate=True)
        except (binascii.Error, ValueError):
            raise RuntimeError("MACOS_CERTIFICATE_P12_BASE64 must contain Base64-encoded .p12 bytes") from None
        if not certificate:
            raise RuntimeError("Signing certificate is empty")
        identity = os.environ["MACOS_SIGNING_IDENTITY"].strip()
        if not (identity.startswith("Developer ID Application:") or re.fullmatch(r"[a-fA-F0-9]{40}", identity)):
            raise RuntimeError("MACOS_SIGNING_IDENTITY must identify a Developer ID Application certificate")
        api_key = os.environ["APPLE_API_KEY_P8"].strip()
        if not (api_key.startswith("-----BEGIN PRIVATE KEY-----") and api_key.endswith("-----END PRIVATE KEY-----")):
            raise RuntimeError("APPLE_API_KEY_P8 must contain the complete, plain-text .p8 private key")
        key_id = os.environ["APPLE_API_KEY_ID"].strip()
        if not re.fullmatch(r"[A-Za-z0-9]{6,32}", key_id):
            raise RuntimeError("APPLE_API_KEY_ID must be an App Store Connect key ID")
        try:
            issuer = str(uuid.UUID(os.environ["APPLE_API_ISSUER_ID"].strip()))
        except ValueError:
            raise RuntimeError("APPLE_API_ISSUER_ID must be the Team API key issuer UUID") from None
        return cls(certificate, os.environ["MACOS_CERTIFICATE_PASSWORD"], identity, api_key, key_id, issuer)


def execute(args: list[str | Path], operation: str, *, check: bool = True) -> subprocess.CompletedProcess[str]:
    # Do not print argv or CalledProcessError: security commands contain passwords.
    try:
        result = subprocess.run([str(arg) for arg in args], capture_output=True, text=True, check=False)
    except OSError:
        raise RuntimeError(f"{operation}: could not start the required tool") from None
    if check and result.returncode != 0:
        raise RuntimeError(f"{operation} failed (exit {result.returncode})")
    return result


def write_private(path: Path, data: bytes) -> None:
    with path.open("xb") as stream:
        path.chmod(0o600)
        stream.write(data)


def resolve_identity(output: str, requested: str) -> str:
    for fingerprint, name in re.findall(r'\b([A-Fa-f0-9]{40})\s+"([^"\n]+)"', output):
        if name.startswith("Developer ID Application:") and (requested == name or requested.upper() == fingerprint.upper()):
            return fingerprint
    raise RuntimeError("Imported .p12 has no valid Developer ID Application identity matching MACOS_SIGNING_IDENTITY; check its private key, password, expiry and certificate chain")


@contextmanager
def signing_keychain(credentials: Credentials):
    original = shlex.split(execute(["security", "list-keychains", "-d", "user"], "Read keychain search list").stdout)
    with tempfile.TemporaryDirectory(prefix="zoitechat-signing-", dir=os.environ.get("RUNNER_TEMP")) as temporary:
        directory = Path(temporary)
        keychain = directory / "signing.keychain-db"
        keychain_password = secrets.token_urlsafe(32)
        certificate = directory / "signing.p12"
        api_key = directory / "notary.p8"
        write_private(certificate, credentials.certificate)
        write_private(api_key, (credentials.api_key + "\n").encode())
        try:
            execute(["security", "create-keychain", "-p", keychain_password, keychain], "Create signing keychain")
            execute(["security", "set-keychain-settings", "-lut", "21600", keychain], "Configure signing keychain")
            execute(["security", "unlock-keychain", "-p", keychain_password, keychain], "Unlock signing keychain")
            # Include both public Developer ID intermediates so signing does not
            # depend on the runner having cached the leaf certificate's issuer.
            for name in ("DeveloperIDCA.cer", "DeveloperIDG2CA.cer"):
                intermediate = directory / name
                try:
                    with urllib.request.urlopen("https://www.apple.com/certificateauthority/" + name, timeout=60) as response:
                        write_private(intermediate, response.read())
                except OSError:
                    raise RuntimeError("Download of Apple's public Developer ID intermediate failed") from None
                execute(["security", "import", intermediate, "-k", keychain], "Import Developer ID intermediate")
            execute(["security", "import", certificate, "-P", credentials.password,
                     "-k", keychain, "-T", "/usr/bin/codesign", "-T", "/usr/bin/security"], "Import signing .p12")
            execute(["security", "set-key-partition-list", "-S", "apple-tool:,apple:",
                     "-k", keychain_password, keychain], "Allow non-interactive code signing")
            execute(["security", "list-keychains", "-d", "user", "-s", keychain, *original], "Add temporary signing keychain")
            identities = execute(["security", "find-identity", "-v", "-p", "codesigning", keychain], "Find signing identity").stdout
            yield keychain, api_key, resolve_identity(identities, credentials.identity)
        finally:
            # Hosted runners are discarded too, but clean up on every ordinary
            # success/failure path and restore the original search list.
            execute(["security", "list-keychains", "-d", "user", "-s", *original], "Restore keychain search list", check=False)
            if keychain.exists():
                result = execute(["security", "delete-keychain", keychain], "Delete signing keychain", check=False)
                if result.returncode:
                    print("Warning: keychain deletion failed; temporary files will still be removed.", file=sys.stderr)


def decode_response(result: subprocess.CompletedProcess[str], operation: str) -> dict:
    try:
        response = json.loads(result.stdout)
    except (json.JSONDecodeError, TypeError):
        raise RuntimeError(f"{operation} did not return valid JSON (exit {result.returncode})") from None
    if not isinstance(response, dict):
        raise RuntimeError(f"{operation} returned an unexpected response")
    return response


def save_json(path: Path, response: dict) -> None:
    path.write_text(json.dumps(response, indent=2) + "\n")


def notarize(app: Path, output: Path, diagnostics: Path, credentials: Credentials, api_key: Path, timeout: str) -> None:
    diagnostics.mkdir(parents=True, exist_ok=True)
    authentication = ["--key", str(api_key), "--key-id", credentials.key_id, "--issuer", credentials.issuer]
    with tempfile.TemporaryDirectory(prefix="zoitechat-notary-", dir=os.environ.get("RUNNER_TEMP")) as temporary:
        submission_zip = Path(temporary) / "ZoiteChat-submission.zip"
        execute(["ditto", "-c", "-k", "--keepParent", app, submission_zip], "Package notarization submission")
        result = execute(["xcrun", "notarytool", "submit", submission_zip, *authentication,
                          "--output-format", "json"], "Submit notarization", check=False)
        submission = decode_response(result, "Notarization submission")
        save_json(diagnostics / "submission.json", submission)
        if result.returncode or not submission.get("id"):
            raise RuntimeError("Apple rejected the notarization submission; see submission.json")
        try:
            submission_id = str(uuid.UUID(submission["id"]))
        except (ValueError, TypeError, AttributeError):
            raise RuntimeError("Apple returned an invalid notarization submission ID") from None
        print(f"Notarization submitted: {submission_id}", flush=True)
        waited = execute(["xcrun", "notarytool", "wait", submission_id, *authentication,
                          "--timeout", timeout, "--output-format", "json"], "Wait for notarization", check=False)
        try:
            status = decode_response(waited, "Notarization wait")
        except RuntimeError:
            status = {"id": submission_id, "status": "Unknown", "exit_code": waited.returncode}
        save_json(diagnostics / "status.json", status)
        # Apple can provide a diagnostic log after processing. Fetch it for both
        # successful and rejected submissions; never include credential files.
        execute(["xcrun", "notarytool", "log", submission_id, *authentication,
                 diagnostics / "notary-log.json"], "Fetch notarization log", check=False)
        if waited.returncode or status.get("status") != "Accepted":
            raise RuntimeError("Notarization was not accepted or exceeded the wait limit; see the notarization diagnostic artifact. No release ZIP will be published")
        execute(["xcrun", "stapler", "staple", app], "Staple notarization ticket")
        execute(["xcrun", "stapler", "validate", app], "Validate notarization ticket")
        execute(["codesign", "--verify", "--deep", "--strict", "--verbose=2", app], "Verify stapled app signature")
        execute(["spctl", "--assess", "--type", "execute", "--verbose=2", app], "Assess Gatekeeper acceptance")
        output.parent.mkdir(parents=True, exist_ok=True)
        # Repack only after stapling. The submission ZIP is not the distributable ZIP.
        execute(["ditto", "-c", "-k", "--keepParent", app, output], "Package notarized release")
    print(f"Signed and notarized release: {output}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-config", action="store_true", help="Validate release settings without accessing Apple or a keychain")
    parser.add_argument("--app", type=Path, default=Path("dist/ZoiteChat.app"))
    parser.add_argument("--output", type=Path, default=Path("dist/ZoiteChat-signed-notarized.zip"))
    parser.add_argument("--diagnostics", type=Path, default=Path("dist/notarization"))
    parser.add_argument("--notary-timeout", default="60m")
    args = parser.parse_args()
    credentials = Credentials.from_environment()
    if args.check_config:
        print("Release signing and notarization settings are present and correctly formatted.")
        return
    if platform.system() != "Darwin":
        parser.error("Signing/notarization requires the macOS runner and Xcode command-line tools")
    if not args.app.is_dir() or args.output.exists():
        parser.error("Provide a built .app and an output ZIP path that does not already exist")
    if not re.fullmatch(r"[1-9]\d*[smh]", args.notary_timeout):
        parser.error("Use a positive notarization timeout, such as 60m")
    with signing_keychain(credentials) as (keychain, api_key, identity):
        sign(args.app.resolve(), identity, Path(__file__).with_name("direct.entitlements"), keychain=keychain)
        notarize(args.app.resolve(), args.output.resolve(), args.diagnostics.resolve(), credentials, api_key, args.notary_timeout)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError):
        # RuntimeError messages are authored above and do not contain secret argv.
        error = sys.exc_info()[1]
        if isinstance(error, RuntimeError):
            sys.exit(f"macOS release failed: {error}")
        sys.exit("macOS release failed: file or network operation failed")
    except subprocess.CalledProcessError:
        sys.exit("macOS release failed: code signing or bundle audit failed")
