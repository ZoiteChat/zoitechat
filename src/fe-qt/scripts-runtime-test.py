#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run real scripting plugins against Qt/core in a relocated isolated layout."""
import os
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

binary = Path(sys.argv[1]).resolve()
build = binary.parents[2]
source = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="zoitechat-qt-scripts-") as temporary:
    root = Path(temporary)
    config = root / "config"
    addons = config / "addons"
    addons.mkdir(parents=True)
    (config / "zoitechat.conf").write_text("gui_slist_skip = 1\nidentd_server = 0\n")
    plugins = root / "lib/plugins"
    plugins.mkdir(parents=True)
    helpers = root / "lib/python"
    helpers.mkdir()
    for helper in ("_zoitechat.py", "zoitechat.py", "xchat.py"):
        shutil.copy2(source / "plugins/python" / helper, helpers)
    options = {row["name"]: row["value"] for row in json.loads((build / "meson-info/intro-buildoptions.json").read_text())}
    languages = []
    for language in ("python", "perl", "lua"):
        module = build / "plugins" / language / (language + ".so")
        if options["with-" + language] != "false":
            assert module.is_file(), "Configured scripting module is missing: " + str(module)
            shutil.copy2(module, plugins)
            languages.append(language)
    if not languages:
        print("SKIP: no scripting plugins built")
        sys.exit(77)
    fixtures = root / "script fixtures"
    fixtures.mkdir()
    (fixtures / "runtime.py").write_text('''import zoitechat, os
__module_name__ = "Qt runtime python"
__module_version__ = "1"
__module_description__ = "Runtime verification"
def record(value):
    with open(os.path.join(os.environ["QT_SCRIPT_ROOT"], "python.log"), "a", encoding="utf-8") as out:
        out.write(value + "\\n")
def command(word, word_eol, userdata):
    record("command:" + zoitechat.get_info("inputbox"))
    return zoitechat.EAT_ALL
def key(word, word_eol, userdata):
    record("key:" + ",".join(word[:4]))
    return zoitechat.EAT_NONE
zoitechat.hook_command("QT_SCRIPT_python", command)
zoitechat.hook_print("Key Press", key)
zoitechat.hook_unload(lambda userdata: record("unload"))
record("load")
''')
    (fixtures / "runtime.pl").write_text('''use strict;
use warnings;
use ZoiteChat qw(:all);
sub record {
    open my $out, '>>:encoding(UTF-8)', "$ENV{QT_SCRIPT_ROOT}/perl.log" or die $!;
    print $out $_[0], "\\n";
    close $out;
}
ZoiteChat::register("Qt runtime perl", "1", "Runtime verification", sub { record("unload"); });
ZoiteChat::hook_command("QT_SCRIPT_perl", sub { record("command:" . ZoiteChat::get_info("inputbox")); return EAT_ALL; });
ZoiteChat::hook_print("Key Press", sub { record("key:" . join(',', @{$_[0]}[0..3])); return EAT_NONE; });
record("load");
''')
    (fixtures / "runtime.lua").write_text('''local function record(value)
    local out = assert(io.open(os.getenv("QT_SCRIPT_ROOT") .. "/lua.log", "a"))
    out:write(value .. "\\n")
    out:close()
end
zoitechat.register("Qt runtime lua", "1", "Runtime verification")
zoitechat.hook_command("QT_SCRIPT_lua", function(word, word_eol) record("command:" .. zoitechat.get_info("inputbox")); return zoitechat.EAT_ALL end)
zoitechat.hook_print("Key Press", function(word) record("key:" .. table.concat(word, ",", 1, 4)); return zoitechat.EAT_NONE end)
zoitechat.hook_unload(function() record("unload") end)
record("load")
''')
    includes = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "glib-2.0"], text=True))
    subprocess.run([*shlex.split(os.environ.get("CC", "cc")), *includes, "-shared", "-fPIC", "-std=c11",
                    "-I" + str(source / "src/common"), "-I" + str(source / "src/fe-qt"),
                    str(source / "src/fe-qt/scripts-runtime-probe.c"), "-o", str(addons / "scripts-runtime.so")], check=True)
    report = root / "report"
    environment = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "ZOITECHAT_LIBDIR": str(plugins),
                   "QT_SCRIPT_ROOT": str(root), "QT_SCRIPT_REPORT": str(report),
                   "QT_SCRIPT_LANGS": ":" + ":".join(languages) + ":"}
    environment.pop("PYTHONPATH", None)
    environment.pop("PYTHONHOME", None)
    environment["PYTHONNOUSERSITE"] = "1"
    process = subprocess.Popen([str(binary), "--no-auto", "--cfgdir", str(config)], env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    succeeded = False
    try:
        deadline = time.monotonic() + 15
        while not report.exists() and time.monotonic() < deadline:
            assert process.poll() is None, "Application exited early"
            time.sleep(0.02)
        result = report.read_text() if report.exists() else "timeout"
        assert result.startswith("PASS"), result
        expected = ["load", "command:hé😀", "key:97,5,a,1", "unload"] * 2
        for language in languages:
            actual = (root / (language + ".log")).read_text().splitlines()
            # ZoiteChat retains a local reference to the old Python module until
            # reload_name returns, so its unload hook follows the replacement's load.
            if language == "python":
                wanted = [*expected[:3], "load", "unload", *expected[5:8],
                          "load", "unload", *expected[:4]]
            else:
                wanted = expected
            assert actual == wanted, (language, actual)
        succeeded = True
        print("PASS: relocated " + ", ".join(languages) + " load/reload/unload, addon rows, Unicode input, native Python interface reload and live command/Key Press hooks without stale callbacks")
    except BaseException:
        for log in root.glob("*.log"):
            print(log.name + ":\n" + log.read_text())
        raise
    finally:
        process.terminate()
        try:
            output, _ = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate(timeout=5)
        if not succeeded or process.returncode not in (0, -15):
            print(output)
