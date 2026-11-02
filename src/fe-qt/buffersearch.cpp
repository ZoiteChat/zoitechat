/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "buffersearch.h"
#include "ircformat.h"
#include <QPalette>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
BufferSearchResult filterChatBuffer(const QTextDocument *source, QTextDocument *output,
                                   const QString &query, int flags, const QPalette &palette)
{
    if (!source || !output || source == output) return {0, QStringLiteral("Choose a source conversation to search.")};
    if (query.isEmpty()) return {0, QStringLiteral("Enter a search string.")};
    auto options = QRegularExpression::UseUnicodePropertiesOption;
    QRegularExpression::PatternOptions patternOptions(options);
    if (!(flags & BufferMatchCase)) patternOptions |= QRegularExpression::CaseInsensitiveOption;
    QRegularExpression expression(flags & BufferRegexp ? query : QRegularExpression::escape(query), patternOptions);
    if (!expression.isValid()) return {0, QStringLiteral("Invalid regular expression: %1").arg(expression.errorString())};
    output->clear();
    if (source->isEmpty()) return {0, QStringLiteral("Search buffer is empty.")};
    QTextCursor destination(output); BufferSearchResult result;
    for (auto block = source->begin(); block.isValid(); block = block.next()) {
        int prefix = 0;
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
            if (!fragment.fragment().charFormat().property(IrcTimestampProperty).toBool()) break;
            prefix += fragment.fragment().length();
        }
        const QString text = block.text().mid(prefix);
        auto matches = expression.globalMatch(text);
        if (!matches.hasNext()) continue;
        if (result.lines++) destination.insertBlock();
        QTextCursor from(const_cast<QTextDocument *>(source));
        from.setPosition(block.position()); from.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        const int start = destination.position();
        destination.insertFragment(QTextDocumentFragment(from));
        if (flags & BufferHighlight) while (matches.hasNext()) {
            const auto match = matches.next();
            if (!match.capturedLength()) continue;
            QTextCursor highlight(output); highlight.setPosition(start + prefix + match.capturedStart());
            highlight.setPosition(start + prefix + match.capturedEnd(), QTextCursor::KeepAnchor);
            QTextCharFormat format; format.setBackground(palette.color(QPalette::Highlight)); format.setForeground(palette.color(QPalette::HighlightedText));
            highlight.mergeCharFormat(format);
        }
    }
    return result;
}
