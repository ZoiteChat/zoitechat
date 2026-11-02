/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/outbound.h"
#include "../common/plugin.h"
#include "completion-bridge.h"
#include "../common/userlist.h"
static gint compare_nicks(gconstpointer left, gconstpointer right, gpointer data)
{
    const struct User *a = left, *b = right; session *sess = data;
    if (prefs.hex_completion_sort == 1) {
        if (a->me != b->me) return a->me ? 1 : -1;
        if (a->lasttalk != b->lasttalk) return a->lasttalk > b->lasttalk ? -1 : 1;
    }
    return sess->server && sess->server->p_cmp ? sess->server->p_cmp(a->nick, b->nick) : g_ascii_strcasecmp(a->nick, b->nick);
}
void qt_core_completion_read(void *ptr, int kind, void (*emit)(const char *, void *), void *userdata)
{
    session *sess = ptr;
    if (!sess || !is_session(sess) || !emit) return;
    if (kind == 2) {
        GSList *users = userlist_flat_list(sess);
        users = g_slist_sort_with_data(users, compare_nicks, sess);
        for (GSList *link = users; link; link = link->next) emit(((struct User *)link->data)->nick, userdata);
        g_slist_free(users);
    } else if (kind == 1) {
        for (int i = 0; xc_cmds[i].name; ++i) emit(xc_cmds[i].name, userdata);
        for (GSList *link = command_list; link; link = link->next) { struct popup *entry = link->data; if (entry->name && *entry->name) emit(entry->name, userdata); }
        /* Hook names belong to plugins; only free the temporary list nodes. */
        GList *names = plugin_command_list(NULL);
        for (GList *link = names; link; link = link->next) if (link->data) emit(link->data, userdata);
        g_list_free(names);
    } else if (kind == 0) {
        for (GSList *link = sess_list; link; link = link->next) {
            session *other = link->data;
            if (other->type == SESS_CHANNEL && *other->channel) emit(other->channel, userdata);
        }
    }
}

int qt_core_completion_compare(void *ptr, const char *left, const char *right)
{
    session *sess = ptr;
    if (!left || !right) return g_strcmp0(left, right);
    if (sess && is_session(sess) && sess->server && sess->server->p_cmp) return sess->server->p_cmp(left, right);
    return g_ascii_strcasecmp(left, right);
}

const char *qt_core_command_prefix(void) { return prefs.hex_input_command_char; }
