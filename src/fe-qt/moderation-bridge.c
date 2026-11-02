/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "moderation-bridge.h"
#include <string.h>

static session *live_channel(void *ptr, const char *channel)
{
    session *sess = ptr;
    if (!sess || !is_session(sess) || sess->type != SESS_CHANNEL ||
        !sess->server->connected || !sess->channel[0]) return NULL;
    if (channel && sess->server->p_cmp(sess->channel, channel)) return NULL;
    return sess;
}
static const char *simple_modes(const char *supported)
{
    int i;
    const char *p = supported ? supported : "";
    for (i = 0; i < 3; ++i) {
        p = strchr(p, ',');
        if (!p) return "";
        ++p;
    }
    return p;
}
static int list_supported(session *sess, char mode)
{
    const char *modes = sess->server->chanmodes;
    const char *found = modes ? strchr(modes, mode) : NULL;
    return found && (size_t)(found - modes) < strcspn(modes, ",");
}
int qt_core_channel_state(void *ptr, QtChannelState *state)
{
    session *sess = live_channel(ptr, NULL);
    if (!sess || !state) return FALSE;
    state->channel = sess->channel;
    state->modes = sess->current_modes ? sess->current_modes : "";
    state->supported = sess->server->chanmodes ? sess->server->chanmodes : "";
    state->simple = simple_modes(state->supported);
    /* The legacy parser supplies a channel context for b/e completion replies.
     * I/q completion lacks that context, so do not expose those lists yet. */
    state->lists = list_supported(sess, 'b') | (list_supported(sess, 'e') << 1);
    state->limit = sess->limit;
    state->key_set = sess->channelkey[0] != 0;
    return TRUE;
}
int qt_core_channel_refresh(void *ptr, const char *channel)
{
    session *sess = live_channel(ptr, channel);
    if (!sess) return FALSE;
    sess->server->p_mode(sess->server, sess->channel, "");
    return TRUE;
}
int qt_core_ban_request(void *ptr, const char *channel, char mode)
{
    session *sess = live_channel(ptr, channel);
    char command[3] = {'+', mode, 0};
    if (!sess || !strchr("be", mode) || !mode || !list_supported(sess, mode)) return FALSE;
    sess->server->p_mode(sess->server, sess->channel, command);
    return TRUE;
}
int qt_core_channel_mode(void *ptr, const char *channel, char mode, int enabled, const char *value)
{
    session *sess = live_channel(ptr, channel);
    char *command;
    const char *argument = NULL;
    if (!sess || !mode || !value || strpbrk(value, " \t\r\n") || strlen(value) > 256) return FALSE;
    if (strchr("be", mode) && list_supported(sess, mode)) {
        if (!*value) return FALSE;
        argument = value;
    } else if (mode == 'k' && sess->server->chanmodes && strchr(sess->server->chanmodes, 'k')) {
        if (enabled && (!*value || strlen(value) >= sizeof(sess->channelkey))) return FALSE;
        argument = enabled ? value : (sess->channelkey[0] ? sess->channelkey : "*");
    } else if (mode == 'l' && sess->server->chanmodes && strchr(sess->server->chanmodes, 'l')) {
        if (enabled) {
            char *end;
            gint64 limit = g_ascii_strtoll(value, &end, 10);
            if (!*value || *end || limit < 1 || limit > G_MAXINT) return FALSE;
            argument = value;
        }
    } else if (!strchr(simple_modes(sess->server->chanmodes), mode)) {
        return FALSE;
    }
    command = argument ? g_strdup_printf("%c%c %s", enabled ? '+' : '-', mode, argument) :
                         g_strdup_printf("%c%c", enabled ? '+' : '-', mode);
    sess->server->p_mode(sess->server, sess->channel, command);
    g_free(command);
    return TRUE;
}
