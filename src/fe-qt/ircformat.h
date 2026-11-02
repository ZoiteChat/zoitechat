/*
 * IRC formatting support for the ZoiteChat Qt frontend.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ZOITECHAT_QT_IRCFORMAT_H
#define ZOITECHAT_QT_IRCFORMAT_H

#include <QPalette>
#include <QString>
#include <QTextCharFormat>
#include <QVector>
#include <ctime>

constexpr int IrcTimestampProperty = QTextFormat::UserProperty + 1;
class QTextDocument;
class QTextCursor;
constexpr int IrcCopyPrefixProperty = QTextFormat::UserProperty + 2;
QString copyIrcSelection(const QTextCursor &selection, bool timestamps, bool colors);

struct IrcRun {
    QString text;
    QTextCharFormat format;
};

// Read the original colors.conf. Missing/invalid entries keep Qt defaults.
QVector<QColor> parseIrcPaletteConfig(const QByteArray &contents);
QColor ircPaletteColor(int index, const QPalette &fallback);
QColor defaultIrcPaletteColor(int index, const QPalette &fallback);
bool loadIrcPalette(const QString &path);
QPalette ircDisplayPalette(const QPalette &fallback);

/* Parse mIRC formatting controls. Text is never treated as HTML. */
QVector<IrcRun> parseIrcText(const QString &text, const QPalette &palette);
// Preserve every raw UTF-16 position for editable input layouts.
QVector<IrcRun> parseIrcInput(const QString &text, const QPalette &palette);

/* Append to a per-session QTextDocument, with clickable links and local time. */
void appendIrcMessage(QTextDocument *document, const QString &raw,
                      std::time_t timestamp, const QPalette &palette, bool showTimestamp = true, const QString &timestampFormat = QStringLiteral("[%H:%M] "), int maxIndent = 0);
void setChatIndentation(QTextDocument *document, int maximumWidth);
QString formatChatTimestamp(std::time_t timestamp, const QString &format);

#endif
