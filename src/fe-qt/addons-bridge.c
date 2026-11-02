/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/outbound.h"
#include <gmodule.h>
#define PLUGIN_C
typedef struct session zoitechat_context;
#include "../common/zoitechat-plugin.h"
#include "../common/plugin.h"
#include "qt-bridge.h"
#include "addons-bridge.h"
#include <string.h>
/* Exercise both plugin configurations independently of the configured core. */
#ifdef QT_TEST_PLUGIN_ENABLED
#undef USE_PLUGIN
#if QT_TEST_PLUGIN_ENABLED
#define USE_PLUGIN 1
#endif
#endif
extern GSList *plugin_list;
int qt_core_addons_enabled(void)
{
#ifdef USE_PLUGIN
    return 1;
#else
    return 0;
#endif
}
void qt_core_addons_read(void (*visitor)(const struct QtAddon *, void *), void *data)
{
    if (!visitor) return;
    for (GSList *link = plugin_list; link; link = link->next) {
        zoitechat_plugin *plugin = link->data;
        if (!plugin->version || !*plugin->version) continue;
        struct QtAddon row = {plugin, plugin->name, plugin->version, plugin->desc, plugin->filename, plugin->fake, !plugin->filename || !*plugin->filename};
        visitor(&row, data);
    }
}
static int fail(char *error, int capacity, const char *message)
{ if (error && capacity > 0) g_strlcpy(error, message, capacity); return 0; }
int qt_core_addon_action(void *ptr, int action, void *identity, const char *name, const char *file, char *error, int capacity)
{
#ifndef USE_PLUGIN
    (void)ptr; (void)action; (void)identity; (void)name; (void)file;
    return fail(error, capacity, "Loadable plugin support is disabled in this build.");
#else
    if (!ptr || !is_session(ptr)) return fail(error, capacity, "The original conversation has closed.");
    if (action < 0 || action > 2 || !file || !*file || strlen(file) > 4096 || strpbrk(file, "\r\n")) return fail(error, capacity, "Invalid addon path or action.");
    int script = !g_str_has_suffix(file, "." PLUGIN_SUFFIX);
    if (action) {
        zoitechat_plugin *plugin = identity;
        if (!identity || !g_slist_find(plugin_list, identity) || g_strcmp0(plugin->name, name) || g_strcmp0(plugin->filename, file))
            return fail(error, capacity, "The selected addon changed or was unloaded. Refresh the list.");
        script = plugin->fake;
        if (!script) {
            int duplicates = 0;
            for (GSList *link = plugin_list; link; link = link->next) {
                zoitechat_plugin *other = link->data;
                if (!other->fake && other->filename && !g_ascii_strcasecmp(other->filename, file)) ++duplicates;
            }
            if (duplicates != 1) return fail(error, capacity, "Several native addons use this filename; selection is ambiguous.");
        }
    }
    // Copy snapshots before invoking plugin callbacks, which can mutate the list.
    char *filename = g_strdup(file); int result = 0;
    qt_core_activate(ptr);
    if (script) {
        if (strchr(filename, '"')) { g_free(filename); return fail(error, capacity, "Script paths containing double quotes cannot be passed to the core command parser."); }
        char *command = g_strdup_printf("%s \"%s\"", action == 0 ? "LOAD" : action == 1 ? "UNLOAD" : "RELOAD", filename);
        handle_command(ptr, command, FALSE); g_free(command); result = 1;
    } else if (!action) {
        char *message = plugin_load(ptr, filename, NULL);
        if (message) fail(error, capacity, message); else result = 1;
    } else if (action == 1) {
        const int status = plugin_kill(filename, TRUE);
        if (status == 1) result = 1;
        else fail(error, capacity, status == 2 ? "The plugin refused to unload." : "The plugin could not be unloaded.");
    } else {
        if (plugin_reload(ptr, filename, TRUE) == 1) result = 1;
        else fail(error, capacity, "The plugin could not be reloaded.");
    }
    g_free(filename); qt_fe_addons_changed(); return result;
#endif
}
