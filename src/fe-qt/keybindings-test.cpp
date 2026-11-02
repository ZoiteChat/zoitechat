/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "keybindings.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); QWidget parent; QLineEdit input(&parent); QTemporaryDir directory; bool ok = true;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    const QString path = directory.filePath(QStringLiteral("keybindings.conf"));
    auto write = [&](const QByteArray &bytes) { QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); };
    auto read = [&]() { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return QByteArray(); return file.readAll(); };
    const QByteArray fixture = "# personal bindings\nACCEL=<Primary>F10\nRun Command\nD1:/echo first\\n/echo second\nD2!\n\nACCEL=<Alt>x\nInsert in Buffer\nD1:hello\nD2!\n\nACCEL=<Hyper>F11\nFuture Action\nD1:preserve\nD2:extra\n";
    check(write(fixture), "native binding fixture saved");
    QList<InputBinding> executed;
    InputBindings bindings(&input, path, [&](const InputBinding &row) { executed.append(row); return true; });
    check(inputBindingKey(QStringLiteral("<Primary>Page_Up")) == QKeySequence(QStringLiteral("Ctrl+PgUp")), "GTK modifier and key names map to Qt");
    check(inputBindingKey(QStringLiteral("<Shift>ISO_Left_Tab")) == QKeySequence(QKeyCombination(Qt::ShiftModifier, Qt::Key_Tab)), "reverse Tab normalized");
    check(inputBindingKey(QStringLiteral("<Hyper>F11")).isEmpty(), "unknown modifiers stay inactive");
    QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_F10, Qt::ControlModifier); override.ignore(); QApplication::sendEvent(&input, &override);
    check(override.isAccepted() && executed.isEmpty(), "custom shortcut claims input scope without running during override");
    QKeyEvent key(QEvent::KeyPress, Qt::Key_F10, Qt::ControlModifier); QApplication::sendEvent(&input, &key);
    check(executed.size() == 1 && inputBindingCommands(executed.last().first) == QStringList({QStringLiteral("/echo first"), QStringLiteral("/echo second")}), "command binding dispatched and multiline escape decoded");
    QKeyEvent repeat(QEvent::KeyPress, Qt::Key_F10, Qt::ControlModifier, QString(), true); QApplication::sendEvent(&input, &repeat);
    check(executed.size() == 1, "held command shortcut does not repeatedly execute");
    {
        QKeyEvent letter(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier, QString(QChar(1)));
        auto translated = pluginKey(letter);
        check(translated.state == 5 && translated.keyval == 'A' && translated.unicode == 'A', "plugin Ctrl+Shift letter keeps GDK modifier bits and printable character");
        QKeyEvent emoji(QEvent::KeyPress, 0x1f600, Qt::NoModifier, QString::fromUtf8("😀"));
        translated = pluginKey(emoji);
        check(translated.keyval == 0x0101f600 && translated.unicode == 0x1f600, "plugin Unicode keys retain full scalar values");
        QKeyEvent reverse(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
        translated = pluginKey(reverse);
        check(translated.state == 1 && translated.keyval == 0xfe20 && !translated.unicode, "reverse Tab maps to GDK ISO_Left_Tab");
        QKeyEvent keypad(QEvent::KeyPress, Qt::Key_1, Qt::KeypadModifier, QStringLiteral("1"));
        translated = pluginKey(keypad);
        check(translated.keyval == 0xffb1 && translated.unicode == '1', "plugin keypad keyval differs from ordinary digit");
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Enter, Qt::KeypadModifier);
        translated = pluginKey(enter);
        check(translated.keyval == 0xff8d && !translated.unicode, "keypad Enter preserves native GDK hook values");
        QKeyEvent dead(QEvent::KeyPress, Qt::Key_Dead_Acute, Qt::NoModifier);
        translated = pluginKey(dead);
        check(translated.keyval == 0xfe51 && !translated.unicode, "dead accent maps without inserting composed text");
        const int before = executed.size(); int hooks = 0;
        bindings.keypress = [&](const QKeyEvent &) { ++hooks; return true; };
        QApplication::sendEvent(&input, &key);
        check(hooks == 1 && executed.size() == before, "consumed plugin key prevents native binding dispatch");
        bindings.keypress = [&](const QKeyEvent &) { ++hooks; return false; };
        QApplication::sendEvent(&input, &key);
        check(hooks == 2 && executed.size() == before + 1, "unconsumed plugin key reaches native binding");
        bindings.keypress = {};
    }
    bindings.edit(&parent); auto *editor = parent.findChild<QDialog *>(QStringLiteral("keyBindingsEditor")); auto *table = editor->findChild<QTableView *>(QStringLiteral("keyBindingRows"));
    check(table->model()->rowCount() == 3 && table->model()->index(2,4).data().toString().contains(QStringLiteral("Unsupported")), "unsupported native rows remain visible");
    table->model()->setData(table->model()->index(0,2), QStringLiteral("bad\nvalue"));
    auto *save = editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save); save->click();
    check(read() == fixture && !editor->findChild<QLabel *>(QStringLiteral("keyBindingError"))->text().isEmpty(), "invalid save leaves native file unchanged");
    table->model()->setData(table->model()->index(0,2), QStringLiteral("/echo changed")); check(write(fixture + "# external\n"), "external edit saved"); save->click();
    check(editor->findChild<QLabel *>(QStringLiteral("keyBindingError"))->text().contains(QStringLiteral("externally")), "stale editor cannot overwrite external changes");
    editor->findChild<QPushButton *>(QStringLiteral("reloadKeyBindings"))->click(); table->model()->setData(table->model()->index(0,2), QStringLiteral("/echo changed")); save->click();
    check(read().contains("Future Action\nD1:preserve\nD2:extra") && read().contains("# external"), "unknown action data and comments survive save");
    QApplication::sendEvent(&input, &key); check(executed.last().first == QStringLiteral("/echo changed"), "saved bindings reload into input dispatcher");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const QByteArray legacy = "# legacy mixed file\nCS\nF8\nRun Command\nD1:/echo legacy\nD2!\n\nACCEL=F7\nSet Buffer\nD1:mixed\nD2!\n";
    check(write(legacy) && bindings.reload(), "mixed legacy modifier/key and ACCEL bindings load");
    QKeyEvent legacyKey(QEvent::KeyPress, Qt::Key_F8, Qt::ControlModifier | Qt::ShiftModifier); QApplication::sendEvent(&input, &legacyKey);
    check(executed.last().first == QStringLiteral("/echo legacy"), "legacy key modifiers route command");
    bindings.edit(&parent); editor = parent.findChild<QDialog *>(QStringLiteral("keyBindingsEditor"));
    editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(read().contains("ACCEL=<Primary><Shift>F8") && read().contains("# legacy mixed file"), "legacy editor saves interoperable ACCEL format and preserves comments");
    check(write(fixture) && bindings.reload(), "restore modern binding fixture");
    check(write("legacy unreadable format\n"), "legacy fixture saved"); QString error;
    check(!bindings.reload(&error) && !error.isEmpty(), "unsupported file is reported instead of replacing active bindings");
    QApplication::sendEvent(&input, &key); check(inputBindingCommands(executed.last().first) == QStringList({QStringLiteral("/echo first"), QStringLiteral("/echo second")}), "failed reload retains last working bindings");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    check(QFile::remove(path) && bindings.reload(), "missing file loads native defaults without writing config");
    check(defaultInputBindings().size() == 39 && !QFile::exists(path), "native default map has all 39 rows and stays implicit");
    for (const auto &row : defaultInputBindings()) check(inputBindingSupported(row.action) && !inputBindingKey(row.key).isEmpty(), "every native default key and action resolves");
    QKeyEvent defaultScroll(QEvent::KeyPress, Qt::Key_PageUp, Qt::NoModifier); QApplication::sendEvent(&input, &defaultScroll);
    check(executed.last().action == QStringLiteral("Scroll Page") && executed.last().first == QStringLiteral("Up"), "default Page Up dispatches transcript scrolling");
    bindings.edit(&parent); editor = parent.findChild<QDialog *>(QStringLiteral("keyBindingsEditor")); table = editor->findChild<QTableView *>(QStringLiteral("keyBindingRows"));
    check(table->model()->rowCount() == 39, "editor shows implicit defaults on first run");
    editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(read().contains("ACCEL=<Primary><Shift>Page_Up") && read().contains("Move tab family left"), "explicit save materializes interoperable native defaults");
    return ok ? 0 : 1;
}
