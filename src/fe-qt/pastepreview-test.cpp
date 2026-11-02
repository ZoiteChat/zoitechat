/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "pastepreview.h"
#include <QApplication>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); PastePreview dialog(QStringLiteral("first\n/quit\n\nlast"), QStringLiteral("#test"), nullptr);
    bool ok = true;
    auto verify = [&](bool condition, const char *message) { if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    verify(dialog.lines() == QStringList({QStringLiteral("first"), QStringLiteral("/quit"), QStringLiteral("last")}) &&
           !dialog.commandsEnabled(), "preview preserves slash text, skips blanks and defaults to literal messages");
    dialog.findChild<QCheckBox *>(QStringLiteral("pasteCommands"))->setChecked(true);
    verify(dialog.commandsEnabled(), "command interpretation requires explicit opt-in");
    auto *text = dialog.findChild<QPlainTextEdit *>(QStringLiteral("pasteText")); auto *send = dialog.findChild<QPushButton *>(QStringLiteral("sendPaste"));
    text->setPlainText(QString()); verify(!send->isEnabled(), "empty paste cannot send");
    text->setPlainText(QStringLiteral("a\n").repeated(101)); verify(!send->isEnabled(), "more than 100 lines cannot send");
    text->setPlainText(QString(65537, QLatin1Char('x'))); verify(!send->isEnabled(), "oversized paste cannot send");
    text->setPlainText(QStringLiteral("edited café")); verify(send->isEnabled() && dialog.lines().first() == QStringLiteral("edited café"), "preview remains editable with Unicode");
    return ok ? 0 : 1;
}
