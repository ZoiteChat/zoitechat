/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_PASTEPREVIEW_H
#define ZOITECHAT_QT_PASTEPREVIEW_H
#include <QDialog>
#include <QStringList>
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class PastePreview final : public QDialog {
public:
    PastePreview(const QString &text, const QString &conversation, QWidget *parent, const QString &commandPrefix = QStringLiteral("/"));
    QStringList lines() const;
    bool commandsEnabled() const;
private:
    void updateCount();
    QPlainTextEdit *text_;
    QCheckBox *commands_;
    QLabel *count_;
    QPushButton *send_;
};
#endif
