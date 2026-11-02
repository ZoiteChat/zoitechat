/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatbackground.h"
#include <QFileInfo>
#include <QImageReader>
bool loadChatBackground(const QString &path, QImage &image, QString &error)
{
    if (path.isEmpty()) { image = {}; error.clear(); return true; }
    if (path.contains(QChar(0)) || path.contains(QLatin1Char('\n')) || path.contains(QLatin1Char('\r')) || path.size() > 4096) {
        error = QStringLiteral("Choose a valid image filename."); return false;
    }
    const QFileInfo file(path);
    if (!file.isFile() || file.size() > 32 * 1024 * 1024) { error = QStringLiteral("Background must be an image file no larger than 32 MiB."); return false; }
    QImageReader reader(path); reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (!size.isValid() || size.width() > 4096 || size.height() > 4096) { error = QStringLiteral("Background dimensions must not exceed 4096 × 4096 pixels."); return false; }
    QImage decoded = reader.read();
    if (decoded.isNull() || decoded.width() > 4096 || decoded.height() > 4096) { error = QStringLiteral("Could not read background image: ") + reader.errorString(); return false; }
    image = std::move(decoded); error.clear(); return true;
}
