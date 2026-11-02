/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_ADDONS_H
#define ZOITECHAT_QT_ADDONS_H
#include <functional>
class QWidget;
struct AddonContext { void *session = nullptr; std::function<bool()> alive; std::function<void()> restore; };
void showAddons(QWidget *parent, std::function<AddonContext()> context, bool chooseFile = false);
#endif
