/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "irclineedit.h"
#include "ircformat.h"
#include <QApplication>
#include <QClipboard>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); bool ok = true;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    IrcLineEdit input; input.resize(500, 44);
    QPalette palette; palette.setColor(QPalette::Base, Qt::white); palette.setColor(QPalette::Text, Qt::black); palette.setColor(QPalette::Mid, Qt::gray);
    input.setPalette(palette); input.setIrcRendering(true); input.show(); input.setFocus(); app.processEvents();
    const QString raw = QChar(4) + QStringLiteral("FF0000") + QChar(2) + QString::fromUtf8("hello café 😀 world") + QChar(15) + QStringLiteral(" plain");
    input.setText(raw);
    QString collected; for (const auto &run : parseIrcInput(raw, palette)) collected += run.text;
    check(collected == raw, "editable parser retains all raw formatting positions and Unicode");
    auto redPixels = [&]() {
        QImage image(input.size(), QImage::Format_ARGB32); image.fill(Qt::white); input.render(&image);
        int red = 0;
        for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
            auto c = image.pixelColor(x, y); if (c.red() > 180 && c.green() < 80 && c.blue() < 80) ++red;
        }
        return red;
    };
    check(input.hasFocus() && redPixels() > 10 && input.text() == raw, "focused input renders true-color text while preserving protocol controls");
    auto point = [&](int position) { input.setCursorPosition(position); return input.inputMethodQuery(Qt::ImCursorRectangle).toRectF().center().toPoint(); };
    const QPoint first = point(8), last = point(13);
    QMouseEvent press(QEvent::MouseButtonPress, first, input.mapToGlobal(first), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&input, &press);
    QMouseEvent move(QEvent::MouseMove, last, input.mapToGlobal(last), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&input, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, last, input.mapToGlobal(last), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&input, &release);
    check(input.selectedText() == QStringLiteral("hello"), "pointer selection follows formatted glyph geometry");
    const QPoint word = point(10);
    QMouseEvent doubleClick(QEvent::MouseButtonDblClick, word, input.mapToGlobal(word), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&input, &doubleClick);
    QMouseEvent wordRelease(QEvent::MouseButtonRelease, word, input.mapToGlobal(word), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&input, &wordRelease);
    check(input.selectedText() == QStringLiteral("hello"), "double-click and release retain a complete formatted word");
    input.selectAll(); input.copy(); check(QApplication::clipboard()->text() == raw, "copy retains raw controls and Unicode");
    input.setCursorPosition(raw.size()); input.insert(QStringLiteral("!")); input.undo(); check(input.text() == raw, "native undo restores formatted source text");
    input.setText(raw.repeated(20) + QString(30, QLatin1Char(' '))); input.setCursorPosition(input.text().size());
    auto caret = input.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    check(caret.left() >= 0 && caret.right() < input.width(), "formatted caret scrolls into view including trailing spaces");
    const int nearEnd = input.cursorPositionAt(caret.center().toPoint());
    check(qAbs(nearEnd - input.text().size()) <= 1, "scrolled pointer hit testing reaches the end of formatted input");
    input.setText(raw); input.setCursorPosition(raw.size());
    QInputMethodEvent preedit(QString::fromUtf8("日本"), {}); QApplication::sendEvent(&input, &preedit);
    check(input.text() == raw, "IME preedit does not enter raw message data");
    QInputMethodEvent commit; commit.setCommitString(QString::fromUtf8("日本")); QApplication::sendEvent(&input, &commit);
    check(input.text() == raw + QString::fromUtf8("日本") && redPixels() > 10, "IME commit preserves controls and restores formatted rendering");
    const QString beforeToggle = input.text(); input.setIrcRendering(false);
    check(input.text() == beforeToggle && redPixels() == 0, "disabling native input attributes restores plain rendering without editing data");
    input.setIrcRendering(true); input.setText(QChar(2) + QString::fromUtf8("العربية 😀"));
    input.setCursorPosition(input.text().size()); caret = input.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    check(caret.left() >= 0 && caret.right() < input.width() && !input.grab().isNull(), "RTL and supplementary characters retain a visible formatted caret");
    return ok ? 0 : 1;
}
