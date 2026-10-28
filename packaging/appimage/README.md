# AppImage build

The workflow stays on Ubuntu 22.04 so updating Python does not also raise the
AppImage's glibc requirement. Python is built separately from a checked source
archive. `python.env` holds its version and SHA-256. CFFI and pycparser are pinned
in `python-requirements.txt`, including wheel hashes.

The Python interpreter used to build the documentation is separate from the
one used by the scripting plugin. The `docs-python` Meson option selects the
interpreter with Sphinx; it defaults to `python3` for normal builds.

## What was in the published build

Checked on October 27, 2026, using the 2.20.0-dev release asset:

`Zoitechat-2.20.0-dev-x86_64.AppImage`

SHA-256: `062eff9ba37556d3eca6c17388db45451353732a0cdf15c2bf3d6a3eae620989`

| Component | Published AppImage | This build |
| --- | --- | --- |
| Python | 3.10.12, built August 31, 2026 | 3.14.10 |
| CFFI | 1.15.0 | 2.1.1 |
| pycparser | 2.21 | 3.12 |
| Perl | 5.34.0 | Ubuntu 22.04 package, with available updates |
| OpenSSL reported by Python | 3.0.2 | Ubuntu 22.04 package, with available updates |

The old build copied the runner's entire Python `dist-packages` tree. That
included cloud-init, the Azure agent, apt bindings, Sphinx and other build-host
packages. The new bundle includes the standard library and the two declared
Python plugin dependencies. It leaves out CPython regression tests, bytecode
caches, static libraries and build tools. Tcl/Tk's runtime scripts are included
for the standard library's tkinter module.

Python 3.10 reached upstream end of life in October 2026. That does not mean
every Ubuntu Python 3.10 package is unpatched: Ubuntu backports security fixes.
The same applies to the old-looking OpenSSL and Perl version strings. The old
AppImage did not record the Ubuntu package revisions, so those strings alone
cannot establish its vulnerability status.

Python 3.14 is a deliberate choice. It is maintained upstream and supported by
CFFI. Moving a scripting runtime can still affect scripts using removed Python
APIs or native extensions built for Python 3.10. Third-party modules required
by user scripts need to be supplied for the new interpreter; incidental runner
packages are no longer treated as part of the scripting API.

## Checks and rebuilds

The workflow can be run manually and rebuilds weekly to pick up available
Ubuntu package and CA certificate updates. It creates an artifact; releasing a
new download is still a separate step. Already downloaded AppImages do not
receive those updates.

After packaging, the workflow extracts the AppImage into a path containing a
space and hides the Python build prefix. `check-runtime.py` checks the bundled
interpreter, native standard-library extensions, CFFI, CA loading and GIO TLS.
It starts the actual application under Xvfb and runs Python, Perl and Lua
scripts through their plugins. A missing script marker, process error or timeout
fails the build. Existing emoji tests remain in the workflow.

The `zoitechat-appimage-audit` artifact contains runtime versions, the build
host's exact Ubuntu package revisions, hashes of the deployment helpers and
the final AppImage checksum. The package list describes the build host, not an
SBOM of the AppImage. linuxdeploy's `continuous` download and its output plugin
remain upstream-managed; their versions should be reviewed when packaging
changes.

The GTK deployment helper is pinned to a commit. Its unconditional X11
override is removed so AppRun can honour `GDK_BACKEND` and choose Wayland with
X11 fallback. Hooks run once through linuxdeploy's generated outer launcher.
The Xvfb test covers X11; Wayland still needs testing in a Wayland session.

When updating Python, change its version and checksum together, refresh the
two wheel pins and hashes, and run the workflow. Review the audit artifact and
test the result on both Ubuntu 22.04 and a current distribution before release.

References:

- [Python release status](https://devguide.python.org/versions/)
- [Python 3.14.10](https://www.python.org/downloads/release/python-31410/)
- [CFFI release notes](https://cffi.readthedocs.io/en/latest/whatsnew.html)
- [Ubuntu security updates](https://documentation.ubuntu.com/security/security-updates/)
