/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "pastepreview.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
PastePreview::PastePreview(const QString &text, const QString &conversation, QWidget *parent, const QString &commandPrefix) : QDialog(parent)
{
    setObjectName(QStringLiteral("pastePreview")); setWindowTitle(QStringLiteral("Paste into ") + conversation); resize(650, 400);
    auto *layout = new QVBoxLayout(this);
    auto *target = new QLabel(QStringLiteral("Send to: ") + conversation, this); target->setTextFormat(Qt::PlainText); layout->addWidget(target);
    text_ = new QPlainTextEdit(this); text_->setObjectName(QStringLiteral("pasteText")); text_->setPlainText(text); layout->addWidget(text_, 1);
    count_ = new QLabel(this); count_->setObjectName(QStringLiteral("pasteCount")); layout->addWidget(count_);
    commands_ = new QCheckBox(commandPrefix.isEmpty() ? QStringLiteral("Use input command syntax") : QStringLiteral("Treat lines starting with %1 as commands").arg(commandPrefix), this);
    commands_->setObjectName(QStringLiteral("pasteCommands")); layout->addWidget(commands_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    send_ = buttons->addButton(QStringLiteral("Send"), QDialogButtonBox::AcceptRole); send_->setObjectName(QStringLiteral("sendPaste"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(text_, &QPlainTextEdit::textChanged, this, [this]() { updateCount(); }); updateCount();
}
QStringList PastePreview::lines() const
{
    QStringList result;
    for (const QString &line : text_->toPlainText().split(QLatin1Char('\n'))) if (!line.isEmpty()) result.append(line);
    return result;
}
bool PastePreview::commandsEnabled() const { return commands_->isChecked(); }
void PastePreview::updateCount()
{
    const int count = lines().size(); const bool valid = count > 0 && count <= 100 && text_->toPlainText().size() <= 65536;
    count_->setText(QStringLiteral("%1 nonempty lines. Up to 100 lines / 64 KiB; blank lines are skipped. Sending is paced.").arg(count));
    send_->setEnabled(valid);
}
