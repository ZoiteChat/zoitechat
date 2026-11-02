/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_MENUMARKUP_H
#define ZOITECHAT_QT_MENUMARKUP_H
#include <QImage>
#include <QString>
class QAction;
class QWidget;
class QFont;
class QColor;
QString plainMenuMarkup(const QString &markup, bool *valid = nullptr);
QImage renderMenuMarkup(const QString &markup, const QFont &font, const QColor &color,
                        qreal dpi, qreal deviceRatio, bool mnemonics);
void setMenuMarkup(QAction *action, const QString &markup, QWidget *container);
#endif
