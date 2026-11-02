/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_IRCTOOLS_H
#define ZOITECHAT_QT_IRCTOOLS_H
#include <QString>
class QWidget;
void initializeIrcTools(QWidget *parent);
void showRawLog(void *server);
void showIgnoreManager();
QString redactedIrcLine(const QString &line);
#endif
