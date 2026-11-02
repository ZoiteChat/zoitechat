/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include <gmodule.h>
#define PLUGIN_C
typedef struct session zoitechat_context;
#include "../common/zoitechat-plugin.h"
#include "../common/plugin.h"
#include "addons-bridge.h"
#include <stdio.h>
#include <string.h>
/* Exercise both plugin configurations independently of the configured core. */
#ifdef QT_TEST_PLUGIN_ENABLED
#undef USE_PLUGIN
#if QT_TEST_PLUGIN_ENABLED
#define USE_PLUGIN 1
#endif
#endif
GSList *plugin_list;
static session context;
static int loaded, killed, reloaded, commands, refusal;
static char last[8192];
int is_session(session *sess) { return sess == &context; }
void qt_core_activate(void *sess) { (void)sess; }
void qt_fe_addons_changed(void) {}
char *plugin_load(session *sess, char *filename, char *arg) { (void)sess; (void)arg; ++loaded; g_strlcpy(last, filename, sizeof(last)); return NULL; }
int plugin_kill(char *name, int by_file) { (void)name; (void)by_file; ++killed; return refusal ? 2 : 1; }
int plugin_reload(session *sess, char *name, int by_file) { (void)sess; (void)name; (void)by_file; ++reloaded; return 1; }
int handle_command(session *sess, char *command, int special) { (void)sess; (void)special; ++commands; g_strlcpy(last, command, sizeof(last)); return 1; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    char error[1024];
#ifdef USE_PLUGIN
    CHECK(qt_core_addons_enabled());
    CHECK(qt_core_addon_action(&context, 0, NULL, "", "/tmp/test file.so", error, sizeof(error)) && loaded == 1 && !strcmp(last, "/tmp/test file.so"));
    CHECK(qt_core_addon_action(&context, 0, NULL, "", "/tmp/test file.py", error, sizeof(error)) && commands == 1 && !strcmp(last, "LOAD \"/tmp/test file.py\""));
    CHECK(!qt_core_addon_action(NULL, 0, NULL, "", "file.so", error, sizeof(error)));
    CHECK(!qt_core_addon_action(&context, 0, NULL, "", "file\nquit.py", error, sizeof(error)));
    CHECK(!qt_core_addon_action(&context, 0, NULL, "", "file\".py", error, sizeof(error)));
    zoitechat_plugin plugin = {0}; plugin.name = "Native"; plugin.filename = "/tmp/test.so"; plugin.version = "1";
    plugin_list = g_slist_append(NULL, &plugin);
    CHECK(qt_core_addon_action(&context, 1, &plugin, "Native", plugin.filename, error, sizeof(error)) && killed == 1);
    refusal = 1; CHECK(!qt_core_addon_action(&context, 1, &plugin, "Native", plugin.filename, error, sizeof(error)));
    CHECK(qt_core_addon_action(&context, 2, &plugin, "Native", plugin.filename, error, sizeof(error)) && reloaded == 1);
    CHECK(!qt_core_addon_action(&context, 1, &plugin, "Changed", plugin.filename, error, sizeof(error)));
    zoitechat_plugin duplicate = plugin; plugin_list = g_slist_append(plugin_list, &duplicate);
    CHECK(!qt_core_addon_action(&context, 1, &plugin, "Native", plugin.filename, error, sizeof(error)));
    plugin_list = g_slist_remove(plugin_list, &duplicate); plugin.fake = 1; plugin.filename = "/tmp/script.py";
    CHECK(qt_core_addon_action(&context, 1, &plugin, "Native", plugin.filename, error, sizeof(error)) && !strcmp(last, "UNLOAD \"/tmp/script.py\""));
    g_slist_free(plugin_list); plugin_list = NULL;
    CHECK(!qt_core_addon_action(&context, 1, &plugin, "Native", plugin.filename, error, sizeof(error)));
#else
    CHECK(!qt_core_addons_enabled());
    CHECK(!qt_core_addon_action(&context, 0, NULL, "", "/tmp/test.so", error, sizeof(error)) && !loaded && !commands);
#endif
    return 0;
}
