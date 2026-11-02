/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "completion-bridge.h"
#include "../common/userlist.h"
#include <stdio.h>
#include <string.h>
static session context, channel, query;
GSList *sess_list, *command_list;
struct zoitechatprefs prefs;
static struct User alpha, zeta, myself;
GSList *userlist_flat_list(session *sess) { (void)sess; GSList *users = g_slist_append(NULL, &zeta); users = g_slist_append(users, &myself); return g_slist_append(users, &alpha); }
const struct commands xc_cmds[] = {{"JOIN", NULL, 0, 0, 0, NULL}, {NULL, NULL, 0, 0, 0, NULL}};
int is_session(session *sess) { return sess == &context; }
GList *plugin_command_list(GList *names) { return g_list_append(names, "PLUGINCOMMAND"); }
static void collect(const char *name, void *data) { g_ptr_array_add(data, g_strdup(name)); }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void) {
    struct popup user = {0}; user.name = "USERCOMMAND";
    command_list = g_slist_append(NULL, &user);
    channel.type = SESS_CHANNEL; strcpy(channel.channel, "#test"); query.type = SESS_DIALOG; strcpy(query.channel, "Someone");
    sess_list = g_slist_append(NULL, &channel); sess_list = g_slist_append(sess_list, &query);
    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
    qt_core_completion_read(NULL, 1, collect, names); CHECK(names->len == 0);
    qt_core_completion_read(&context, 1, collect, names); CHECK(names->len == 3);
    CHECK(!strcmp(names->pdata[0], "JOIN") && !strcmp(names->pdata[1], "USERCOMMAND") && !strcmp(names->pdata[2], "PLUGINCOMMAND"));
    g_ptr_array_set_size(names, 0); qt_core_completion_read(&context, 0, collect, names);
    CHECK(names->len == 1 && !strcmp(names->pdata[0], "#test"));
    server serv = {0}; serv.p_cmp = g_ascii_strcasecmp; context.server = &serv;
    CHECK(qt_core_completion_compare(&context, "ALICE", "alice") == 0);
    CHECK(qt_core_completion_compare(NULL, "Alice", "Zoe") < 0);
    strcpy(alpha.nick, "Alice"); alpha.lasttalk = 100; strcpy(zeta.nick, "Zoe"); zeta.lasttalk = 200;
    strcpy(myself.nick, "Myself"); myself.lasttalk = 300; myself.me = 1;
    g_ptr_array_set_size(names, 0); qt_core_completion_read(&context, 2, collect, names);
    CHECK(names->len == 3 && !strcmp(names->pdata[0], "Alice") && !strcmp(names->pdata[1], "Myself"));
    prefs.hex_completion_sort = 1; g_ptr_array_set_size(names, 0); qt_core_completion_read(&context, 2, collect, names);
    CHECK(names->len == 3 && !strcmp(names->pdata[0], "Zoe") && !strcmp(names->pdata[1], "Alice") && !strcmp(names->pdata[2], "Myself"));
    g_ptr_array_free(names, TRUE); g_slist_free(command_list); g_slist_free(sess_list); return 0;
}
