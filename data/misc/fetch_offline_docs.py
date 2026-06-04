#!/usr/bin/env python3
import html
import io
import pathlib
import shutil
import sys
import tarfile
import urllib.error
import urllib.request

url = sys.argv[1]
out_dir = pathlib.Path(sys.argv[2])
source_dir = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else pathlib.Path.cwd()
docs_dir = out_dir / 'offline-docs'
if docs_dir.exists():
    shutil.rmtree(docs_dir)
docs_dir.mkdir(parents=True, exist_ok=True)

def write_source_docs():
    parts = ['<!doctype html><html><head><meta charset="utf-8"><title>ZoiteChat Documentation</title></head><body><h1>ZoiteChat Documentation</h1>']
    for name in ('readme.md', 'troubleshooting.md', 'changelog.rst'):
        path = source_dir / name
        if path.exists():
            parts.append(f'<h2>{html.escape(name)}</h2><pre>{html.escape(path.read_text(encoding="utf-8", errors="replace"))}</pre>')
    parts.append('</body></html>')
    (docs_dir / 'index.html').write_text('\n'.join(parts), encoding='utf-8')

if url:
    try:
        with urllib.request.urlopen(url, timeout=30) as r:
            data = r.read()
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as tar:
            try:
                tar.extractall(docs_dir, filter='data')
            except TypeError:
                tar.extractall(docs_dir)
    except (OSError, tarfile.TarError, urllib.error.URLError) as error:
        print(f'offline docs download failed: {error}', file=sys.stderr)
        write_source_docs()
else:
    write_source_docs()
(out_dir / 'offline-docs.stamp').write_text('ok')
