"""Regression coverage for stable release selection and build/installer alignment."""

import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location(
    "windows_runtimes", Path(__file__).resolve().parents[1] / "prepare-windows-runtimes.py"
)
runtimes = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtimes)


def release(version, arch="MSWin32-x64-multi-thread"):
    prefix = runtimes.RELEASE_ROOT + "SP_" + version.replace(".", "") + "_64bit/"
    stem = f"strawberry-perl-{version}-64bit"
    return {
        "version": version,
        "archname": arch,
        "edition": {
            "portable": {"url": prefix + stem + "-portable.zip", "sha256": "a" * 64},
            "msi": {"url": prefix + stem + ".msi", "sha256": "b" * 64},
        },
    }


class RuntimeTests(unittest.TestCase):
    def test_compares_versions_instead_of_feed_order_or_dates(self):
        releases = [release("5.40.5.1"), release("5.42.3.1"), release("5.42.0.1")]
        self.assertEqual(runtimes.select_perl(releases)["version"], "5.42.3.1")

    def test_numeric_patch_order(self):
        self.assertEqual(runtimes.select_perl([release("5.42.9.1"), release("5.42.10.1")])["version"], "5.42.10.1")

    def test_filters_development_prerelease_and_wrong_architecture(self):
        releases = [release("5.43.1.1"), release("5.44.0.1-RC1"), release("5.44.0.1", "MSWin32-x86-multi-thread"), release("5.42.3.1")]
        self.assertEqual(runtimes.select_perl(releases)["version"], "5.42.3.1")

    def test_accepts_next_stable_branch(self):
        self.assertEqual(runtimes.select_perl([release("5.42.3.1"), release("5.44.0.1")])["version"], "5.44.0.1")

    def test_requires_both_build_and_installer_packages(self):
        newer = release("5.44.0.1")
        del newer["edition"]["msi"]
        self.assertEqual(runtimes.select_perl([newer, release("5.42.3.1")])["version"], "5.42.3.1")

    def test_fails_if_no_suitable_release_exists(self):
        with self.assertRaises(ValueError):
            runtimes.select_perl([release("5.43.1.1")])

    def test_rejects_bad_hashes_untrusted_hosts_and_wrong_packages(self):
        for kind in ("portable", "msi"):
            for field, value in (("sha256", ""), ("url", "https://example.org/perl.zip"), ("url", runtimes.RELEASE_ROOT + "SP/strawberry-perl-5.40.5.1-64bit.msi")):
                with self.subTest(kind=kind, field=field, value=value):
                    invalid = copy.deepcopy(release("5.42.3.1"))
                    invalid["edition"][kind][field] = value
                    with self.assertRaises(ValueError):
                        runtimes.select_perl([invalid])

    def test_generated_dll_and_installer_follow_selected_release(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            (repo / "win32/installer").mkdir(parents=True)
            selected = release("5.44.1.1")
            perl_root = Path("C:/gtk-build/Perl & tools/perl")
            runtimes.write_metadata(repo, perl_root, selected, "3.14.8")
            props = ET.parse(repo / "win32/runtimes.generated.props")
            ns = {"m": runtimes.MSBUILD_NS}
            self.assertEqual(props.find("m:PropertyGroup/m:PerlLib", ns).text, "perl544")
            self.assertEqual(props.find("m:PropertyGroup/m:PerlPath", ns).text, str(perl_root))
            installer = (repo / "win32/installer/runtimes.generated.iss").read_text()
            self.assertIn('#define PerlRuntimeDLL "perl544.dll"', installer)
            self.assertIn(selected["edition"]["msi"]["url"], installer)
            self.assertIn("/3.14.8/python-3.14.8-amd64.exe", installer)


if __name__ == "__main__":
    unittest.main()
