/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/notify.h"
#include "../common/server.h"
#include "friends-bridge.h"
#include <stdio.h>
#include <string.h>
GSList *notify_list, *serv_list;
static session context;
static int saves, windows, last_type, visited, last_online;
static server *last_server;
int rfc_casecmp(const char *a, const char *b) { return g_ascii_strcasecmp(a, b); }
int is_session(session *sess) { return sess == &context; }
char *server_get_network(server *serv, gboolean fallback) { (void)fallback; return serv->servername; }
session *new_ircwindow(server *serv, char *name, int type, int focus)
{ (void)name; (void)focus; ++windows; last_type = type; last_server = serv; return &context; }
void notify_save(void) { ++saves; }
void notify_adduser(char *name, char *networks)
{
    struct notify *friend = g_new0(struct notify, 1); friend->name = g_strdup(name); friend->networks = g_strdup(networks);
    notify_list = g_slist_prepend(notify_list, friend);
}
int notify_deluser(char *name)
{
    GSList *entry;
    for (entry = notify_list; entry; entry = entry->next) {
        struct notify *friend = entry->data;
        if (!g_ascii_strcasecmp(name, friend->name)) {
            notify_list = g_slist_remove(notify_list, friend); g_free(friend->name); g_free(friend->networks);
            g_slist_free(friend->server_list); g_free(friend); return 1;
        }
    }
    return 0;
}
static void visit(const char *name, const char *networks, const char *network, void *serv, int online,
                  time_t seen, time_t on, time_t off, void *data)
{
    (void)name; (void)networks; (void)network; (void)serv; (void)seen; (void)on; (void)off; (void)data;
    ++visited; last_online = online;
}
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); return 1; } } while (0)
int main(void)
{
    server serv = {0}; struct notify_per_server state = {0};
    g_strlcpy(serv.servername, "Network", sizeof(serv.servername)); serv.connected = 1; context.server = &serv;
    serv_list = g_slist_prepend(NULL, &serv);
    CHECK(qt_core_friend_set("", "Alice", "Network")); CHECK(saves == 1);
    CHECK(!qt_core_friend_set("", "alice", "Other")); CHECK(saves == 1);
    CHECK(!qt_core_friend_set("", "bad nick", "")); CHECK(!qt_core_friend_set("", "nick!*@*", ""));
    CHECK(!qt_core_friend_set("missing", "Bob", "")); CHECK(!qt_core_friend_set("", "Bob", "bad\nnetwork"));
    qt_core_friends_foreach(visit, NULL); CHECK(visited == 1 && last_online == -1);
    state.server = &serv; state.ison = TRUE;
    ((struct notify *)notify_list->data)->server_list = g_slist_prepend(NULL, &state);
    visited = 0; qt_core_friends_foreach(visit, NULL); CHECK(visited == 1 && last_online == 1);
    CHECK(qt_core_friend_query("ALICE", &serv, "Network") && last_type == SESS_DIALOG && last_server == &serv);
    CHECK(!qt_core_friend_query("Alice", &serv, "Other"));
    state.ison = FALSE; CHECK(!qt_core_friend_query("Alice", &serv, "Network"));
    serv.connected = FALSE; CHECK(!qt_core_friend_query("Alice", &serv, "Network"));
    visited = 0; qt_core_friends_foreach(visit, NULL); CHECK(visited == 1 && last_online == -1);
    CHECK(qt_core_friend_set("Alice", "Bob", "")); CHECK(saves == 2 && !strcmp(((struct notify *)notify_list->data)->name, "Bob"));
    CHECK(qt_core_friend_remove("bob") && saves == 3); CHECK(!qt_core_friend_remove("missing"));
    qt_core_new_tab(NULL, TRUE); CHECK(last_type == SESS_SERVER && last_server == NULL);
    qt_core_new_tab(&context, FALSE); CHECK(last_type == SESS_CHANNEL && last_server == &serv);
    int before = windows; qt_core_new_tab(NULL, FALSE); CHECK(windows == before);
    g_slist_free(serv_list);
    return 0;
}
