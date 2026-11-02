/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatbackground.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <cstdio>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv); QTemporaryDir dir; bool ok = true;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    QImage source(8, 8, QImage::Format_ARGB32); source.fill(QColor(20, 30, 40, 128));
    const QString path = dir.filePath(QStringLiteral("background with spaces.png")); check(source.save(path), "fixture writes");
    QImage loaded; QString error; check(loadChatBackground(path, loaded, error) && loaded.size() == source.size() && error.isEmpty(), "local image decodes with alpha");
    const QImage previous = loaded;
    check(!loadChatBackground(dir.filePath(QStringLiteral("missing.png")), loaded, error) && loaded == previous && !error.isEmpty(), "failed load preserves previous image");
    QFile bad(dir.filePath(QStringLiteral("bad.png"))); check(bad.open(QIODevice::WriteOnly), "bad fixture opens"); bad.write("not an image"); bad.close();
    check(!loadChatBackground(bad.fileName(), loaded, error) && loaded == previous, "invalid image is rejected transactionally");
    QImage wide(4097, 1, QImage::Format_RGB32); wide.fill(Qt::black); const QString widePath = dir.filePath(QStringLiteral("wide.png")); check(wide.save(widePath), "oversized fixture writes");
    check(!loadChatBackground(widePath, loaded, error) && loaded == previous, "oversized dimensions rejected before use");
    check(!loadChatBackground(path + QChar(0), loaded, error), "NUL in filename rejected");
    check(loadChatBackground(QString(), loaded, error) && loaded.isNull() && error.isEmpty(), "empty filename clears image");
    return ok ? 0 : 1;
}
