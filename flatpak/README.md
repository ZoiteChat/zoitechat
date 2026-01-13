# Flatpak Python plugin notes

The Python plugin embeds a Python interpreter and imports the ZoiteChat Python
module during plugin initialization. That initialization relies on the Python
standard library being available at runtime.

The Flatpak manifest bundles a Python runtime (built into `/app`) alongside
`python3-cffi` so the embedded interpreter can find a standard library at
runtime. The manifest also sets `PYTHONHOME=/app` to point the embedded runtime
at the bundled stdlib.

If you change the bundled Python version, update `flatpak/python3.json` and the
`/lib/python3.x` cleanup paths accordingly, and make sure the `python3-cffi`
module installs into the same versioned `site-packages` directory.
