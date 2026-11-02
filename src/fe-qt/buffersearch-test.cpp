/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "buffersearch.h"
#include "ircformat.h"
#include <QApplication>
#include <QPalette>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); const QPalette palette = app.palette(); bool ok = true;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    QTextDocument source, output;
    appendIrcMessage(&source, QStringLiteral("Alice: \00304red needle\017 https://example.test"), 1, palette);
    appendIrcMessage(&source, QStringLiteral("Bob: NEEDLE twice needle"), 1, palette);
    appendIrcMessage(&source, QStringLiteral("unrelated 😀"), 1, palette);
    const auto original = source.toHtml();
    auto result = filterChatBuffer(&source, &output, QStringLiteral("needle"), 0, palette);
    check(result.lines == 2 && output.toPlainText().contains(QStringLiteral("NEEDLE")) && !output.toPlainText().contains(QStringLiteral("unrelated")), "literal search copies matching lines case-insensitively");
    bool red = false, link = false;
    for (auto block = output.begin(); block.isValid(); block = block.next()) for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
        const auto format = fragment.fragment().charFormat(); red |= format.foreground().color() == QColor(255, 0, 0); link |= format.isAnchor();
    }
    check(red && link && source.toHtml() == original, "results preserve colors and links without modifying source");
    result = filterChatBuffer(&source, &output, QStringLiteral("NEEDLE"), BufferMatchCase | BufferHighlight, palette);
    check(result.lines == 1, "case-sensitive search");
    bool highlighted = false;
    for (auto it = output.begin().begin(); !it.atEnd(); ++it) highlighted |= it.fragment().charFormat().background().color() == palette.color(QPalette::Highlight);
    check(highlighted, "matching spans are highlighted");
    result = filterChatBuffer(&source, &output, QStringLiteral("Alice|😀"), BufferRegexp, palette);
    check(result.lines == 2, "Unicode regular-expression search");
    result = filterChatBuffer(&source, &output, QStringLiteral("^"), BufferRegexp | BufferHighlight, palette);
    check(result.lines == 3, "zero-length regex matches terminate");
    const QString stamp = source.begin().text().left(7);
    result = filterChatBuffer(&source, &output, stamp, BufferMatchCase, palette);
    check(result.lines == 0, "generated timestamps are excluded from matching");
    result = filterChatBuffer(&source, &output, QStringLiteral("["), BufferRegexp, palette);
    check(!result.error.isEmpty(), "invalid regular expression reported");
    check(!filterChatBuffer(&source, &source, QStringLiteral("needle"), 0, palette).error.isEmpty(), "self search cannot destroy source");
    QTextDocument empty;
    check(!filterChatBuffer(&empty, &output, QStringLiteral("x"), 0, palette).error.isEmpty(), "empty source reported");
    output.setMaximumBlockCount(2);
    result = filterChatBuffer(&source, &output, QStringLiteral("."), BufferRegexp | BufferHighlight, palette);
    check(result.lines == 3 && output.blockCount() <= 2, "results respect destination scrollback bound");
    return ok ? 0 : 1;
}
