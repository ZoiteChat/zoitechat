/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/chanopt.h"
#include "../common/server.h"
#include "../common/text.h"
#include "conversationoptions-bridge.h"
#include <stddef.h>
#include <string.h>
static const size_t offsets[] = {offsetof(session, alert_balloon), offsetof(session, alert_beep), offsetof(session, alert_taskbar), offsetof(session, alert_tray),
    offsetof(session, text_hidejoinpart), offsetof(session, text_logging), offsetof(session, text_scrollback), offsetof(session, text_strip)};
int qt_core_conversation_options(void *ptr, struct QtConversationOptions *state)
{
    session *sess = ptr;
    if (!sess || !state || !is_session(sess) || (sess->type != SESS_CHANNEL && sess->type != SESS_DIALOG) || !*sess->session_name) return 0;
    state->name = sess->session_name; const char *network = server_get_network(sess->server, FALSE);
    state->network = network ? network : ""; state->persistent = network != NULL;
    for (int i = 0; i < QT_CONVERSATION_OPTION_COUNT; ++i) state->values[i] = *(guint8 *)((char *)sess + offsets[i]);
    return 1;
}
static int fail(char *error, int capacity, const char *message)
{ if (error && capacity > 0) g_strlcpy(error, message, capacity); return 0; }
int qt_core_conversation_options_apply(void *ptr, const char *name, const char *network, const int *expected, const int *values, char *error, int capacity)
{
    struct QtConversationOptions state;
    if (!expected || !values || !qt_core_conversation_options(ptr, &state) || g_strcmp0(name, state.name) || g_strcmp0(network, state.network)) return fail(error, capacity, "The original conversation is no longer available.");
    for (int i = 0; i < QT_CONVERSATION_OPTION_COUNT; ++i) {
        if (values[i] < SET_OFF || values[i] > SET_DEFAULT) return fail(error, capacity, "Invalid conversation option value.");
        if (expected[i] != state.values[i]) return fail(error, capacity, "Conversation options changed. Reload before applying.");
    }
    for (int i = 0; i < QT_CONVERSATION_OPTION_COUNT; ++i) *(guint8 *)((char *)ptr + offsets[i]) = values[i];
    chanopt_save(ptr); log_open_or_close(ptr); return 1;
}
