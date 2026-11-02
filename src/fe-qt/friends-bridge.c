/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/notify.h"
#include "../common/server.h"
#include "../common/util.h"
#include "friends-bridge.h"
#include <string.h>
static struct notify *find_friend(const char *name)
{
    GSList *entry;
    if (!name) return NULL;
    for (entry = notify_list; entry; entry = entry->next) {
        struct notify *friend = entry->data;
        if (!rfc_casecmp(friend->name, name)) return friend;
    }
    return NULL;
}
void qt_core_friends_foreach(QtFriendVisitor visitor, void *data)
{
    GSList *entry, *link;
    if (!visitor) return;
    for (entry = notify_list; entry; entry = entry->next) {
        struct notify *friend = entry->data;
        int emitted = FALSE;
        for (link = friend->server_list; link; link = link->next) {
            struct notify_per_server *state = link->data;
            if (!g_slist_find(serv_list, state->server) || !state->server->connected) continue;
            visitor(friend->name, friend->networks, server_get_network(state->server, TRUE), state->server,
                    state->ison, state->lastseen, state->laston, state->lastoff, data);
            emitted = TRUE;
        }
        if (!emitted) visitor(friend->name, friend->networks, "", NULL, -1, 0, 0, 0, data);
    }
}
int qt_core_friend_set(const char *old_name, const char *name, const char *networks)
{
    struct notify *old, *existing;
    if (!name || !*name || strlen(name) >= NICKLEN || strpbrk(name, " \t\r\n,!@*?")) return FALSE;
    if (networks && (strlen(networks) > 512 || strpbrk(networks, "\r\n"))) return FALSE;
    old = old_name && *old_name ? find_friend(old_name) : NULL;
    if (old_name && *old_name && !old) return FALSE;
    existing = find_friend(name);
    if (existing && existing != old) return FALSE;
    if (old) notify_deluser(old->name);
    notify_adduser((char *)name, networks && *networks ? (char *)networks : NULL);
    notify_save();
    return TRUE;
}
int qt_core_friend_remove(const char *name)
{
    if (!name || !notify_deluser((char *)name)) return FALSE;
    notify_save(); return TRUE;
}
int qt_core_friend_query(const char *name, void *ptr, const char *network)
{
    server *serv = ptr;
    GSList *link;
    struct notify *friend;
    if (!serv || !g_slist_find(serv_list, serv) || !serv->connected || !network ||
        strcmp(server_get_network(serv, TRUE), network)) return FALSE;
    friend = find_friend(name);
    if (!friend) return FALSE;
    for (link = friend->server_list; link; link = link->next) {
        struct notify_per_server *state = link->data;
        if (state->server == serv && state->ison) { new_ircwindow(serv, friend->name, SESS_DIALOG, 1); return TRUE; }
    }
    return FALSE;
}
void qt_core_new_tab(void *ptr, int server_tab)
{
    session *sess = ptr;
    if (server_tab) new_ircwindow(NULL, NULL, SESS_SERVER, 1);
    else if (sess && is_session(sess)) new_ircwindow(sess->server, NULL, SESS_CHANNEL, 1);
}
