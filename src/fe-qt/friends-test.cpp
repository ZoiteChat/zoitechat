/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "friends.h"
#include "friends-test-engine.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QWidget>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); QWidget parent; initializeFriends(&parent);
    bool ok = true;
    auto verify = [&](bool condition, const char *message) { if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    int keys[2];
    FriendsTestEngine::rules[QStringLiteral("Alice")] = QStringLiteral("NetA,NetB");
    FriendsTestEngine::rows = {{QStringLiteral("Alice"), QStringLiteral("NetA"), &keys[0], 1, 123, 100, 0},
                               {QStringLiteral("Alice"), QStringLiteral("NetB"), &keys[1], 0, 100, 50, 110}};
    showFriendsList();
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("friendsList"));
    auto *table = dialog->findChild<QTableView *>(QStringLiteral("friendRows"));
    auto *filter = dialog->findChild<QLineEdit *>(QStringLiteral("friendsFilter"));
    verify(table->model()->rowCount() == 2, "friends have independent network status rows");
    filter->setText(QStringLiteral("ONLINE")); verify(table->model()->rowCount() == 1, "status filter is case insensitive");
    table->selectRow(0); dialog->findChild<QPushButton *>(QStringLiteral("queryFriend"))->click();
    verify(FriendsTestEngine::queried == QStringLiteral("Alice") && FriendsTestEngine::queriedServer == &keys[0], "query chooses correct online network");
    FriendsTestEngine::rows[0].online = 0; FriendsTestEngine::queried.clear();
    dialog->findChild<QPushButton *>(QStringLiteral("queryFriend"))->click();
    verify(FriendsTestEngine::queried.isEmpty(), "stale online selection cannot open an offline friend");
    qt_fe_friends_changed(); app.processEvents();
    verify(table->model()->rowCount() == 0, "presence callbacks refresh filtered results"); filter->clear();
    auto finishEditor = [&](const QString &name, const QString &networks) {
        auto *editor = dialog->findChild<QDialog *>(QStringLiteral("friendEditor"));
        editor->findChild<QLineEdit *>(QStringLiteral("friendName"))->setText(name);
        editor->findChild<QLineEdit *>(QStringLiteral("friendNetworks"))->setText(networks);
        editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        app.processEvents(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };
    dialog->findChild<QPushButton *>(QStringLiteral("addFriend"))->click(); finishEditor(QStringLiteral("Bob"), QString());
    verify(FriendsTestEngine::rules.contains(QStringLiteral("Bob")) && table->model()->rowCount() == 3, "adding all-network friend shows unknown status");
    filter->setText(QStringLiteral("Bob")); table->selectRow(0);
    verify(table->model()->index(0, 1).data().toString() == QStringLiteral("Unknown") &&
           !dialog->findChild<QPushButton *>(QStringLiteral("queryFriend"))->isEnabled(), "unknown friend is not falsely online");
    dialog->findChild<QPushButton *>(QStringLiteral("editFriend"))->click(); finishEditor(QStringLiteral("Robert"), QStringLiteral("NetA"));
    verify(!FriendsTestEngine::rules.contains(QStringLiteral("Bob")) && FriendsTestEngine::rules.value(QStringLiteral("Robert")) == QStringLiteral("NetA"), "editing renames friend and changes network scope");
    filter->clear(); dialog->findChild<QPushButton *>(QStringLiteral("addFriend"))->click();
    auto *editor = dialog->findChild<QDialog *>(QStringLiteral("friendEditor"));
    editor->findChild<QLineEdit *>(QStringLiteral("friendName"))->setText(QStringLiteral("Alice"));
    editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    verify(!editor->findChild<QLabel *>(QStringLiteral("friendError"))->text().isEmpty(), "duplicate friend rejected inline");
    editor->reject(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    filter->setText(QStringLiteral("Robert")); table->selectRow(0);
    dialog->findChild<QPushButton *>(QStringLiteral("removeFriend"))->click(); app.processEvents();
    verify(!FriendsTestEngine::rules.contains(QStringLiteral("Robert")), "friend removal updates core");
    qt_fe_friend_ask("Carol", "NetA"); editor = dialog->findChild<QDialog *>(QStringLiteral("friendEditor"));
    verify(editor->findChild<QLineEdit *>(QStringLiteral("friendName"))->text() == QStringLiteral("Carol") &&
           editor->findChild<QLineEdit *>(QStringLiteral("friendNetworks"))->text() == QStringLiteral("NetA"), "core prompt prepopulates friend editor");
    editor->reject(); verify(!FriendsTestEngine::rules.contains(QStringLiteral("Carol")), "cancel does not add friend");
    return ok ? 0 : 1;
}
