/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "paletteeditor.h"
#include "ircformat.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); QWidget parent; QTemporaryDir directory; bool ok = true; int applied = 0;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    const QString path = directory.filePath(QStringLiteral("colors.conf"));
    auto write = [&](const QByteArray &bytes) { QFile file(path); if (!file.open(QIODevice::WriteOnly)) return false; return file.write(bytes) == bytes.size(); };
    auto read = [&]() { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return QByteArray(); return file.readAll(); };
    const QByteArray original = "# personal colors\ncolor_0 = 1234 5678 abcd\ncolor_18 = 1111 2222 3333\nfuture_option = preserve\n";
    check(write(original), "fixture saved");
    showPaletteEditor(path, &parent, [&]() { ++applied; });
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("paletteEditor"));
    auto *table = dialog->findChild<QTableView *>(QStringLiteral("paletteRows"));
    check(table->model()->rowCount() == 42 && table->model()->index(0, 1).data().toString() == QStringLiteral("#1256ab"), "native entries populate editable draft");
    table->model()->setData(table->model()->index(18, 1), QStringLiteral("bad"));
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(read() == original && applied == 0 && !dialog->findChild<QLabel *>(QStringLiteral("paletteError"))->text().isEmpty(), "invalid edit leaves disk and live state unchanged");
    table->model()->setData(table->model()->index(18, 1), QStringLiteral("#aabbcc"));
    check(read() == original, "valid edits remain drafts");
    check(write(original + "# external\n"), "external change written");
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(applied == 0 && dialog->findChild<QLabel *>(QStringLiteral("paletteError"))->text().contains(QStringLiteral("changed")), "stale editor cannot overwrite external changes");
    dialog->findChild<QPushButton *>(QStringLiteral("reloadPalette"))->click();
    table->model()->setData(table->model()->index(18, 1), QStringLiteral("#aabbcc"));
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    const QByteArray saved = read();
    check(applied == 1 && saved.contains("color_18 = aaaa bbbb cccc") && !saved.contains("color_18 = 1111"), "save updates changed native entry and invokes apply");
    check(saved.contains("color_0 = 1234 5678 abcd\n") && saved.contains("future_option = preserve\n") && saved.contains("# external\n"), "unmodified precision, comments and unknown settings preserved");
    const auto complete = parseIrcPaletteConfig(saved);
    for (const auto &color : complete) check(color.isValid(), "saved palette supplies every native entry");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    showPaletteEditor(path, &parent, [&]() { ++applied; });
    dialog = parent.findChild<QDialog *>(QStringLiteral("paletteEditor")); table = dialog->findChild<QTableView *>(QStringLiteral("paletteRows"));
    table->setCurrentIndex(table->model()->index(18, 1));
    dialog->findChild<QPushButton *>(QStringLiteral("defaultPaletteColor"))->click();
    check(table->model()->index(18, 1).data().toString() == defaultIrcPaletteColor(18, app.palette()).name(), "selected default restores Qt default as draft");
    dialog->reject();
    check(read() == saved && applied == 1, "Cancel does not write or apply defaults");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    showPaletteEditor(directory.filePath(QStringLiteral("missing/colors.conf")), &parent, [&]() { ++applied; });
    dialog = parent.findChild<QDialog *>(QStringLiteral("paletteEditor"));
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(applied == 1 && dialog->isVisible() && !dialog->findChild<QLabel *>(QStringLiteral("paletteError"))->text().isEmpty(), "write failure keeps editor open without applying");
    dialog->reject();
    return ok ? 0 : 1;
}
