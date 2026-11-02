/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_MODERATION_TEST_ENGINE_H
#define ZOITECHAT_QT_MODERATION_TEST_ENGINE_H
#include "moderation-bridge.h"
#include <QHash>
#include <QList>
#include <QString>
namespace ModerationTestEngine {
QHash<void *, QtChannelState> channels;
struct Request { void *session; QString channel; char mode; bool enable; QString value; };
QList<Request> changes;
QList<Request> lists;
int refreshes = 0;
}
extern "C" {
int qt_core_channel_state(void *session, QtChannelState *state)
{
    if (!ModerationTestEngine::channels.contains(session)) return 0;
    *state = ModerationTestEngine::channels.value(session); return 1;
}
int qt_core_channel_refresh(void *session, const char *channel)
{
    QtChannelState state{};
    if (!qt_core_channel_state(session, &state) || QString::fromUtf8(state.channel) != QString::fromUtf8(channel)) return 0;
    ++ModerationTestEngine::refreshes; return 1;
}
int qt_core_channel_mode(void *session, const char *channel, char mode, int enabled, const char *value)
{
    QtChannelState state{};
    if (!qt_core_channel_state(session, &state) || QString::fromUtf8(state.channel) != QString::fromUtf8(channel) ||
        QString::fromUtf8(value).contains(QLatin1Char(' '))) return 0;
    ModerationTestEngine::changes.append({session, QString::fromUtf8(channel), mode, bool(enabled), QString::fromUtf8(value)}); return 1;
}
int qt_core_ban_request(void *session, const char *channel, char mode)
{
    QtChannelState state{};
    if (!qt_core_channel_state(session, &state)) return 0;
    ModerationTestEngine::lists.append({session, QString::fromUtf8(channel), mode, true, QString()}); return 1;
}
}
#endif
