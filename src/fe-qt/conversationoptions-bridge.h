/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_CONVERSATIONOPTIONS_BRIDGE_H
#define ZOITECHAT_QT_CONVERSATIONOPTIONS_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
#define QT_CONVERSATION_OPTION_COUNT 8
struct QtConversationOptions { const char *name, *network; int persistent; int values[QT_CONVERSATION_OPTION_COUNT]; };
int qt_core_conversation_options(void *session, struct QtConversationOptions *state);
int qt_core_conversation_options_apply(void *session, const char *name, const char *network, const int *expected, const int *values, char *error, int capacity);
#ifdef __cplusplus
}
#endif
#endif
