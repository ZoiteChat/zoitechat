/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatinput.h"
#include <QApplication>
#include <QKeyEvent>
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    ChatInput input;
    InputHistory first, second;
    bool ok = true;
    auto verify = [&](bool condition, const char *message) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; }
    };
    auto press = [&](int key) {
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, key == Qt::Key_Space ? QStringLiteral(" ") : QString());
        QApplication::sendEvent(&input, &event);
    };
    first.record(QStringLiteral("hello"));
    first.record(QStringLiteral("world"));
    first.record(QStringLiteral("world"));
    input.setConversation(&first, {QStringLiteral("Alice"), QStringLiteral("Alfred")});
    input.setText(QStringLiteral("unfinished"));
    press(Qt::Key_Up);
    verify(input.text() == QStringLiteral("world"), "latest history entry");
    press(Qt::Key_Up);
    press(Qt::Key_Up);
    verify(input.text() == QStringLiteral("hello"), "history start and adjacent deduplication");
    press(Qt::Key_Down);
    press(Qt::Key_Down);
    verify(input.text() == QStringLiteral("unfinished"), "restore unsent draft");
    input.setText(QStringLiteral("edited draft"));
    press(Qt::Key_Down);
    verify(input.text() == QStringLiteral("edited draft"), "down at newest preserves edits");
    input.setConversation(&second, {});
    input.setText(QStringLiteral("other draft"));
    press(Qt::Key_Up);
    verify(input.text() == QStringLiteral("other draft"), "independent empty history");
    input.setConversation(&first, {QStringLiteral("Alice"), QStringLiteral("Alfred")});
    press(Qt::Key_Up);
    verify(input.text() == QStringLiteral("world"), "history survives conversation switch");
    input.setText(QStringLiteral("al"));
    press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("Alfred: "), "case insensitive first-word completion");
    press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("Alice: "), "cycle matching nicknames");
    press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("Alfred: "), "completion wraps");
    input.setText(QStringLiteral("al")); input.completeNick(true);
    verify(input.text() == QStringLiteral("Alice: "), "reverse completion starts from final match");
    input.completeNick(true); verify(input.text() == QStringLiteral("Alfred: "), "reverse completion cycles backward");
    input.setText(QStringLiteral("hello ali please"));
    input.setCursorPosition(9);
    press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("hello Alice please"), "complete at cursor preserving suffix");
    input.setNicks({});
    input.setText(QStringLiteral("ali"));
    press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("ali"), "departed nick is not completed");
    for (int i = 0; i < 150; ++i) second.record(QString::number(i));
    verify(second.lines.size() == 100 && second.lines.first() == QStringLiteral("50"),
           "bounded history");
    QString preview;
    input.multilinePaste = [&](const QString &text) { preview = text; };
    input.setText(QStringLiteral("before replace after")); input.setSelection(7, 7);
    input.pasteText(QStringLiteral("one\r\ntwo\rthree"));
    verify(preview == QStringLiteral("before one\ntwo\nthree after") && input.text() == QStringLiteral("before replace after"),
           "multiline paste normalizes newlines, replaces selection in preview and leaves draft unchanged");
    input.setText(QStringLiteral("prefix ")); input.pasteText(QStringLiteral("single"));
    verify(input.text() == QStringLiteral("prefix single"), "single-line paste inserts directly");
    input.setText(QStringLiteral("bold")); input.selectAll();
    QKeyEvent bold(QEvent::KeyPress, Qt::Key_B, Qt::ControlModifier); QApplication::sendEvent(&input, &bold);
    verify(input.text() == QChar(0x02) + QStringLiteral("bold") + QChar(0x02) && input.selectedText() == QStringLiteral("bold"),
           "IRC formatting shortcut wraps selected text");
    input.setText(QStringLiteral("x"));
    QKeyEvent reset(QEvent::KeyPress, Qt::Key_O, Qt::ControlModifier); QApplication::sendEvent(&input, &reset);
    verify(input.text() == QStringLiteral("x") + QChar(0x0f), "reset formatting shortcut inserts IRC reset");
    input.wordReplacement = [](const QString &word) { return word == QStringLiteral("teh") ? QStringLiteral("the") : word; };
    input.setText(QStringLiteral("teh")); press(Qt::Key_Space);
    verify(input.text() == QStringLiteral("the "), "Auto Replace applies before a typed space");
    input.setText(QStringLiteral("teh")); press(Qt::Key_Return);
    verify(input.text() == QStringLiteral("the"), "Auto Replace applies before Enter submission");
    input.setText(QStringLiteral("/echo teh")); press(Qt::Key_Space);
    verify(input.text() == QStringLiteral("/echo teh "), "Auto Replace does not rewrite command input");
    input.setText(QStringLiteral("Teh")); press(Qt::Key_Space);
    verify(input.text() == QStringLiteral("Teh "), "replacement matching remains case-sensitive");
    input.setCompletionOptions(true, QStringLiteral(","));
    input.setConversation(&first, {QStringLiteral("Alice")}); input.setText(QStringLiteral("Al")); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("Alice, "), "custom completion suffix applies at start of input");
    input.setText(QStringLiteral("hello Al")); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("hello Alice"), "completion suffix is limited to initial nickname");
    input.setCompletionOptions(false, QStringLiteral(",")); input.setText(QStringLiteral("Al")); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("Al"), "nickname completion can be disabled");
    input.setAutomaticReplacement(false); input.setText(QStringLiteral("teh")); press(Qt::Key_Return);
    verify(input.text() == QStringLiteral("teh"), "automatic replacement can be disabled");
    input.setCompletionOptions(true, QStringLiteral(":"));
    input.completionWords = [](bool commands) { return commands ? QStringList({QStringLiteral("JOIN"), QStringLiteral("join"), QStringLiteral("JUMP")}) : QStringList({QStringLiteral("#general"), QStringLiteral("#games")}); };
    input.setText(QStringLiteral("/j")); press(Qt::Key_Tab); verify(input.text() == QStringLiteral("/JOIN "), "command completion appends a space instead of nick suffix");
    press(Qt::Key_Tab); verify(input.text() == QStringLiteral("/JUMP "), "command completion deduplicates case-insensitively and cycles");
    QKeyEvent reverse(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier); QApplication::sendEvent(&input, &reverse);
    verify(input.text() == QStringLiteral("/JOIN "), "default reverse completion cycles commands backwards");
    input.setText(QStringLiteral("/join #ge")); press(Qt::Key_Tab); verify(input.text() == QStringLiteral("/join #general "), "channel completion works inside command arguments");
    input.setAutomaticReplacement(true); input.wordReplacement = [](const QString &word) { return word == QStringLiteral("a") ? QStringLiteral("b") : word == QStringLiteral("b") ? QStringLiteral("c") : word; };
    input.setText(QStringLiteral("a")); input.checkReplacement(); press(Qt::Key_Space);
    verify(input.text() == QStringLiteral("b "), "replacement binding does not replace twice before default space action");
    input.orderedNicks = []() { return QStringList({QStringLiteral("Alphonse"), QStringLiteral("Alice"), QStringLiteral("Alfred")}); };
    input.setCompletionThreshold(3); input.setText(QStringLiteral("Al")); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("Alphonse: "), "ordered nickname provider controls completion order");
    press(Qt::Key_Tab); verify(input.text() == QStringLiteral("Alice: "), "ordered matches cycle in provider order");
    input.setText(QStringLiteral("hello Al")); press(Qt::Key_Tab); const QString completedLine = input.text(); input.setCursorPosition(2); press(Qt::Key_Tab);
    verify(input.text() == completedLine && input.cursorPosition() == 2, "moving cursor invalidates an old completion cycle");
    QStringList listed; input.completionMatches = [&](const QStringList &matches) { listed = matches; };
    input.setCompletionThreshold(2); input.setText(QStringLiteral("A")); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("Al") && listed.size() == 3 && listed.first() == QStringLiteral("Alphonse"), "over threshold extends common prefix and lists ordered matches");
    listed.clear(); press(Qt::Key_Tab); verify(input.text() == QStringLiteral("Al") && listed.size() == 3, "repeated ambiguous completion lists again without selecting an arbitrary nick");
    input.setCompletionThreshold(0); listed.clear(); input.setText(QStringLiteral("Alic")); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("Alice: ") && listed.isEmpty(), "unique nickname completes even in always-list mode");
    input.setText(QStringLiteral("hello A tail")); input.setCursorPosition(7); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("hello Al tail") && input.cursorPosition() == 8, "common-prefix completion preserves text after cursor");
    input.completionEquivalent = [](QString left, QString right) { left.replace(QLatin1Char('{'), QLatin1Char('[')); right.replace(QLatin1Char('{'), QLatin1Char('[')); return left.compare(right, Qt::CaseInsensitive) == 0; };
    input.orderedNicks = []() { return QStringList({QStringLiteral("[Alice")}); };
    input.setText(QStringLiteral("{al")); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("[Alice: "), "completion honors server-specific nickname case mapping");
    input.setCommandPrefix(QStringLiteral("!")); input.setCompletionThreshold(10); input.setText(QStringLiteral("!j")); press(Qt::Key_Tab);
    verify(input.text() == QStringLiteral("!JOIN "), "command completion uses configured input prefix");
    input.wordReplacement = [](const QString &word) { return word == QStringLiteral("teh") ? QStringLiteral("the") : word; };
    input.setText(QStringLiteral("!echo teh")); press(Qt::Key_Space);
    verify(input.text() == QStringLiteral("!echo teh "), "replacement leaves configured command input unchanged");
    input.setText(QStringLiteral("/echo teh")); press(Qt::Key_Space);
    verify(input.text() == QStringLiteral("/echo the "), "old slash is ordinary text after changing prefix");
    return ok ? 0 : 1;
}
