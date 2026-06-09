#!/usr/bin/env python3

import html
import io
import pathlib
import shutil
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request


url = sys.argv[1]
out_dir = pathlib.Path(sys.argv[2])
source_dir = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else pathlib.Path.cwd()
docs_dir = out_dir / "offline-docs"

if docs_dir.exists():
    shutil.rmtree(docs_dir)
docs_dir.mkdir(parents=True, exist_ok=True)


def write_source_docs() -> None:
    parts = [
        '<!doctype html><html><head><meta charset="utf-8">'
        "<title>ZoiteChat Documentation</title></head><body>"
        "<h1>ZoiteChat Documentation</h1>"
    ]

    for name in ("readme.md", "troubleshooting.md", "changelog.rst"):
        path = source_dir / name
        if path.exists():
            parts.append(
                f"<h2>{html.escape(name)}</h2>"
                f"<pre>{html.escape(path.read_text(encoding='utf-8', errors='replace'))}</pre>"
            )

    parts.append("</body></html>")
    (docs_dir / "index.html").write_text("\n".join(parts), encoding="utf-8")


def safe_extract(tar: tarfile.TarFile, target: pathlib.Path) -> None:
    target = target.resolve()

    for member in tar.getmembers():
        member_path = (target / member.name).resolve()
        if not str(member_path).startswith(str(target) + "/"):
            raise tarfile.TarError(f"unsafe archive path: {member.name}")

    tar.extractall(target)


def copy_index_tree(extracted_dir: pathlib.Path) -> bool:
    indexes = sorted(extracted_dir.rglob("index.html"))
    if not indexes:
        return False

    root = indexes[0].parent
    for item in root.iterdir():
        dest = docs_dir / item.name
        if item.is_dir():
            shutil.copytree(item, dest, dirs_exist_ok=True)
        else:
            shutil.copy2(item, dest)

    return (docs_dir / "index.html").exists()


if url:
    try:
        with urllib.request.urlopen(url, timeout=30) as response:
            data = response.read()

        with tempfile.TemporaryDirectory() as tmp:
            extract_dir = pathlib.Path(tmp)
            with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
                safe_extract(tar, extract_dir)

            if not copy_index_tree(extract_dir):
                write_source_docs()

    except (OSError, tarfile.TarError, urllib.error.URLError) as error:
        print(f"offline docs download failed: {error}", file=sys.stderr)
        write_source_docs()
else:
    write_source_docs()

(out_dir / "offline-docs.stamp").write_text("ok", encoding="utf-8")
