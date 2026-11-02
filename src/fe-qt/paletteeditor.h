/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_PALETTEEDITOR_H
#define ZOITECHAT_QT_PALETTEEDITOR_H
#include <QString>
#include <functional>
class QWidget;
void showPaletteEditor(const QString &path, QWidget *parent, std::function<void()> applied);
#endif
