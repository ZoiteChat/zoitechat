/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_TOPICENTRY_H
#define ZOITECHAT_QT_TOPICENTRY_H
#include "irclineedit.h"
class TopicEntry final : public IrcLineEdit {
public:
    explicit TopicEntry(QWidget *parent = nullptr) : IrcLineEdit(parent) { setIrcRendering(true); }
};
#endif
