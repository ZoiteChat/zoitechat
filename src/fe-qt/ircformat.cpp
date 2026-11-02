/*
 * Qt-side mIRC formatting: foreground/background colours, true-colour,
 * bold, italic, underline, strikethrough, reverse and monospace.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ircformat.h"

#include <QDateTime>
#include <QFile>
#include <QFont>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QVariant>
#include <QUrl>
#include <cmath>

namespace {
struct Style {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    bool monospace = false;
    bool reverse = false;
    QColor fg;
    int fgSlot = -1;
    QColor bg;
};

QColor configuredColors[42];

double luminance(const QColor &color)
{
    auto linear = [](double value) { return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}
QColor readableColor(const QColor &foreground, const QColor &background, double minimum = 4.5)
{
    const double base = luminance(background);
    auto contrast = [base](const QColor &color) { const double value = luminance(color); return (qMax(base, value) + 0.05) / (qMin(base, value) + 0.05); };
    if (contrast(foreground) >= minimum) return foreground;
    const double target = base < 0.179 ? 1 : 0;
    auto blend = [&](double amount) {
        return QColor::fromRgbF(foreground.redF() * (1 - amount) + target * amount,
                               foreground.greenF() * (1 - amount) + target * amount,
                               foreground.blueF() * (1 - amount) + target * amount);
    };
    double low = 0, high = 1;
    for (int i = 0; i < 20; ++i) {
        const double middle = (low + high) / 2;
        if (contrast(blend(middle)) >= minimum) high = middle; else low = middle;
    }
    return blend(high);
}
QColor readableOnChatBackground(int index, const QColor &color, const QPalette &palette)
{
    if ((index < 32 || index >= 37) && luminance(palette.color(QPalette::Base)) < 0.179)
        return readableColor(color, palette.color(QPalette::Base));
    return color;
}


QColor defaultIrcColor(int number, const QPalette &palette)
{
    static const char *const colors[] = {
        "#FFFFFF", "#000000", "#00007F", "#009300",
        "#FF0000", "#7F0000", "#9C009C", "#FC7F00",
        "#FFFF00", "#00FC00", "#009393", "#00FFFF",
        "#0000FC", "#FF00FF", "#7F7F7F", "#D2D2D2"
    };
    if (number < 0 || number == 99) return QColor();
    if (number >= 42) number %= 32;
    if (number < 32) return QColor(QString::fromLatin1(colors[number % 16]));
    switch (number) {
    case 32: return palette.color(QPalette::HighlightedText);
    case 33: return palette.color(QPalette::Highlight);
    case 34: return palette.color(QPalette::Text);
    case 35: return palette.color(QPalette::Base);
    case 36: return palette.color(QPalette::Highlight);
    case 37: return QColor(115, 135, 115);
    case 38: return QColor(206, 83, 83);
    case 39: return QColor(65, 135, 210);
    case 40: return palette.color(QPalette::Disabled, QPalette::Text);
    default: return QColor(Qt::red);
    }
}

QColor mircColor(int number, const QPalette &palette)
{
    if (number < 0 || number == 99) return QColor();
    if (number >= 42) number %= 32;
    return configuredColors[number].isValid() ? configuredColors[number] : defaultIrcColor(number, palette);
}

QTextCharFormat toFormat(const Style &style, const QPalette &palette)
{
    QTextCharFormat format;
    format.setFontWeight(style.bold ? QFont::Bold : QFont::Normal);
    format.setFontItalic(style.italic);
    format.setFontUnderline(style.underline);
    format.setFontStrikeOut(style.strike);
    format.setFontFixedPitch(style.monospace);
    QColor foreground = style.fg.isValid() ? style.fg : mircColor(34, palette);
    const QColor copyForeground = foreground;
    const int slot = style.fgSlot >= 42 ? style.fgSlot % 32 : style.fgSlot;
    if (!style.bg.isValid() && !style.reverse && slot >= 0 && slot < 42)
        foreground = readableOnChatBackground(slot, foreground, palette);
    const QColor background = style.bg.isValid() ? style.bg : mircColor(35, palette);
    QString prefix;
    if (style.bold) prefix += QChar(0x02);
    if (style.italic) prefix += QChar(0x1d);
    if (style.underline) prefix += QChar(0x1f);
    if (style.strike) prefix += QChar(0x1e);
    if (style.monospace) prefix += QChar(0x11);
    if (style.reverse) prefix += QChar(0x16);
    if (style.fg.isValid() || style.bg.isValid()) {
        prefix += QChar(0x04); prefix += copyForeground.name(QColor::HexRgb).mid(1);
        if (style.bg.isValid()) prefix += QLatin1Char(',') + background.name(QColor::HexRgb).mid(1);
    }
    format.setProperty(IrcCopyPrefixProperty, prefix);
    format.setForeground(style.reverse ? background : foreground);
    if (style.reverse || style.bg.isValid())
        format.setBackground(style.reverse ? foreground : background);
    return format;
}

bool digit(QChar c) { return c.unicode() >= '0' && c.unicode() <= '9'; }

int readDecimal(const QString &text, qsizetype &at)
{
    int value = 0;
    int count = 0;
    while (at < text.size() && count < 2 && digit(text.at(at))) {
        value = value * 10 + text.at(at).unicode() - '0';
        ++at;
        ++count;
    }
    return count ? value : -1;
}

QColor readHex(const QString &text, qsizetype &at)
{
    if (at + 6 > text.size()) return QColor();
    const QString value = text.mid(at, 6);
    for (QChar c : value) {
        const ushort u = c.unicode();
        if (!digit(c) && !(u >= 'a' && u <= 'f') && !(u >= 'A' && u <= 'F'))
            return QColor();
    }
    at += 6;
    return QColor(QLatin1Char('#') + value);
}

void appendLinkedText(QTextCursor &cursor, const IrcRun &run, const QPalette &palette)
{
    // Format URLs as anchors, not HTML, so IRC messages cannot inject markup.
    static const QRegularExpression urlPattern(
        QStringLiteral(R"((?:https?://|ircs?://|www\.)[^\s<>"]+)"),
        QRegularExpression::CaseInsensitiveOption);
    qsizetype pos = 0;
    auto matches = urlPattern.globalMatch(run.text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (match.capturedStart() > pos)
            cursor.insertText(run.text.mid(pos, match.capturedStart() - pos), run.format);
        QString url = match.captured();
        while (!url.isEmpty() && QStringLiteral(".,;:!?)]").contains(url.back()))
            url.chop(1);
        if (url.isEmpty()) continue;
        QTextCharFormat link = run.format;
        link.setAnchor(true);
        link.setAnchorHref(url.startsWith(QStringLiteral("www."), Qt::CaseInsensitive)
                           ? QStringLiteral("https://") + url : url);
        link.setFontUnderline(true);
        link.setForeground(readableColor(palette.color(QPalette::Link), run.format.background().style() == Qt::NoBrush ? mircColor(35, palette) : run.format.background().color()));
        cursor.insertText(url, link);
        const qsizetype trailing = match.capturedLength() - url.size();
        if (trailing)
            cursor.insertText(match.captured().right(trailing), run.format);
        pos = match.capturedEnd();
    }
    if (pos < run.text.size())
        cursor.insertText(run.text.mid(pos), run.format);
}
} // namespace

QColor ircPaletteColor(int index, const QPalette &fallback)
{ return index >= 0 && index < 42 ? readableOnChatBackground(index, mircColor(index, fallback), fallback) : QColor(); }

QColor defaultIrcPaletteColor(int index, const QPalette &fallback)
{ return index >= 0 && index < 42 ? defaultIrcColor(index, fallback) : QColor(); }

QVector<QColor> parseIrcPaletteConfig(const QByteArray &contents)
{
    QVector<QColor> colors(42);
    const QRegularExpression entry(QStringLiteral("^\\s*color_(\\d+)\\s*=\\s*([0-9a-fA-F]{1,4})\\s+([0-9a-fA-F]{1,4})\\s+([0-9a-fA-F]{1,4})\\s*$"));
    for (const auto &line : QString::fromUtf8(contents).split(QLatin1Char('\n'))) {
        const auto match = entry.match(line); if (!match.hasMatch()) continue;
        bool validKey = false;
        const int key = match.captured(1).toInt(&validKey);
        if (!validKey) continue;
        const int index = key >= 0 && key < 32 ? key : key >= 256 && key <= 265 ? key - 224 : -1;
        if (index < 0) continue;
        const auto component = [&](int group) { return int(match.captured(group).toUInt(nullptr, 16) >> 8); };
        colors[index] = QColor(component(2), component(3), component(4));
    }
    return colors;
}

bool loadIrcPalette(const QString &path)
{
    for (auto &color : configuredColors) color = QColor();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return false;
    const auto colors = parseIrcPaletteConfig(file.readAll());
    for (int i = 0; i < colors.size(); ++i) configuredColors[i] = colors[i];
    return true;
}

QPalette ircDisplayPalette(const QPalette &fallback)
{
    QPalette result(fallback);
    const QPalette::ColorRole roles[] = {QPalette::HighlightedText, QPalette::Highlight, QPalette::Text, QPalette::Base};
    for (int i = 0; i < 4; ++i) if (configuredColors[32 + i].isValid()) result.setColor(roles[i], configuredColors[32 + i]);
    return result;
}

static QVector<IrcRun> parseIrc(const QString &text, const QPalette &palette, bool input)
{
    Style style;
    QVector<IrcRun> result;
    QString buffer;
    auto flush = [&]() {
        if (!buffer.isEmpty()) {
            result.push_back({buffer, toFormat(style, palette)});
            buffer.clear();
        }
    };
    for (qsizetype i = 0; i < text.size(); ++i) {
        const qsizetype start = i;
        const ushort c = text.at(i).unicode();
        const bool control = c == 0 || c == 7 || c == 8 || c == '\r' || c == 2 || c == 3 || c == 4 || c == 15 || c == 17 || c == 22 || c == 29 || c == 30 || c == 31;
        if (input && control) flush();
        switch (c) {
        case 0x02: flush(); style.bold = !style.bold; break;
        case 0x1D: flush(); style.italic = !style.italic; break;
        case 0x1F: flush(); style.underline = !style.underline; break;
        case 0x1E: flush(); style.strike = !style.strike; break;
        case 0x11: flush(); style.monospace = !style.monospace; break;
        case 0x16: flush(); style.reverse = !style.reverse; break;
        case 0x0F: flush(); style = Style{}; break;
        case 0x03: {
            flush();
            qsizetype next = i + 1;
            const int fg = readDecimal(text, next);
            style.fg = mircColor(fg, palette);
            style.fgSlot = fg;
            style.bg = QColor();
            if (fg >= 0 && next < text.size() && text.at(next) == QLatin1Char(',')) {
                qsizetype after = next + 1;
                const int bg = readDecimal(text, after);
                if (bg >= 0) {
                    style.bg = mircColor(bg, palette);
                    next = after;
                }
            }
            i = next - 1;
            break;
        }
        case 0x04: {
            flush();
            qsizetype next = i + 1;
            style.fg = readHex(text, next);
            style.fgSlot = -1;
            style.bg = QColor();
            if (style.fg.isValid() && next < text.size() && text.at(next) == QLatin1Char(',')) {
                qsizetype after = next + 1;
                QColor bg = readHex(text, after);
                if (bg.isValid()) {
                    style.bg = bg;
                    next = after;
                }
            }
            i = next - 1;
            break;
        }
        case 0x00: case 0x07: case 0x08:
            // NUL, bell and hidden-text marker are not displayed.
            break;
        case '\r':
            // IRC uses CRLF; QTextDocument uses '\n'.
            break;
        default:
            buffer += text.at(i);
            break;
        }
        if (input && control) {
            QTextCharFormat marker; marker.setForeground(readableColor(palette.color(QPalette::Mid), mircColor(35, palette)));
            result.push_back({text.mid(start, i - start + 1), marker});
        }
    }
    flush();
    return result;
}
QVector<IrcRun> parseIrcText(const QString &text, const QPalette &palette) { return parseIrc(text, palette, false); }
QVector<IrcRun> parseIrcInput(const QString &text, const QPalette &palette) { return parseIrc(text, palette, true); }

QString formatChatTimestamp(std::time_t timestamp, const QString &format)
{
    if (format.size() > 512 || format.contains(QChar(0)) || format.contains(QLatin1Char('\n')) || format.contains(QLatin1Char('\r'))) return {};
    if (timestamp <= 0) timestamp = std::time(nullptr);
    const std::tm *local = std::localtime(&timestamp);
    if (!local) return {};
    const std::tm date = *local;
    char buffer[4096];
    const QByteArray bytes = format.toLocal8Bit();
    const auto length = std::strftime(buffer, sizeof(buffer), bytes.constData(), &date);
    const QString result = QString::fromLocal8Bit(buffer, int(length));
    if (result.contains(QLatin1Char('\n')) || result.contains(QLatin1Char('\r'))) return {};
    return result;
}

namespace {
qreal eventHeaderWidth(const QTextBlock &block)
{
    int timestampEnd = 0;
    QList<QTextLayout::FormatRange> formats;
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const auto fragment = it.fragment(); if (!fragment.isValid()) continue;
        const int start = fragment.position() - block.position();
        if (fragment.charFormat().boolProperty(IrcTimestampProperty)) timestampEnd = start + fragment.length();
        formats.append({start, fragment.length(), fragment.charFormat()});
    }
    const int separator = block.text().indexOf(QLatin1Char('\t'), timestampEnd);
    if (separator < 0) return -1;
    QTextLayout layout(block.text().left(separator), block.document()->defaultFont());
    QList<QTextLayout::FormatRange> headerFormats;
    for (auto format : formats) if (format.start < separator) { format.length = qMin(format.length, separator - format.start); headerFormats.append(format); }
    layout.setFormats(headerFormats); layout.beginLayout(); auto line = layout.createLine();
    if (line.isValid()) line.setLineWidth(10000000);
    layout.endLayout(); return line.isValid() ? line.naturalTextWidth() : 0;
}
void indentBlock(const QTextBlock &block, qreal column, qreal header)
{
    auto format = block.blockFormat();
    format.setLeftMargin(header >= 0 ? column : 0);
    format.setTextIndent(header >= 0 ? -header - 8 : 0);
    if (header >= 0 && column > 0) {
        QTextOption::Tab tab; tab.position = header + 8; tab.type = QTextOption::LeftTab;
        format.setTabPositions({tab});
    } else { format.setLeftMargin(0); format.setTextIndent(0); format.setTabPositions({}); }
    QTextCursor cursor(block); cursor.setBlockFormat(format);
}
}
void setChatIndentation(QTextDocument *document, int maximumWidth)
{
    if (!document) return;
    QList<QPair<QTextBlock, qreal>> blocks; qreal column = 0;
    for (auto block = document->begin(); block.isValid(); block = block.next()) {
        const qreal header = maximumWidth > 0 ? eventHeaderWidth(block) : -1;
        blocks.append({block, header}); if (header >= 0) column = qMax(column, header + 8);
    }
    column = qMin(column, qreal(qMax(0, maximumWidth)));
    for (const auto &block : blocks) indentBlock(block.first, column, block.second);
    document->setProperty("ircIndentColumn", column); document->setProperty("ircIndentMaximum", maximumWidth);
}

void appendIrcMessage(QTextDocument *document, const QString &raw,
                      std::time_t timestamp, const QPalette &palette, bool showTimestamp, const QString &timestampFormat, int maxIndent)
{
    if (!document || raw.isEmpty()) return;
    QString text = raw;
    while (!text.isEmpty() && (text.endsWith(QLatin1Char('\n')) ||
                                text.endsWith(QLatin1Char('\r'))))
        text.chop(1);
    if (text.isEmpty()) return;

    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::End);
    if (!document->isEmpty()) cursor.insertBlock();
    const QTextBlock firstOutputBlock = cursor.block();

    if (showTimestamp) {
        QTextCharFormat timeFormat;
        timeFormat.setProperty(IrcTimestampProperty, true);
        timeFormat.setForeground(readableColor(palette.color(QPalette::Mid), mircColor(35, palette), 7.0));
        QString time = formatChatTimestamp(timestamp, timestampFormat);
        if (time.isEmpty()) time = formatChatTimestamp(timestamp, QStringLiteral("[%H:%M] "));
        cursor.insertText(time, timeFormat);
    }
    for (const IrcRun &run : parseIrcText(text, palette))
        appendLinkedText(cursor, run, palette);
    const qreal column = document->property("ircIndentColumn").toDouble();
    bool resizeColumn = document->property("ircIndentMaximum").toInt() != maxIndent;
    QList<QPair<QTextBlock, qreal>> output;
    for (auto block = firstOutputBlock.isValid() ? firstOutputBlock : document->begin(); block.isValid(); block = block.next()) {
        const qreal header = maxIndent > 0 ? eventHeaderWidth(block) : -1;
        output.append({block, header});
        if (header >= 0 && qMin(header + 8, qreal(maxIndent)) > column) resizeColumn = true;
    }
    if (resizeColumn) setChatIndentation(document, maxIndent);
    else for (const auto &block : output) indentBlock(block.first, column, block.second);
}

QString copyIrcSelection(const QTextCursor &selection, bool timestamps, bool colors)
{
    if (!selection.hasSelection() || !selection.document()) return {};
    const int start = selection.selectionStart(), end = selection.selectionEnd();
    QString result, previous;
    auto reset = [&]() { if (!previous.isEmpty()) result += QChar(0x0f); previous.clear(); };
    for (auto block = selection.document()->findBlock(start); block.isValid() && block.position() < end; block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment(); if (!fragment.isValid()) continue;
            const int left = qMax(start, fragment.position()), right = qMin(end, fragment.position() + fragment.length());
            if (left >= right || (!timestamps && fragment.charFormat().boolProperty(IrcTimestampProperty))) continue;
            const QString prefix = colors ? fragment.charFormat().stringProperty(IrcCopyPrefixProperty) : QString();
            if (prefix != previous) { reset(); result += prefix; previous = prefix; }
            result += fragment.text().mid(left - fragment.position(), right - left);
        }
        const int separator = block.position() + block.length() - 1;
        if (separator >= start && separator < end && block.next().isValid()) { reset(); result += QLatin1Char('\n'); }
    }
    reset(); return result;
}
