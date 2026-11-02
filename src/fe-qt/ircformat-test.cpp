/*
 * Simple headless Qt/IRC formatting regressions (no external IRC needed).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ircformat.h"
#include <QApplication>
#include <QDateTime>
#include <QPalette>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFragment>
#include <QTextLayout>
#include <cmath>
#include <cstdio>
#include <QTemporaryDir>
#include <QFile>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QPalette palette;
    auto verify = [](bool ok, const char *what) {
        if (!ok) std::fprintf(stderr, "FAIL: %s\n", what);
        return ok;
    };
    bool ok = true;
    const QString boldText = QStringLiteral("a") + QChar(0x02) +
                             QStringLiteral("b") + QChar(0x02) + QStringLiteral("c");
    const auto bold = parseIrcText(boldText, palette);
    ok &= verify(bold.size() == 3, "bold boundaries");
    if (bold.size() == 3) {
        ok &= verify(bold[0].text == QStringLiteral("a") &&
                     bold[1].text == QStringLiteral("b") &&
                     bold[2].text == QStringLiteral("c"), "visible content");
        ok &= verify(bold[1].format.fontWeight() == QFont::Bold, "bold toggle");
        ok &= verify(bold[2].format.fontWeight() == QFont::Normal, "bold reset");
    }
    const QString colorText = QChar(0x03) + QStringLiteral("04,01RED") +
                              QChar(0x0F) + QStringLiteral("default");
    const auto colored = parseIrcText(colorText, palette);
    ok &= verify(colored.size() == 2, "mIRC color boundaries");
    if (colored.size() == 2) {
        ok &= verify(colored[0].format.foreground().color() == QColor("#FF0000"),
                     "foreground color 04");
        ok &= verify(colored[0].format.background().color() == QColor("#000000"),
                     "background color 01");
        ok &= verify(colored[1].format.foreground().color() == palette.color(QPalette::Text),
                     "reset color");
    }
    const auto hex = parseIrcText(QChar(0x04) + QStringLiteral("12ABEFtest"), palette);
    ok &= verify(hex.size() == 1 && hex[0].text == QStringLiteral("test") &&
                 hex[0].format.foreground().color() == QColor("#12ABEF"), "hex color");
    const auto bad = parseIrcText(QStringLiteral("<script>alert(1)</script>"), palette);
    ok &= verify(bad.size() == 1 && bad[0].text == QStringLiteral("<script>alert(1)</script>"),
                 "raw text kept literally");
    QTextDocument document;
    document.setMaximumBlockCount(10);
    appendIrcMessage(&document, QStringLiteral("go to https://example.com/test."), 0, palette, false);
    ok &= verify(document.toPlainText() == QStringLiteral("go to https://example.com/test."),
                 "text roundtrip");
    bool foundLink = false;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (auto frag = block.begin(); !frag.atEnd(); ++frag) {
            const QTextFragment part = frag.fragment();
            if (part.isValid() && part.charFormat().isAnchor() &&
                part.charFormat().anchorHref() == QStringLiteral("https://example.com/test"))
                foundLink = true;
        }
    }
    ok &= verify(foundLink, "clickable link without trailing period");
    appendIrcMessage(&document, QStringLiteral("second line\n"), 0, palette);
    ok &= verify(document.blockCount() == 2 &&
                 document.toPlainText().endsWith(QStringLiteral("second line")),
                 "newline handling");
    QTextDocument live;
    const QString before = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    appendIrcMessage(&live, QStringLiteral("live message"), 0, palette, true);
    const QString after = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    ok &= verify(live.toPlainText() == QStringLiteral("[%1] live message").arg(before) ||
                 live.toPlainText() == QStringLiteral("[%1] live message").arg(after),
                 "zero timestamp uses current local time");
    QTextDocument historical;
    appendIrcMessage(&historical, QStringLiteral("historical message"), 12345, palette, true);
    const QString expectedTime = QDateTime::fromSecsSinceEpoch(12345).toLocalTime().toString(QStringLiteral("HH:mm"));
    ok &= verify(historical.toPlainText() == QStringLiteral("[%1] historical message").arg(expectedTime),
                 "nonzero timestamp preserves original message time");
    QTextDocument disabled;
    appendIrcMessage(&disabled, QStringLiteral("live message"), 0, palette, false);
    ok &= verify(disabled.toPlainText() == QStringLiteral("live message"), "timestamps can be disabled for live messages");
    const auto extended = parseIrcText(QChar(3) + QStringLiteral("18event"), palette);
    ok &= verify(extended.first().format.foreground().color() == QColor("#00007F"), "core event color slots 16–31 repeat base defaults");
    const auto wrappedColor = parseIrcText(QChar(3) + QStringLiteral("42wrapped"), palette);
    const auto baseColor = parseIrcText(QChar(3) + QStringLiteral("10base"), palette);
    ok &= verify(wrappedColor.first().format.foreground() == baseColor.first().format.foreground(), "out-of-range colors wrap like native frontend");
    QTemporaryDir directory;
    const QString paletteFile = directory.filePath(QStringLiteral("colors.conf"));
    QFile config(paletteFile); ok &= verify(config.open(QIODevice::WriteOnly), "palette fixture opens");
    config.write("color_18 = 1234 abcd ffff\ncolor_0 = invalid\ncolor_258 = 2222 3333 4444\ncolor_259 = aaaa bbbb cccc\ncolor_256 = ffff eeee dddd\ncolor_257 = 1111 2222 3333\ncolor_99999999999999999999999 = 0000 0000 0000\n"); config.close();
    ok &= verify(loadIrcPalette(paletteFile), "native colors.conf loads");
    const auto configured = parseIrcText(QChar(3) + QStringLiteral("18configured") + QChar(15) + QStringLiteral("default"), palette);
    ok &= verify(configured[0].format.foreground().color() == QColor("#12ABFF") && configured[1].format.foreground().color() == QColor("#223344"), "16-bit palette channels and default text color map correctly");
    const auto malformedFallback = parseIrcText(QChar(3) + QStringLiteral("00white"), palette);
    ok &= verify(malformedFallback.first().format.foreground().color() == QColor("#FFFFFF"), "malformed entries and oversized indices are ignored");
    const auto displayPalette = ircDisplayPalette(palette);
    ok &= verify(displayPalette.color(QPalette::Base) == QColor("#AABBCC") && displayPalette.color(QPalette::Highlight) == QColor("#112233"), "chat background and selection colors load");
    const auto defaults = parseIrcText(QChar(3) + QStringLiteral("99,99reset"), displayPalette);
    ok &= verify(defaults[0].format.foreground().color() == displayPalette.color(QPalette::Text) && defaults[0].format.background().style() == Qt::NoBrush, "color 99 restores current display defaults");
    ok &= verify(!loadIrcPalette(directory.filePath(QStringLiteral("missing"))), "missing palette uses defaults");
    const auto restored = parseIrcText(QChar(3) + QStringLiteral("00white"), palette);
    ok &= verify(restored.first().format.foreground().color() == QColor("#FFFFFF"), "malformed and missing entries cannot override defaults");
    QTextDocument customTimestamp;
    appendIrcMessage(&customTimestamp, QStringLiteral("custom clock"), 1700000000, palette, true, QStringLiteral("year=%Y "));
    ok &= verify(customTimestamp.toPlainText() == QStringLiteral("year=2023 custom clock"), "custom strftime timestamp format is rendered");
    ok &= verify(formatChatTimestamp(1700000000, QStringLiteral("%n")).isEmpty(), "timestamp format cannot inject new lines");
    ok &= verify(formatChatTimestamp(1700000000, QString(513, QLatin1Char('x'))).isEmpty(), "timestamp format is bounded");
    QTextDocument fallbackTimestamp;
    appendIrcMessage(&fallbackTimestamp, QStringLiteral("fallback"), 1700000000, palette, true, QStringLiteral("%n"));
    ok &= verify(fallbackTimestamp.toPlainText().startsWith(QLatin1Char('[')) && !fallbackTimestamp.toPlainText().contains(QLatin1Char('\n')), "invalid timestamp falls back to safe default");
    QTextDocument indented; indented.setDefaultFont(QFont(QStringLiteral("monospace"), 12)); indented.setTextWidth(240);
    appendIrcMessage(&indented, QStringLiteral("Nick\tmessage message message message message message"), 0, palette, false, {}, 160);
    appendIrcMessage(&indented, QStringLiteral("LongerNick\tother text"), 0, palette, false, {}, 160);
    indented.size();
    auto firstLayout = indented.begin().layout(); auto secondLayout = indented.begin().next().layout();
    const qreal bodyX = firstLayout->lineAt(0).cursorToX(5);
    ok &= verify(firstLayout->lineCount() > 1 && std::abs(bodyX - firstLayout->lineAt(1).cursorToX(firstLayout->lineAt(1).textStart())) < 1, "wrapped text aligns with message body after nickname column");
    ok &= verify(std::abs(bodyX - secondLayout->lineAt(0).cursorToX(11)) < 1, "different nickname widths share one right-justified column");
    const QString retainedText = indented.toPlainText(); setChatIndentation(&indented, 0);
    ok &= verify(indented.toPlainText() == retainedText && indented.begin().blockFormat().leftMargin() == 0, "disabling indentation preserves transcript text");
    appendIrcMessage(&indented, QStringLiteral("Nick\tfirst\nplain continuation"), 0, palette, false, {}, 160);
    ok &= verify(indented.lastBlock().blockFormat().leftMargin() == 0 && indented.lastBlock().previous().blockFormat().leftMargin() > 0, "multiline output formats each paragraph using its own separator");
    QTextDocument copying;
    appendIrcMessage(&copying, QString(QChar(2)) + QStringLiteral("bold") + QChar(2) + QStringLiteral(" https://example.test"), 1, palette, true, QStringLiteral("[clock] "));
    appendIrcMessage(&copying, QString(QChar(3)) + QStringLiteral("04,02red") + QChar(15) + QStringLiteral(" plain"), 1, palette, true, QStringLiteral("[clock] "));
    QTextCursor copied(&copying); copied.select(QTextCursor::Document);
    const QString plainCopy = QStringLiteral("bold https://example.test\nred plain");
    ok &= verify(copyIrcSelection(copied, false, false) == plainCopy, "plain copy excludes timestamp fragments across lines");
    ok &= verify(copyIrcSelection(copied, true, false) == copying.toPlainText(), "timestamp-inclusive copy retains displayed timestamps");
    const QString coloredCopy = copyIrcSelection(copied, false, true);
    QString roundTrip; const auto copiedRuns = parseIrcText(coloredCopy, palette);
    bool copiedBold = false, copiedRed = false, syntheticLinkStyle = false;
    for (const auto &run : copiedRuns) {
        roundTrip += run.text;
        if (run.text.contains(QStringLiteral("bold"))) copiedBold = run.format.fontWeight() == QFont::Bold;
        if (run.text.contains(QStringLiteral("red"))) copiedRed = run.format.foreground().color() == ircPaletteColor(4, palette) && run.format.background().color() == ircPaletteColor(2, palette);
        if (run.text.contains(QStringLiteral("https://"))) syntheticLinkStyle = run.format.fontUnderline();
    }
    ok &= verify(roundTrip == plainCopy && copiedBold && copiedRed && !syntheticLinkStyle, "IRC copy preserves actual attributes and colors without synthetic hyperlink styling");
    copied.setPosition(2); copied.setPosition(6, QTextCursor::KeepAnchor);
    ok &= verify(copyIrcSelection(copied, false, false).isEmpty() && copyIrcSelection(copied, true, false) == QStringLiteral("lock"), "partial timestamp selection honors timestamp copy setting");
    copied.setPosition(9); copied.setPosition(11, QTextCursor::KeepAnchor);
    ok &= verify(copyIrcSelection(copied, false, false) == QStringLiteral("ol"), "partial fragment copy respects selection boundaries");
    ok &= verify(copyIrcSelection(QTextCursor(&copying), true, true).isEmpty(), "empty selection produces no clipboard text");
    auto contrast = [](const QColor &a, const QColor &b) {
        auto luminance = [](const QColor &color) {
            auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
            return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
        };
        const double first = luminance(a), second = luminance(b);
        return (qMax(first, second) + 0.05) / (qMin(first, second) + 0.05);
    };
    for (const QColor &base : {QColor("#1e1e1e"), QColor("#343434")}) {
        QPalette dark;
        dark.setColor(QPalette::Base, base); dark.setColor(QPalette::Text, QColor("#eeeeee"));
        dark.setColor(QPalette::Mid, base); dark.setColor(QPalette::Link, QColor("#0000ff"));
        for (int slot = 0; slot < 32; ++slot) {
            const auto runs = parseIrcText(QChar(3) + QString::number(slot).rightJustified(2, QLatin1Char('0')) + QStringLiteral("text"), dark);
            ok &= verify(contrast(runs.first().format.foreground().color(), base) >= 4.49, "default numeric IRC foregrounds have readable contrast on dark chat backgrounds");
            ok &= verify(contrast(ircPaletteColor(slot, dark), base) >= 4.49, "default nickname palette has readable dark contrast");
        }
        QTextDocument darkChat;
        appendIrcMessage(&darkChat, QStringLiteral("https://example.com dark link"), 12345, dark);
        bool timeFound = false, linkFound = false;
        for (auto it = darkChat.firstBlock().begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment(); const auto format = fragment.charFormat();
            if (format.boolProperty(IrcTimestampProperty)) {
                timeFound = true;
                ok &= verify(contrast(format.foreground().color(), base) >= 6.99, "timestamp remains readable when theme Mid is identical to dark background");
            }
            if (format.isAnchor()) {
                linkFound = true;
                ok &= verify(contrast(format.foreground().color(), base) >= 4.49, "theme link color remains readable on dark background");
            }
        }
        ok &= verify(timeFound && linkFound, "dark transcript regression exercises actual timestamp and link fragments");
        const auto pair = parseIrcText(QChar(3) + QStringLiteral("00,01explicit"), dark);
        ok &= verify(pair.first().format.foreground().color() == QColor(Qt::white) && pair.first().format.background().color() == QColor(Qt::black), "explicit IRC foreground/background pairs retain their colors");
        const auto rgb = parseIrcText(QChar(4) + QStringLiteral("010203rgb"), dark);
        ok &= verify(rgb.first().format.foreground().color() == QColor("#010203"), "explicit RGB text retains its specified color");
        QTextDocument darkCopy;
        appendIrcMessage(&darkCopy, QChar(3) + QStringLiteral("02blue"), 0, dark, false);
        QTextCursor selection(&darkCopy); selection.select(QTextCursor::Document);
        ok &= verify(copyIrcSelection(selection, false, true).contains(QStringLiteral("00007f")), "contrast adjustment keeps original IRC colors in copied formatting");
    }
    const QByteArray darkConfigured = "color_2 = 1010 1010 2020\n";
    ok &= verify(config.open(QIODevice::WriteOnly | QIODevice::Truncate), "dark configured color fixture opens");
    config.write(darkConfigured); config.close();
    ok &= verify(loadIrcPalette(paletteFile), "dark configured foreground loads");
    QPalette darkConfiguredPalette;
    darkConfiguredPalette.setColor(QPalette::Base, QColor("#202124"));
    darkConfiguredPalette.setColor(QPalette::Text, QColor("#eeeeee"));
    const auto adjusted = parseIrcText(QChar(3) + QStringLiteral("02configured"), darkConfiguredPalette);
    ok &= verify(contrast(adjusted.first().format.foreground().color(), darkConfiguredPalette.color(QPalette::Base)) >= 4.49, "configured numeric foreground stays readable on dark default background");
    QTextDocument configuredCopy;
    appendIrcMessage(&configuredCopy, QChar(3) + QStringLiteral("02configured"), 0, darkConfiguredPalette, false);
    QTextCursor configuredSelection(&configuredCopy); configuredSelection.select(QTextCursor::Document);
    ok &= verify(copyIrcSelection(configuredSelection, false, true).contains(QStringLiteral("101020")), "configured foreground copy preserves stored original color");
    ok &= verify(config.open(QIODevice::ReadOnly), "configured foreground fixture remains readable");
    ok &= verify(config.readAll() == darkConfigured, "display contrast treatment never rewrites configured palette values");
    config.close();
    return ok ? 0 : 1;
}
