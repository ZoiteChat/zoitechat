/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_FRIENDS_TEST_ENGINE_H
#define ZOITECHAT_QT_FRIENDS_TEST_ENGINE_H
#include "friends-bridge.h"
#include <QMap>
#include <QList>
#include <QString>
namespace FriendsTestEngine {
QMap<QString, QString> rules;
struct Row { QString name, network; void *server; int online; time_t seen = 0, on = 0, off = 0; };
QList<Row> rows;
QString queried;
void *queriedServer = nullptr;
int newTab = -1;
void *newTabSource = nullptr;
}
extern "C" {
void qt_core_friends_foreach(QtFriendVisitor visitor, void *data)
{
    for (auto it = FriendsTestEngine::rules.cbegin(); it != FriendsTestEngine::rules.cend(); ++it) {
        bool emitted = false;
        for (const auto &row : FriendsTestEngine::rows) if (row.name == it.key()) {
            visitor(it.key().toUtf8().constData(), it.value().toUtf8().constData(), row.network.toUtf8().constData(),
                row.server, row.online, row.seen, row.on, row.off, data); emitted = true;
        }
        if (!emitted) visitor(it.key().toUtf8().constData(), it.value().toUtf8().constData(), "", nullptr, -1, 0, 0, 0, data);
    }
}
int qt_core_friend_set(const char *oldName, const char *name, const char *networks)
{
    const QString old = QString::fromUtf8(oldName ? oldName : ""), value = QString::fromUtf8(name);
    if (value.isEmpty() || value.contains(QLatin1Char(' ')) || (!old.isEmpty() && !FriendsTestEngine::rules.contains(old)) ||
        (value != old && FriendsTestEngine::rules.contains(value))) return 0;
    FriendsTestEngine::rules.remove(old); FriendsTestEngine::rules[value] = QString::fromUtf8(networks ? networks : "");
    qt_fe_friends_changed(); return 1;
}
int qt_core_friend_remove(const char *name)
{
    const bool removed = FriendsTestEngine::rules.remove(QString::fromUtf8(name)); qt_fe_friends_changed(); return removed;
}
int qt_core_friend_query(const char *name, void *server, const char *network)
{
    for (const auto &row : FriendsTestEngine::rows) if (row.name == QString::fromUtf8(name) && row.server == server &&
        row.network == QString::fromUtf8(network) && row.online == 1 && FriendsTestEngine::rules.contains(row.name)) {
        FriendsTestEngine::queried = row.name; FriendsTestEngine::queriedServer = server; return 1;
    }
    return 0;
}
void qt_core_new_tab(void *session, int serverTab) { FriendsTestEngine::newTab = serverTab; FriendsTestEngine::newTabSource = session; }
}
#endif
