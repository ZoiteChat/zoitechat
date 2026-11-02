/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_RANKICON_H
#define ZOITECHAT_QT_RANKICON_H
#include <QIcon>
#include <QStringList>
// Match the native frontend's fixed voice/halfop/op icons and three ranks above op.
inline QIcon rankIcon(const QString &prefix, const QString &ranks) {
    QString icon;
    if (prefix == QStringLiteral("+")) icon = QStringLiteral("voice");
    else if (prefix == QStringLiteral("%")) icon = QStringLiteral("halfop");
    else if (prefix == QStringLiteral("@")) icon = QStringLiteral("op");
    else if (!prefix.isEmpty()) {
        const int op = ranks.indexOf(QLatin1Char('@')), rank = ranks.indexOf(prefix);
        const int above = op - rank;
        if (rank >= 0 && op >= 0 && above >= 1 && above <= 3)
            icon = QStringList({QStringLiteral("owner"), QStringLiteral("founder"), QStringLiteral("netop")})[above - 1];
    }
    return icon.isEmpty() ? QIcon() : QIcon(QStringLiteral(":/zoitechat/ulist_%1.png").arg(icon));
}
#endif
