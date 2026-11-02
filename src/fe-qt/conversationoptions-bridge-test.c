/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "conversationoptions-bridge.h"
#include <stdio.h>
#include <string.h>
static session context;
static int saved, logging;
int is_session(session *sess) { return sess == &context; }
char *server_get_network(server *serv, gboolean fallback) { (void)serv; (void)fallback; return "Network"; }
void chanopt_save(session *sess) { (void)sess; ++saved; }
void log_open_or_close(session *sess) { (void)sess; ++logging; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    context.type = SESS_CHANNEL; strcpy(context.session_name, "#test");
    context.alert_balloon = context.alert_beep = context.alert_taskbar = context.alert_tray = context.text_hidejoinpart = context.text_logging = context.text_scrollback = context.text_strip = SET_DEFAULT;
    struct QtConversationOptions state; CHECK(qt_core_conversation_options(&context, &state) && state.persistent);
    char error[1024]; int values[8] = {1,0,2,0,1,1,0,1};
    CHECK(qt_core_conversation_options_apply(&context, "#test", "Network", state.values, values, error, sizeof(error)));
    CHECK(saved == 1 && logging == 1 && context.alert_balloon == SET_ON && context.alert_tray == SET_OFF && context.text_strip == SET_ON);
    CHECK(!qt_core_conversation_options_apply(&context, "#test", "Network", state.values, values, error, sizeof(error)) && saved == 1);
    CHECK(qt_core_conversation_options(&context, &state)); values[7] = 3;
    CHECK(!qt_core_conversation_options_apply(&context, "#test", "Network", state.values, values, error, sizeof(error)) && context.text_strip == SET_ON);
    values[7] = 2; CHECK(!qt_core_conversation_options_apply(&context, "#other", "Network", state.values, values, error, sizeof(error)));
    CHECK(!qt_core_conversation_options_apply(NULL, "#test", "Network", state.values, values, error, sizeof(error)));
    return 0;
}
