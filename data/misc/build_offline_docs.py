#!/usr/bin/env python3
"""Build ZoiteChat's offline HTML documentation for bundling with releases.

Clones the ZoiteChat/documentation repository, preferring a tag that
matches this source tree's version so the bundled manual matches the
application, and builds it with Sphinx into --output. Used by the
meson build (offline-docs option) and by the Windows CI workflow.

Sphinx is installed into a throwaway virtualenv so the invoking Python
(which the Windows build also bundles pieces of) is left untouched.

Unless --strict is given, any failure prints a warning (GitHub Actions
annotation syntax) and exits 0: builds then simply fall back to opening
the online manual instead of failing outright.
"""

import argparse
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

DEFAULT_REPO = "https://github.com/ZoiteChat/documentation.git"


def project_version() -> str:
    meson_build = pathlib.Path(__file__).resolve().parents[2] / "meson.build"
    match = re.search(r"version\s*:\s*'([^']+)'", meson_build.read_text(encoding="utf-8"))
    if not match:
        raise RuntimeError(f"could not parse project version from {meson_build}")
    return match.group(1)


def run(argv, **kwargs) -> None:
    print("+", " ".join(str(arg) for arg in argv), flush=True)
    subprocess.run(argv, check=True, **kwargs)


def clone_docs(repo: str, version: str, target: pathlib.Path) -> str:
    # A missing tag falls through to the default branch so the docs may be
    # newer than the app, which beats shipping none at all.
    for ref in (f"v{version}", version, None):
        cmd = ["git", "clone", "--depth", "1"]
        if ref:
            cmd += ["--branch", ref]
        cmd += [repo, str(target)]
        try:
            run(cmd)
            return ref or "default branch"
        except subprocess.CalledProcessError:
            shutil.rmtree(target, ignore_errors=True)
    raise RuntimeError(f"could not clone {repo}")


def find_source_dir(checkout: pathlib.Path) -> pathlib.Path:
    for candidate in (checkout, checkout / "docs", checkout / "doc"):
        if (candidate / "conf.py").is_file():
            return candidate
    raise RuntimeError(f"no Sphinx conf.py found in {checkout}")


def venv_python(venv_dir: pathlib.Path) -> pathlib.Path:
    if sys.platform == "win32":
        return venv_dir / "Scripts" / "python.exe"
    return venv_dir / "bin" / "python"


def build(output: pathlib.Path, repo: str, version: str) -> None:
    with tempfile.TemporaryDirectory(prefix="zoitechat-docs-") as tmp:
        tmp_dir = pathlib.Path(tmp)
        checkout = tmp_dir / "documentation"
        html_dir = tmp_dir / "html"
        venv_dir = tmp_dir / "venv"

        ref = clone_docs(repo, version, checkout)
        print(f"building documentation from {ref}", flush=True)
        source_dir = find_source_dir(checkout)

        run([sys.executable, "-m", "venv", str(venv_dir)])
        python = venv_python(venv_dir)
        requirements = source_dir / "requirements.txt"
        if not requirements.is_file():
            requirements = checkout / "requirements.txt"
        if requirements.is_file():
            run([python, "-m", "pip", "install", "--quiet", "-r", str(requirements)])
            run([python, "-m", "pip", "install", "--quiet", "sphinx"])
        else:
            run([python, "-m", "pip", "install", "--quiet", "sphinx", "sphinx_rtd_theme"])

        run([python, "-m", "sphinx", "-b", "html", str(source_dir), str(html_dir)])
        if not (html_dir / "index.html").is_file():
            raise RuntimeError("Sphinx build produced no index.html")

        if output.exists():
            shutil.rmtree(output)
        shutil.copytree(html_dir, output,
                        ignore=shutil.ignore_patterns(".doctrees", ".buildinfo"))

    file_count = sum(1 for path in output.rglob("*") if path.is_file())
    print(f"offline documentation staged at {output} ({file_count} files)", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=pathlib.Path,
                        help="directory to stage the built HTML tree into")
    parser.add_argument("--repo", default=DEFAULT_REPO,
                        help="documentation git repository to build from")
    parser.add_argument("--version", default=None,
                        help="app version used to pick a matching docs tag "
                             "(default: parsed from meson.build)")
    parser.add_argument("--strict", action="store_true",
                        help="fail on error instead of emitting a warning")
    parser.add_argument("--stamp", type=pathlib.Path, default=None,
                        help="file to touch on completion (meson target output)")
    args = parser.parse_args()

    try:
        build(args.output, args.repo, args.version or project_version())
    except Exception as error:  # noqa: BLE001 - degrade to website-only help
        if args.strict:
            raise
        print(f"::warning::offline documentation not bundled: {error}", flush=True)

    if args.stamp:
        args.stamp.write_text("ok\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
