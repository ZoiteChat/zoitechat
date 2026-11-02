/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_URLHISTORY_H
#define ZOITECHAT_QT_URLHISTORY_H
#include <QUrl>
#include <functional>
class QMenu;
class QWidget;
void initializeUrlHistory(QWidget *parent);
void showUrlHistory();
void setUrlPopupDecorator(std::function<void(QMenu *, const QString &)> decorator);
QUrl safeHistoryUrl(const QString &text);
extern "C" void qt_fe_url_capture(const char *url);
#endif
