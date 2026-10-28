#!/usr/bin/env python3
"""Exercise an extracted AppImage without borrowing the build interpreter."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


PROBE = r'''
import bz2, ctypes, curses, dbm.gnu, json, lzma, pathlib, readline, sqlite3, ssl, sys, tkinter, zlib
import cffi, _cffi_backend, pycparser
root = pathlib.Path(sys.argv[1]).resolve()
assert pathlib.Path(sys.base_prefix).resolve() == root / 'usr', sys.base_prefix
for module in (cffi, _cffi_backend, pycparser, ssl, sqlite3):
    assert pathlib.Path(module.__file__).resolve().is_relative_to(root), module.__file__
assert cffi.FFI().sizeof('int') > 0
assert tkinter.Tcl().eval('info patchlevel').startswith('8.6.')
assert ssl.create_default_context().get_ca_certs(), 'No trusted CA certificates'
gio = ctypes.CDLL('libgio-2.0.so.0')
gio.g_tls_backend_get_default.restype = ctypes.c_void_p
gio.g_tls_backend_supports_tls.argtypes = [ctypes.c_void_p]
assert gio.g_tls_backend_supports_tls(gio.g_tls_backend_get_default()), 'GIO TLS backend unavailable'
print(json.dumps({'python': sys.version, 'openssl': ssl.OPENSSL_VERSION,
                  'cffi': cffi.__version__, 'pycparser': pycparser.__version__}))
'''

PYTHON_PLUGIN = r'''
import os
from pathlib import Path
import sys
import zoitechat, hexchat, xchat
__module_name__ = 'AppImage Python check'
__module_version__ = '1'
root = Path(os.environ['ZOITECHAT_SMOKE'])
(root / 'python.ok').write_text(sys.version)
def finish(userdata):
    if all((root / (name + '.ok')).is_file() for name in ('python', 'perl', 'lua')):
        # Return from Python before shutdown unloads the native plugin.
        zoitechat.command('TIMER 0.1 KILLALL')
        return False
    return True
zoitechat.hook_timer(100, finish)
'''

PERL_PLUGIN = r'''
use strict;
use warnings;
use File::Spec;
use Encode;
ZoiteChat::register('AppImage Perl check', '1', 'Runtime check');
open my $marker, '>', File::Spec->catfile($ENV{ZOITECHAT_SMOKE}, 'perl.ok') or die $!;
print {$marker} $^V;
close $marker or die $!;
'''

LUA_PLUGIN = r'''
zoitechat.register('AppImage Lua check', '1', 'Runtime check')
local marker = assert(io.open(os.getenv('ZOITECHAT_SMOKE') .. '/lua.ok', 'w'))
marker:write(_VERSION)
marker:close()
'''


def run(command, env):
    try:
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=90)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f"{command[0]} timed out:\n{error.stdout}\n{error.stderr}") from error
    if result.returncode:
        raise RuntimeError(f"{command[0]} failed:\n{result.stdout}\n{result.stderr}")
    return result.stdout.strip()


def main():
    appdir = Path(sys.argv[1]).resolve()
    env = os.environ.copy()
    for key in ("PYTHONHOME", "PYTHONPATH", "PERL5LIB", "LD_LIBRARY_PATH",
                "GIO_MODULE_DIR", "GIO_EXTRA_MODULES"):
        env.pop(key, None)
    env.update({
        "APPDIR": str(appdir),
        "PYTHONHOME": str(appdir / "usr"),
        "PYTHONNOUSERSITE": "1",
        "LD_LIBRARY_PATH": f"{appdir}/usr/lib:{appdir}/usr/lib/x86_64-linux-gnu",
        "SSL_CERT_FILE": str(appdir / "etc/ssl/certs/ca-certificates.crt"),
        "SSL_CERT_DIR": str(appdir / "etc/ssl/certs"),
        "GDK_BACKEND": "x11",
        "TCL_LIBRARY": str(appdir / "usr/share/tcltk/tcl8.6"),
        "TK_LIBRARY": str(appdir / "usr/share/tcltk/tk8.6"),
    })
    for relative in ("usr/lib/x86_64-linux-gnu/gio/modules", "usr/lib/gio/modules"):
        if (appdir / relative).is_dir():
            env["GIO_MODULE_DIR"] = str(appdir / relative)
            env["GIO_EXTRA_MODULES"] = str(appdir / relative)
            break
    perl_dirs = [appdir / "usr/lib/x86_64-linux-gnu/perl-base"]
    for relative in ("usr/lib/x86_64-linux-gnu/perl", "usr/lib/x86_64-linux-gnu/perl5", "usr/share/perl"):
        perl_dirs.extend(sorted((appdir / relative).glob("*")))
    perl_dirs.append(appdir / "usr/share/perl5")
    env["PERL5LIB"] = ":".join(str(path) for path in perl_dirs if path.is_dir())
    runtime = json.loads(run([str(appdir / "usr/bin/python3"), "-s", "-c", PROBE, str(appdir)], env))
    expected = dict(line.split("=", 1) for line in
                    Path(__file__).with_name("python.env").read_text().splitlines()
                    if line and not line.startswith("#"))["PYTHON_VERSION"]
    assert runtime["python"].split()[0] == expected, runtime
    runtime["perl"] = run([str(appdir / "usr/bin/perl"), "-MFile::Spec", "-MEncode", "-MPOSIX",
                           "-MJSON::PP", "-e", "print $^V"], env)

    with tempfile.TemporaryDirectory(prefix="zoitechat-smoke-") as temporary:
        config = Path(temporary)
        (config / "zoitechat.conf").write_text("gui_slist_skip = 1\n")
        autoload = config / "addons"
        autoload.mkdir()
        for filename, content in (("smoke.py", PYTHON_PLUGIN), ("smoke.pl", PERL_PLUGIN), ("smoke.lua", LUA_PLUGIN)):
            (autoload / filename).write_text(content)
        env["HOME"] = temporary
        env["XDG_CONFIG_HOME"] = temporary
        env["ZOITECHAT_SMOKE"] = temporary
        output = run(["xvfb-run", "-a", "dbus-run-session", "--", str(appdir / "AppRun"),
                      "--run-as-root", "--no-auto", "--cfgdir", temporary], env)
        for name in ("python", "perl", "lua"):
            assert (config / f"{name}.ok").is_file(), f"{name} plugin failed:\n{output}"
        runtime["lua"] = (config / "lua.ok").read_text()

    (appdir.parent / "runtime.json").write_text(json.dumps(runtime, indent=2) + "\n")
    print(json.dumps(runtime, indent=2))


if __name__ == "__main__":
    main()
