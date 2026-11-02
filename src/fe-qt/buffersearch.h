/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_BUFFERSEARCH_H
#define ZOITECHAT_QT_BUFFERSEARCH_H
#include <QString>
class QTextDocument;
class QPalette;
// Values match the original engine's gtk_xtext_search_flags.
enum BufferSearchFlag { BufferMatchCase = 1, BufferHighlight = 4, BufferRegexp = 16 };
struct BufferSearchResult { int lines = 0; QString error; };
BufferSearchResult filterChatBuffer(const QTextDocument *source, QTextDocument *output,
                                   const QString &query, int flags, const QPalette &palette);
#endif
