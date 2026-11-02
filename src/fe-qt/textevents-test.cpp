/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "textevents.h"
#include "textevents-test-engine.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTextBrowser>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); QWidget parent; bool ok = true;
    auto check = [&](bool condition, const char *message) { if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    showTextEvents(&parent);
    auto *editor = parent.findChild<QDialog *>(QStringLiteral("textEventsEditor"));
    auto *format = editor->findChild<QLineEdit *>(QStringLiteral("eventFormat"));
    auto *table = editor->findChild<QTableView *>(QStringLiteral("eventRows"));
    check(format->text() == QStringLiteral("<$1> $2") && editor->findChild<QLabel *>(QStringLiteral("eventHelp"))->text().contains(QStringLiteral("$2: Message")), "selection shows format and argument help");
    format->setText(QStringLiteral("changed $1")); table->setCurrentIndex(table->model()->index(1, 0)); table->setCurrentIndex(table->model()->index(0, 0));
    check(format->text() == QStringLiteral("changed $1") && TextEventsTestEngine::texts.first() == QStringLiteral("<$1> $2"), "draft survives selection without changing core");
    editor->findChild<QPushButton *>(QStringLiteral("previewEvent"))->click();
    check(editor->findChild<QTextBrowser *>(QStringLiteral("eventPreview"))->toPlainText().contains(QStringLiteral("changed example1")), "preview renders sample arguments");
    format->setText(QStringLiteral("$9")); editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(editor->isVisible() && !editor->findChild<QLabel *>(QStringLiteral("eventError"))->text().isEmpty(), "invalid save remains editable");
    editor->findChild<QPushButton *>(QStringLiteral("defaultEvent"))->click();
    check(format->text() == QStringLiteral("<$1> $2"), "selected default restores format");
    format->setText(QStringLiteral("edited $1")); ++TextEventsTestEngine::revision;
    editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(editor->findChild<QLabel *>(QStringLiteral("eventError"))->text().contains(QStringLiteral("changed")), "concurrent changes block stale save");
    editor->findChild<QPushButton *>(QStringLiteral("reloadEvents"))->click(); format->setText(QStringLiteral("saved $1"));
    editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(TextEventsTestEngine::texts.first() == QStringLiteral("saved $1"), "save commits draft");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    showTextEvents(&parent); editor = parent.findChild<QDialog *>(QStringLiteral("textEventsEditor"));
    editor->findChild<QLineEdit *>(QStringLiteral("eventFormat"))->setText(QStringLiteral("discarded")); editor->reject();
    check(TextEventsTestEngine::texts.first() == QStringLiteral("saved $1"), "cancel discards edits");
    showSoundSettings(&parent);
    auto *sounds = parent.findChild<QDialog *>(QStringLiteral("soundSettings"));
    auto *soundRows = sounds->findChild<QTableView *>(QStringLiteral("soundRows"));
    soundRows->model()->setData(soundRows->model()->index(0, 1), QStringLiteral("draft.wav"));
    sounds->findChild<QPushButton *>(QStringLiteral("playSound"))->click();
    check(TextEventsTestEngine::played == QStringLiteral("draft.wav") && TextEventsTestEngine::sounds.first() == QStringLiteral("one.wav"), "sound preview uses draft without applying assignments");
    sounds->findChild<QPushButton *>(QStringLiteral("clearSound"))->click();
    check(soundRows->model()->index(0, 1).data().toString().isEmpty(), "clear removes selected draft assignment");
    sounds->findChild<QPushButton *>(QStringLiteral("reloadSounds"))->click();
    check(soundRows->model()->index(0, 1).data().toString() == QStringLiteral("one.wav"), "reload discards sound drafts");
    ++TextEventsTestEngine::soundRevision;
    sounds->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(sounds->isVisible() && sounds->findChild<QLabel *>(QStringLiteral("soundError"))->text().contains(QStringLiteral("changed")), "stale sound save blocked");
    sounds->findChild<QPushButton *>(QStringLiteral("reloadSounds"))->click();
    soundRows->model()->setData(soundRows->model()->index(0, 1), QStringLiteral("saved.wav"));
    sounds->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    check(TextEventsTestEngine::sounds.first() == QStringLiteral("saved.wav"), "sound save commits assignments");
    return ok ? 0 : 1;
}
