/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "zoitechat-plugin.h"
#include "addons-bridge.h"
#include "qt-bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static zoitechat_plugin *plugin;
static int language, phase, matches;
static void *identity;
static char filename[4096], addon_name[256];
static const char *languages[] = {"python", "perl", "lua"};
static const char *extensions[] = {"py", "pl", "lua"};
static void inspect(const struct QtAddon *row, void *unused)
{
    (void)unused;
    if (row->filename && !strcmp(row->filename, filename)) {
        ++matches; identity = row->identity;
        snprintf(addon_name, sizeof(addon_name), "%s", row->name);
    }
}
static int finish(const char *result)
{
    const char *path = getenv("QT_SCRIPT_REPORT");
    char temporary[4096];
    snprintf(temporary, sizeof(temporary), "%s.new", path);
    FILE *out = fopen(temporary, "w");
    if (out) {
        int written = fprintf(out, "%s language=%d phase=%d\n", result, language, phase);
        int closed = fclose(out);
        if (written > 0 && !closed) rename(temporary, path);
    }
    return 0;
}
static int tick(void *unused)
{
    char command[128], error[512], wanted[64];
    void *context = plugin->zoitechat_get_context(plugin);
    (void)unused;
    while (language < 3) {
        snprintf(wanted, sizeof(wanted), ":%s:", languages[language]);
        if (strstr(getenv("QT_SCRIPT_LANGS"), wanted)) break;
        ++language;
    }
    if (language == 3) return finish("PASS");
    snprintf(filename, sizeof(filename), "%s/script fixtures/runtime.%s", getenv("QT_SCRIPT_ROOT"), extensions[language]);
    matches = 0; identity = NULL;
    qt_core_addons_read(inspect, NULL);
    if (!phase) {
        if (matches || !qt_core_addon_action(context, 0, NULL, "", filename, error, sizeof(error))) return finish("FAIL load");
    } else if (phase == 1 || phase == 2 || phase == 6) {
        if (matches != 1) return finish("FAIL missing or duplicate script row");
        qt_fe_set_input(context, "h\303\251\360\237\230\200");
        snprintf(command, sizeof(command), "QT_SCRIPT_%s", languages[language]);
        qt_core_submit_command(context, command);
        if (qt_core_keypress(context, 5, 97, 97)) return finish("FAIL unexpected key consumption");
        if (!qt_core_addon_action(context, phase == 1 ? 2 : 1, identity, addon_name, filename, error, sizeof(error))) return finish("FAIL reload/unload");
    } else if (phase == 4) {
        if (matches != 1) return finish("FAIL missing script before interface unload");
        snprintf(filename, sizeof(filename), "%s/lib/plugins/python.so", getenv("QT_SCRIPT_ROOT"));
        matches = 0; identity = NULL;
        qt_core_addons_read(inspect, NULL);
        if (matches != 1 || !qt_core_addon_action(context, 2, identity, addon_name, filename, error, sizeof(error))) return finish("FAIL native Python interface reload");
    } else {
        if (matches) return finish("FAIL stale script row after unload");
        snprintf(command, sizeof(command), "QT_SCRIPT_%s", languages[language]);
        qt_core_submit_command(context, command);
        qt_core_keypress(context, 5, 97, 97);
        if (!language && (phase == 3 || phase == 5)) {
            if (!qt_core_addon_action(context, 0, NULL, "", filename, error, sizeof(error))) return finish("FAIL load before interface unload");
        } else {
            ++language; phase = 0;
            return 1;
        }
    }
    ++phase;
    return 1;
}
int zoitechat_plugin_init(zoitechat_plugin *handle, char **name, char **description, char **version, char *argument)
{
    (void)argument;
    plugin = handle; *name = "Qt scripts runtime probe"; *description = "Isolated script lifecycle verification"; *version = "1";
    plugin->zoitechat_hook_timer(plugin, 50, tick, NULL);
    return 1;
}
