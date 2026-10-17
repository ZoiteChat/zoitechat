"""Dependency parsing, relocation guards and release audit tests without a Mac SDK."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import bundle


class BundleTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform == "darwin", "requires Apple's Mach-O tools")
    def test_signed_binary_relocation_and_resigning(self):
        with tempfile.TemporaryDirectory(prefix="zoitechat relocation ") as temporary:
            directory = Path(temporary)
            library_source = directory / "answer.c"
            library_source.write_text("int answer(void) { return 42; }\n")
            library = directory / "libanswer.dylib"
            main_source = directory / "main.c"
            main_source.write_text(
                "#include <stdio.h>\n"
                "extern int answer(void);\n"
                "int main(void) { printf(\"%d\\n\", answer()); return 0; }\n"
            )
            executable = directory / "original"
            bundle.run("xcrun", "clang", "-dynamiclib", "-Wl,-headerpad_max_install_names",
                       "-Wl,-install_name," + str(library), library_source, "-o", library)
            bundle.run("xcrun", "clang", "-Wl,-headerpad_max_install_names",
                       "-Wl,-rpath," + str(directory), main_source, library, "-o", executable)
            for binary in (library, executable):
                bundle.run("codesign", "--force", "--sign", "-", binary)
            app = directory / "Test.app"
            target = app / "Contents/MacOS/zoitechat"
            target.parent.mkdir(parents=True)
            shutil.copy2(executable, target)
            relocator = bundle.Relocator(app, executable)
            relocator.relocate(executable, target)
            # Remove the original dependency so execution proves relocation.
            library.unlink()
            for binary in (app / "Contents/Frameworks/libanswer.dylib", target):
                bundle.run("codesign", "--force", "--sign", "-", binary)
                bundle.run("codesign", "--verify", "--strict", binary)
            self.assertEqual(bundle.run(target), "42")
            self.assertEqual(len(bundle.audit(app)["mach_o"]), 2)

    def test_otool_preserves_paths_with_spaces(self):
        output = "app:\n\t/opt/Build Dependencies/lib/liba.dylib (compatibility version 1.0.0, current version 1.0.0)\n\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1.0.0)"
        with patch.object(bundle, "run", return_value=output):
            self.assertEqual(bundle.dependencies(Path("app")), ["/opt/Build Dependencies/lib/liba.dylib", "/usr/lib/libSystem.B.dylib"])

    def test_lc_rpaths_preserve_spaces(self):
        with patch.object(bundle, "run", return_value="cmd LC_RPATH\n cmdsize 48\n path @loader_path/../Build Dependencies (offset 12)\n"):
            self.assertEqual(bundle.rpaths(Path("app")), ["@loader_path/../Build Dependencies"])

    def test_inherited_rpath_resolution(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            library = directory / "with spaces/libtest.dylib"
            library.parent.mkdir()
            library.write_bytes(b"library")
            self.assertEqual(bundle.resolve_dependency("@rpath/libtest.dylib", directory / "libparent.dylib", directory / "app", [library.parent]), library.resolve())

    def test_unresolved_rpath_fails(self):
        with self.assertRaisesRegex(RuntimeError, "Cannot resolve"):
            bundle.resolve_dependency("@rpath/missing.dylib", Path("/tmp/parent"), Path("/tmp/app"), [])

    def test_conflicting_basename_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            sources = [directory / name / "libtest.dylib" for name in ("one", "two")]
            for source in sources:
                source.parent.mkdir()
                source.write_bytes(b"library")
            relocator = bundle.Relocator(directory / "App.app", directory / "executable")
            relocator.copy_dependency(sources[0])
            with self.assertRaisesRegex(RuntimeError, "Conflicting library basename"):
                relocator.copy_dependency(sources[1])

    def test_audit_rejects_external_symlink(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / "App.app"
            app.mkdir()
            (app / "outside").symlink_to("/usr/lib")
            with patch.object(bundle, "run", return_value="arm64"), self.assertRaisesRegex(RuntimeError, "External symlink"):
                bundle.audit(app)

    def test_audit_raises_minimum_for_dependencies(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / "App.app"
            binary = app / "Contents/MacOS/zoitechat"
            library = app / "Contents/Frameworks/libtest.dylib"
            for path in (binary, library):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(bytes.fromhex("cffaedfe") + b"payload")
            def fake_run(*args):
                if args[0] == "lipo":
                    return "arm64"
                if args[1] == "-D":
                    return str(args[2]) + ("\n@rpath/libtest.dylib" if args[2] == library else "")
                if args[1] == "-L":
                    name = "@loader_path/../Frameworks/libtest.dylib" if args[2] == binary else "@rpath/libtest.dylib"
                    return f"{args[2]}:\n\t{name} (compatibility version 1.0.0, current version 1.0.0)"
                return "cmd LC_BUILD_VERSION\n cmdsize 32\n platform 1\n minos " + ("15.0.1" if args[2] == library else "11.0")
            with patch.object(bundle, "run", side_effect=fake_run):
                report = bundle.audit(app)
            self.assertEqual(report["minimum_macos"], "15.0.1")
            self.assertEqual(report["architectures"], ["arm64"])
            self.assertEqual(len(report["mach_o"]), 2)

    def test_audit_rejects_wrong_architecture(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / "App.app"
            binary = app / "Contents/MacOS/zoitechat"
            library = app / "Contents/Frameworks/libtest.dylib"
            for path in (binary, library):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(bytes.fromhex("cffaedfe"))
            with patch.object(bundle, "run", side_effect=["arm64", "x86_64"]), self.assertRaisesRegex(RuntimeError, "Architecture mismatch"):
                bundle.audit(app)


if __name__ == "__main__":
    unittest.main()
