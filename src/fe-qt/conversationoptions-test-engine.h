/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_CONVERSATIONOPTIONS_TEST_ENGINE_H
#define ZOITECHAT_QT_CONVERSATIONOPTIONS_TEST_ENGINE_H
#include "conversationoptions-bridge.h"
#include <QHash>
#include <cstring>
namespace ConversationOptionsTestEngine { QHash<void *, QtConversationOptions> states; int applied = 0; }
extern "C" {
int qt_core_conversation_options(void *session, QtConversationOptions *state) {
    if (!ConversationOptionsTestEngine::states.contains(session)) return 0;
    *state = ConversationOptionsTestEngine::states.value(session); return 1;
}
int qt_core_conversation_options_apply(void *session, const char *name, const char *network, const int *expected, const int *values, char *error, int capacity) {
    QtConversationOptions state{};
    if (!qt_core_conversation_options(session, &state) || std::strcmp(name, state.name) || std::strcmp(network, state.network)) return 0;
    for (int i = 0; i < QT_CONVERSATION_OPTION_COUNT; ++i) if (expected[i] != state.values[i]) { std::strncpy(error, "Options changed. Reload.", capacity); return 0; }
    for (int i = 0; i < QT_CONVERSATION_OPTION_COUNT; ++i) ConversationOptionsTestEngine::states[session].values[i] = values[i];
    ++ConversationOptionsTestEngine::applied; return 1;
}
}
#endif
