/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "menumarkup.h"
#include <QApplication>
#include <QFont>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); bool ok = true;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    bool valid;
    check(plainMenuMarkup(QString::fromUtf8("<b>_Café &amp; <i>世界</i></b>"), &valid) == QString::fromUtf8("_Café & 世界") && valid,
          "native Pango parser preserves text/entities and mnemonic markers");
    const QString broken = QStringLiteral("<b>unfinished");
    check(plainMenuMarkup(broken, &valid) == broken && !valid, "invalid markup remains literal");
    QFont font(QStringLiteral("Sans"), 12);
    const QString rich = QStringLiteral("<span foreground='#ff0000' size='large' weight='bold'>_Marked</span>");
    auto one = renderMenuMarkup(rich, font, Qt::black, 96, 1, false);
    auto two = renderMenuMarkup(rich, font, Qt::black, 96, 2, false);
    check(!one.isNull() && two.devicePixelRatio() == 2 && qAbs(one.width() - two.width() / 2) <= 1 && qAbs(one.height() - two.height() / 2) <= 1,
          "markup raster keeps logical dimensions at high DPI");
    int red = 0;
    for (int y = 0; y < one.height(); ++y) for (int x = 0; x < one.width(); ++x) {
        const auto color = one.pixelColor(x, y);
        if (color.alpha() > 128 && color.red() > 180 && color.green() < 80 && color.blue() < 80) ++red;
    }
    check(red > 10, "Pango foreground and weight render into actual image pixels");
    const auto small = renderMenuMarkup(QStringLiteral("<small>Marked</small>"), font, Qt::black, 96, 1, false);
    check(one.height() > small.height() && one.width() > small.width(), "Pango size attributes change measured label dimensions");
    check(renderMenuMarkup(broken, font, Qt::black, 96, 1, false).isNull(), "invalid markup does not render as HTML");
    check(renderMenuMarkup(QStringLiteral("<img src='file:///tmp/no-image'/ >"), font, Qt::black, 96, 1, false).isNull(), "HTML image elements are not Pango markup");
    check(renderMenuMarkup(rich, font, Qt::black, 0, 1, false).isNull(), "invalid rendering scale rejected");
    return ok ? 0 : 1;
}
