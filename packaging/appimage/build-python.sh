#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
# shellcheck source=python.env
source "$script_dir/python.env"
prefix="${1:?Usage: build-python.sh ABSOLUTE_INSTALL_PREFIX}"
[[ "$prefix" = /* ]]
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

curl -fL --retry 3 -o "$work_dir/python.tar.xz" \
    "https://www.python.org/ftp/python/$PYTHON_VERSION/Python-$PYTHON_VERSION.tar.xz"
printf '%s  %s\n' "$PYTHON_SHA256" "$work_dir/python.tar.xz" | sha256sum --check --strict
tar -xJf "$work_dir/python.tar.xz" -C "$work_dir"
cd "$work_dir/Python-$PYTHON_VERSION"

# Build on the AppImage's oldest supported distribution. A newer runner
# would raise the glibc requirement for the interpreter and its extensions.
./configure --prefix="$prefix" --enable-shared --without-static-libpython --with-ensurepip=install
make -j"$(nproc)"
LD_LIBRARY_PATH="$PWD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" make install
export LD_LIBRARY_PATH="$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$prefix/bin/python3" -m pip install --disable-pip-version-check \
    --only-binary=:all: --require-hashes -r "$script_dir/python-requirements.txt"

# Fail here if configure silently omitted a standard-library extension.
"$prefix/bin/python3" -I -c \
    'import bz2, ctypes, curses, dbm.gnu, lzma, readline, sqlite3, ssl, tkinter, zlib, cffi, _cffi_backend; print(ssl.OPENSSL_VERSION)'
