/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_CHARACTERCHART_H
#define ZOITECHAT_QT_CHARACTERCHART_H
#include <QString>
#include <functional>
class QWidget;
void showCharacterChart(QWidget *parent, std::function<bool(const QString &)> insert);
#endif
