/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_TRANSFERS_H
#define ZOITECHAT_QT_TRANSFERS_H
#include <functional>
class QWidget;
void initializeTransfers(QWidget *parent, std::function<void()> send, std::function<void()> chat = {});
void showTransfers();
void showDirectChats();
#endif
