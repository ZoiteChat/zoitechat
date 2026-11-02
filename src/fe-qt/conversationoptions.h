/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_CONVERSATIONOPTIONS_H
#define ZOITECHAT_QT_CONVERSATIONOPTIONS_H
#include <functional>
class QWidget;
void showConversationOptions(void *session, QWidget *parent, std::function<bool()> alive, std::function<void()> applied);
#endif
