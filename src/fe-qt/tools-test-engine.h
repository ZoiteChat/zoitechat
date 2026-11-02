/* SPDX-License-Identifier: GPL-2.0-or-later */
// Shared fake adapter for frontend and tool tests; no real configuration is used.
#ifndef ZOITECHAT_QT_TOOLS_TEST_ENGINE_H
#define ZOITECHAT_QT_TOOLS_TEST_ENGINE_H
#include "tools-bridge.h"
#include <QHash>
#include <QMap>
#include <QString>
namespace ToolTestEngine {
QHash<void *, bool> servers;
QHash<void *, void *> sessionServers;
void *requestedServer = nullptr, *joinedServer = nullptr;
QString requestedFilter, joinedChannel;
int requests = 0;
QMap<QString, int> ignores;
}
extern "C" {
void qt_core_urls_clear(void) {}
void *qt_core_session_server(void *session) { return ToolTestEngine::sessionServers.value(session, session); }
int qt_core_server_info(void *server, const char **name, int *connected)
{
    if (!ToolTestEngine::servers.contains(server)) return 0;
    *name = "irc.test"; *connected = ToolTestEngine::servers.value(server); return 1;
}
int qt_core_request_channels(void *server, const char *filter)
{
    if (!ToolTestEngine::servers.value(server)) return 0;
    ToolTestEngine::requestedServer = server;
    ToolTestEngine::requestedFilter = QString::fromUtf8(filter);
    ++ToolTestEngine::requests;
    return 1;
}
int qt_core_join_channel(void *server, const char *channel)
{
    if (!ToolTestEngine::servers.value(server)) return 0;
    ToolTestEngine::joinedServer = server; ToolTestEngine::joinedChannel = QString::fromUtf8(channel); return 1;
}
void qt_core_ignore_foreach(void (*visitor)(const char *, int, void *), void *data)
{
    for (auto it = ToolTestEngine::ignores.cbegin(); it != ToolTestEngine::ignores.cend(); ++it)
        visitor(it.key().toUtf8().constData(), it.value(), data);
}
int qt_core_ignore_set(const char *oldMask, const char *mask, int flags)
{
    const QString old = QString::fromUtf8(oldMask), value = QString::fromUtf8(mask);
    if (value.isEmpty() || value.contains(QLatin1Char(' ')) || !(flags & 159) ||
        (!old.isEmpty() && !ToolTestEngine::ignores.contains(old)) ||
        (value != old && ToolTestEngine::ignores.contains(value))) return 0;
    ToolTestEngine::ignores.remove(old); ToolTestEngine::ignores.insert(value, flags); qt_fe_ignore_changed(); return 1;
}
int qt_core_ignore_remove(const char *mask)
{
    const bool removed = ToolTestEngine::ignores.remove(QString::fromUtf8(mask));
    qt_fe_ignore_changed(); return removed;
}
}
#endif
