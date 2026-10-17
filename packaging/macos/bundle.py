#!/usr/bin/env python3
"""Build and audit a relocatable GTK3 application. No third-party Python packages."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
MACH_MAGIC = {bytes.fromhex(h) for h in ("feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}
SYSTEM_ROOTS = ("/usr/lib/", "/System/Library/")


def run(*args: str | Path, env: dict[str, str] | None = None) -> str:
    return subprocess.check_output([str(arg) for arg in args], env=env, text=True).strip()


def is_macho(path: Path) -> bool:
    if not path.is_file() or path.is_symlink():
        return False
    with path.open("rb") as stream:
        return stream.read(4) in MACH_MAGIC


def dependencies(path: Path) -> list[str]:
    # Paths may contain spaces; splitting on whitespace would corrupt them.
    return re.findall(r"^\s+(.+?) \(compatibility version", run("otool", "-L", path), re.M)


def rpaths(path: Path) -> list[str]:
    return re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.+?) \(offset", run("otool", "-l", path))


def dylib_id(path: Path) -> str | None:
    lines = run("otool", "-D", path).splitlines()
    return lines[1].strip() if len(lines) > 1 else None


def system_library(name: str) -> bool:
    return name.startswith(SYSTEM_ROOTS)


def expand_path(name: str, loader: Path, executable: Path) -> Path:
    return Path(name.replace("@loader_path", str(loader.parent)).replace("@executable_path", str(executable.parent)))


def resolve_dependency(name: str, loader: Path, executable: Path, search: list[Path]) -> Path:
    if name.startswith("@rpath/"):
        suffix = name[len("@rpath/"):]
        candidates = [directory / suffix for directory in search]
    else:
        candidates = [expand_path(name, loader, executable)]
    for candidate in candidates:
        if candidate.is_absolute() and candidate.is_file():
            return candidate.resolve()
    raise RuntimeError(f"Cannot resolve {name!r} from {loader}; searched {candidates}")


class Relocator:
    def __init__(self, app: Path, executable: Path):
        self.app = app
        self.executable = executable
        self.frameworks = app / "Contents/Frameworks"
        self.frameworks.mkdir(parents=True)
        self.sources: dict[Path, Path] = {}
        self.names: dict[str, Path] = {}
        self.visited: set[Path] = set()

    def copy_dependency(self, source: Path) -> Path:
        source = source.resolve()
        if ".framework/" in str(source):
            raise RuntimeError(f"Third-party frameworks require separate packaging support: {source}")
        if source in self.sources:
            return self.sources[source]
        existing = self.names.get(source.name)
        if existing and existing != source:
            raise RuntimeError(f"Conflicting library basename: {existing} and {source}")
        target = self.frameworks / source.name
        shutil.copy2(source, target)
        target.chmod(target.stat().st_mode | 0o200)
        self.sources[source] = target
        self.names[source.name] = source
        return target

    def relocate(self, source: Path, target: Path, inherited: list[Path] | None = None) -> None:
        source = source.resolve()
        if target in self.visited:
            return
        self.visited.add(target)
        local = [expand_path(name, source, self.executable) for name in rpaths(source)]
        search = local + (inherited or [])
        identity = dylib_id(source)
        # Keep the existing signature allocation while editing: stripping it
        # first can leave __LINKEDIT padding that install_name_tool rejects.
        # The final inside-out codesign --force pass replaces stale signatures.
        edits: list[str | Path] = []
        for name in dependencies(source):
            if name == identity or system_library(name):
                continue
            dependency = resolve_dependency(name, source, self.executable, search)
            destination = self.copy_dependency(dependency)
            relative = os.path.relpath(destination, target.parent)
            edits += ["-change", name, "@loader_path/" + relative]
            self.relocate(dependency, destination, search)
        if identity:
            edits += ["-id", "@rpath/" + target.name]
        for name in sorted(set(rpaths(target))):
            edits += ["-delete_rpath", name]
        if edits:
            run("install_name_tool", *edits, target)


def copy_tree(source: Path, target: Path) -> None:
    if source.is_dir():
        # Dereference symlinks: never leave links into the builder's Cellar.
        shutil.copytree(source, target, dirs_exist_ok=True, symlinks=False)


def audit(app: Path) -> dict[str, object]:
    binary = app / "Contents/MacOS/zoitechat"
    architectures = set(run("lipo", "-archs", binary).split())
    minimum = (11, 0, 0)
    entries = []
    for path in sorted(app.rglob("*")):
        if path.is_symlink():
            resolved = path.resolve()
            if not resolved.is_relative_to(app.resolve()):
                raise RuntimeError(f"External symlink: {path} -> {resolved}")
        if not is_macho(path):
            continue
        if not architectures.issubset(set(run("lipo", "-archs", path).split())):
            raise RuntimeError(f"Architecture mismatch: {path}")
        identity = dylib_id(path)
        for name in dependencies(path):
            if name == identity or system_library(name):
                continue
            if not name.startswith("@loader_path/"):
                raise RuntimeError(f"Non-relocatable dependency: {path}: {name}")
            target = expand_path(name, path, binary).resolve()
            if not target.is_relative_to(app.resolve()) or not target.is_file():
                raise RuntimeError(f"Missing or external dependency: {path}: {name}")
        if rpaths(path):
            raise RuntimeError(f"Unexpected runtime search paths: {path}")
        load_commands = run("otool", "-l", path)
        versions = re.findall(r"\bminos (\d+(?:\.\d+){0,2})", load_commands)
        versions += re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+(?:\.\d+){0,2})", load_commands)
        if not versions:
            raise RuntimeError(f"Missing deployment version: {path}")
        for version in versions:
            parts = tuple(int(value) for value in version.split("."))
            minimum = max(minimum, parts + (0,) * (3 - len(parts)))
        entries.append({"path": str(path.relative_to(app))})
    return {"architectures": sorted(architectures), "minimum_macos": ".".join(map(str, minimum if minimum[2] else minimum[:2])), "mach_o": entries}


def package(stage: Path, app: Path, build_number: str) -> None:
    if not re.fullmatch(r"[1-9]\d*(?:\.\d+){0,2}", build_number):
        raise RuntimeError("Build number must be a positive Apple CFBundleVersion (for example 1 or 12.3)")
    if app.exists():
        raise RuntimeError(f"Output already exists; choose another --output or remove it explicitly: {app}")
    app.mkdir(parents=True)
    resources = app / "Contents/Resources"
    resources.mkdir(parents=True)
    binary = app / "Contents/MacOS/zoitechat"
    binary.parent.mkdir()
    shutil.copy2(stage / "bin/zoitechat", binary)
    copy_tree(stage / "share", resources / "share")
    copy_tree(stage / "lib/zoitechat/plugins", resources / "lib/zoitechat/plugins")
    relocator = Relocator(app, stage / "bin/zoitechat")
    seeds: list[tuple[Path, Path]] = [(stage / "bin/zoitechat", binary)]
    for source in (stage / "lib/zoitechat/plugins").glob("*"):
        if is_macho(source):
            seeds.append((source, resources / "lib/zoitechat/plugins" / source.name))

    prefixes = {formula: Path(run("brew", "--prefix", formula)) for formula in (
        "gtk+3", "gdk-pixbuf", "glib", "glib-networking", "librsvg", "enchant", "adwaita-icon-theme", "hicolor-icon-theme", "gsettings-desktop-schemas", "ca-certificates", "openssl@3", "lua", "libarchive", "gtk-mac-integration")}
    # Runtime-loaded code is not visible in the main executable's otool output.
    module_groups = [
        (prefixes["gdk-pixbuf"] / "lib/gdk-pixbuf-2.0/2.10.0/loaders", "lib/gdk-pixbuf-2.0/2.10.0/loaders"),
        (prefixes["librsvg"] / "lib/gdk-pixbuf-2.0/2.10.0/loaders", "lib/gdk-pixbuf-2.0/2.10.0/loaders"),
        (prefixes["gtk+3"] / "lib/gtk-3.0/3.0.0/immodules", "lib/gtk-3.0/3.0.0/immodules"),
        (prefixes["glib-networking"] / "lib/gio/modules", "lib/gio/modules"),
        (prefixes["enchant"] / "lib/enchant-2", "lib/enchant-2"),
        (prefixes["openssl@3"] / "lib/ossl-modules", "lib/ossl-modules"),
    ]
    for source_dir, relative in module_groups:
        target_dir = resources / relative
        target_dir.mkdir(parents=True, exist_ok=True)
        for source in sorted(source_dir.glob("*")):
            if source.suffix not in (".so", ".dylib") or not source.is_file():
                continue
            target = target_dir / source.name
            if target.exists() and target.read_bytes() != source.read_bytes():
                raise RuntimeError(f"Conflicting module: {target}")
            shutil.copy2(source.resolve(), target)
            seeds.append((source.resolve(), target))
    enchant = (prefixes["enchant"] / "lib/libenchant-2.dylib").resolve()
    # Register Enchant at Resources/lib so its public prefix relocation API
    # finds Resources/lib/enchant-2 and Resources/share/enchant.
    enchant_alias = resources / "lib/libenchant-2.dylib"
    shutil.copy2(enchant, enchant_alias)
    relocator.sources[enchant] = enchant_alias
    relocator.names[enchant.name] = enchant
    seeds.append((enchant, enchant_alias))
    for formula, prefix in prefixes.items():
        for name in ("icons", "glib-2.0/schemas", "locale", "enchant", "hunspell"):
            copy_tree(prefix / "share" / name, resources / "share" / name)
    ca = prefixes["ca-certificates"] / "share/ca-certificates/cacert.pem"
    if not ca.is_file():
        raise RuntimeError(f"CA bundle not found: {ca}")
    (resources / "certs").mkdir()
    shutil.copy2(ca, resources / "certs/ca-bundle.crt")
    schemas = resources / "share/glib-2.0/schemas"
    run(prefixes["glib"] / "bin/glib-compile-schemas", schemas)
    pixbuf_modules = sorted((resources / "lib/gdk-pixbuf-2.0/2.10.0/loaders").glob("*"))
    if not any("svg" in path.name for path in pixbuf_modules):
        raise RuntimeError("SVG loader missing: install librsvg")
    if list((resources / "lib/zoitechat/plugins").glob("fishlim.*")) and not list((resources / "lib/ossl-modules").glob("legacy.*")):
        raise RuntimeError("FiSH needs the OpenSSL legacy provider in the bundle")
    if not list((resources / "lib/gio/modules").glob("*tls*")):
        raise RuntimeError("GIO TLS backend missing: install glib-networking")
    # Query original binaries before relocation, then rewrite only the cached paths.
    pixbuf_query = prefixes["gdk-pixbuf"] / "bin/gdk-pixbuf-query-loaders"
    if not pixbuf_query.exists():
        pixbuf_query = prefixes["gdk-pixbuf"] / "lib/gdk-pixbuf-2.0/gdk-pixbuf-query-loaders"
    for tool, module_dir, cache in (
        (pixbuf_query, "lib/gdk-pixbuf-2.0/2.10.0/loaders", "pixbuf-loaders.cache"),
        (prefixes["gtk+3"] / "bin/gtk-query-immodules-3.0", "lib/gtk-3.0/3.0.0/immodules", "gtk-immodules.cache"),
    ):
        originals = [(source, target) for source, target in seeds if target.parent == resources / module_dir]
        # Never call a query tool with zero arguments: it would scan global modules.
        data = run(tool, *(source for source, _ in originals)) if originals else "# No external input-method modules\n"
        for source, target in originals:
            data = data.replace(str(source), "@ZOITECHAT_RESOURCES@/" + str(target.relative_to(resources)))
        (resources / cache).write_text(data + "\n")
    # The private font map loads the bundled Noto font; ordinary GTK fonts stay local.
    (resources / "fonts.conf").write_text('''<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir>/System/Library/Fonts</dir><dir>/Library/Fonts</dir><dir>~/Library/Fonts</dir>
  <cachedir prefix="xdg">fontconfig</cachedir>
  <alias><family>sans-serif</family><prefer><family>Helvetica</family></prefer></alias>
  <alias><family>monospace</family><prefer><family>Menlo</family></prefer></alias>
</fontconfig>
''')
    (resources / "zoitechat-bundle").write_text("1\n")
    licenses = resources / "licenses"
    licenses.mkdir()
    shutil.copy2(ROOT / "COPYING", licenses / "ZoiteChat-COPYING")
    # Preserve the notices shipped with every installed Homebrew dependency.
    # brew deps traverses dependencies by default; --recursive is not an option.
    wanted = set(prefixes)
    for formula in prefixes:
        wanted.update(run("brew", "deps", "--formula", formula).splitlines())
    installed = [item for item in json.loads(run("brew", "info", "--json=v2", "--installed"))["formulae"]
                 if item["name"] in wanted]
    for formula in installed:
        prefix = Path(run("brew", "--prefix", formula["name"]))
        notices = [p for p in prefix.iterdir() if p.is_file() and re.match(r"(?i)(copying|license|copyright|notice)", p.name)]
        if notices:
            target = licenses / formula["name"].replace("/", "-")
            target.mkdir(exist_ok=True)
            for notice in notices:
                shutil.copy2(notice, target / notice.name)
    (resources / "homebrew-dependencies.json").write_text(json.dumps(installed, indent=2) + "\n")
    for source, target in seeds:
        relocator.relocate(source, target)
    report = audit(app)
    (resources / "bundle-audit.json").write_text(json.dumps(report, indent=2) + "\n")
    # Use the actual highest deployment target of the executable and all dylibs.
    # Setting an older plist version cannot make newer Homebrew bottles compatible.
    version = (ROOT / "VERSION").read_text().strip()
    release = re.match(r"\d+\.\d+\.\d+", version)
    if not release:
        raise RuntimeError(f"Unsupported release version: {version}")
    plist = {
        "CFBundleIdentifier": "org.Zoitechat", "CFBundleExecutable": "zoitechat",
        "CFBundleName": "ZoiteChat", "CFBundleDisplayName": "ZoiteChat",
        "CFBundlePackageType": "APPL", "CFBundleShortVersionString": release.group(),
        "CFBundleVersion": build_number, "CFBundleIconFile": "ZoiteChat.icns",
        "LSMinimumSystemVersion": report["minimum_macos"], "NSHighResolutionCapable": True,
        "NSPrincipalClass": "NSApplication",
        "LSApplicationCategoryType": "public.app-category.social-networking",
        "DTSDKName": "macosx" + run("xcrun", "--sdk", "macosx", "--show-sdk-version"),
        "DTSDKBuild": run("xcrun", "--sdk", "macosx", "--show-sdk-build-version"),
    }
    with (app / "Contents/Info.plist").open("wb") as stream:
        plistlib.dump(plist, stream)
    iconset = app.parent / (app.stem + ".iconset")
    if iconset.exists():
        raise RuntimeError(f"Icon staging directory already exists: {iconset}")
    iconset.mkdir()
    try:
        # Rasterize the original vector at 1024px, then generate every icon size.
        large = iconset / "source.png"
        run(prefixes["librsvg"] / "bin/rsvg-convert", "-w", "1024", "-h", "1024", "-o", large, ROOT / "data/icons/zoitechat.svg")
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                name = f"icon_{size}x{size}" + ("@2x" if scale == 2 else "") + ".png"
                run("sips", "-z", str(size * scale), str(size * scale), large, "--out", iconset / name)
        large.unlink()
        run("iconutil", "-c", "icns", iconset, "-o", resources / "ZoiteChat.icns")
    finally:
        shutil.rmtree(iconset)
    # Local builds need an ad-hoc signature after install_name_tool changes.
    sign(app, "-", None)
    print(f"Created {app}: {', '.join(report['architectures'])}, macOS {report['minimum_macos']}+")


def sign(app: Path, identity: str, entitlements: Path | None, *, keychain: Path | None = None) -> None:
    audit(app)
    binaries = [p for p in app.rglob("*") if is_macho(p) and p != app / "Contents/MacOS/zoitechat"]
    options = [] if identity == "-" else ["--timestamp", "--options", "runtime"]
    if keychain is not None:
        options += ["--keychain", str(keychain)]
    for binary in sorted(binaries, key=lambda p: len(p.parts), reverse=True):
        run("codesign", "--force", "--sign", identity, *options, binary)
    main_options = ["--entitlements", str(entitlements)] if entitlements else []
    run("codesign", "--force", "--sign", identity, *options, *main_options, app)
    run("codesign", "--verify", "--deep", "--strict", "--verbose=2", app)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    create = sub.add_parser("create")
    create.add_argument("--stage", type=Path, required=True)
    create.add_argument("--app", type=Path, required=True)
    create.add_argument("--build-number", default="1")
    verify = sub.add_parser("audit")
    verify.add_argument("app", type=Path)
    signing = sub.add_parser("sign")
    signing.add_argument("app", type=Path)
    signing.add_argument("--identity", required=True)
    signing.add_argument("--entitlements", type=Path)
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("This tool requires macOS and the Xcode command-line tools")
    if args.action == "create":
        package(args.stage.resolve(), args.app.resolve(), args.build_number)
    elif args.action == "audit":
        print(json.dumps(audit(args.app.resolve()), indent=2))
    else:
        sign(args.app.resolve(), args.identity, args.entitlements)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError) as error:
        sys.exit(f"macOS packaging failed: {error}")
