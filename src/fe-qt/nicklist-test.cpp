/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nicklist.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    NickList list;
    bool ok = true;
    auto verify = [&](bool condition, const char *message) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; }
    };
    auto first = std::make_shared<NickListState>();
    first->users.insert(QStringLiteral("Alice"), QStringLiteral("@Alice"));
    first->users.insert(QStringLiteral("Bob"), QStringLiteral("+Bob"));
    list.setState(first);
    first->prefixes = "!~&@%+";
    first->details[QStringLiteral("Alice")].host = QStringLiteral("alice@example.test");
    list.setDisplayOptions(true, true);
    verify(!list.item(0)->icon().isNull() && list.item(0)->text() == QStringLiteral("Alice  alice@example.test"), "graphical mode icon and hostname displayed");
    verify(list.item(0)->data(Qt::UserRole).toString() == QStringLiteral("Alice"), "display options preserve raw nickname identity");
    for (const auto &prefix : {QString("!"), QString("~"), QString("&"), QString("@"), QString("%"), QString("+")}) {
        first->users[QStringLiteral("Alice")] = prefix + QStringLiteral("Alice"); list.refresh();
        verify(!list.item(0)->icon().isNull(), "all native rank icons available including advertised custom upper ranks");
    }
    first->users[QStringLiteral("Alice")] = QStringLiteral("@Alice");
    list.setDisplayOptions(false, false);
    verify(list.item(0)->icon().isNull() && list.item(0)->text() == QStringLiteral("@Alice"), "text mode restores prefix and hides host");

    list.item(0)->setSelected(true);
    list.item(1)->setSelected(true);
    verify(first->selected.size() == 2, "multi-selection updates conversation state");
    first->users.insert(QStringLiteral("Carol"), QStringLiteral("Carol"));
    first->users[QStringLiteral("Alice")] = QStringLiteral("+Alice");
    list.refresh();
    verify(list.selectedItems().size() == 2 && first->selected.size() == 2,
           "join and prefix update preserve selection");
    verify(list.item(0)->text() == QStringLiteral("+Alice"), "prefix update displayed");
    QString target;
    list.action = [&](const QString &nick, bool whois) {
        verify(!whois, "activation opens private chat");
        target = nick;
    };
    list.itemActivated(list.item(0));
    verify(target == QStringLiteral("Alice"), "action uses nickname without privilege prefix");
    list.resize(200, 200);
    list.show();
    app.processEvents();
    bool requestedWhois = false;
    list.action = [&](const QString &nick, bool whois) {
        target = nick;
        requestedWhois = whois;
    };
    auto chooseWhois = [&]() {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (!menu) { verify(false, "context menu created"); return; }
        menu->setActiveAction(menu->actions().last());
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(menu, &enter);
    };
    QTimer::singleShot(0, &list, chooseWhois);
    list.customContextMenuRequested(list.visualItemRect(list.item(0)).center());
    verify(requestedWhois && target == QStringLiteral("Alice"), "WHOIS menu action");
    list.setModerationEnabled(true);
    QStringList moderated;
    list.userAction = [&](NickAction kind, const QStringList &nicks) {
        verify(kind == NickAction::Voice, "voice action kind"); moderated = nicks;
    };
    auto chooseVoice = [&]() {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        menu->setActiveAction(menu->findChild<QAction *>(QStringLiteral("giveVoice")));
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(menu, &enter);
    };
    QTimer::singleShot(0, &list, chooseVoice);
    list.customContextMenuRequested(list.visualItemRect(list.item(0)).center());
    verify(moderated == QStringList({QStringLiteral("Alice"), QStringLiteral("Bob")}), "selected users captured without privilege prefixes");
    QTimer::singleShot(0, &list, [&]() {
        first->users.remove(QStringLiteral("Bob")); first->selected.remove(QStringLiteral("Bob"));
        list.refresh(); chooseVoice();
    });
    list.customContextMenuRequested(list.visualItemRect(list.item(0)).center());
    verify(moderated == QStringList({QStringLiteral("Alice")}), "departed selection excluded before action");
    first->users.insert(QStringLiteral("Bob"), QStringLiteral("+Bob")); first->selected.insert(QStringLiteral("Bob")); list.refresh();
    target.clear();
    QTimer::singleShot(0, &list, [&]() {
        first->users.remove(QStringLiteral("Alice"));
        first->selected.remove(QStringLiteral("Alice"));
        list.refresh();
        chooseWhois();
    });
    list.customContextMenuRequested(list.visualItemRect(list.item(0)).center());
    verify(target.isEmpty(), "departed user cannot receive stale menu action");
    first->users.insert(QStringLiteral("Alice"), QStringLiteral("Alice"));
    first->selected.insert(QStringLiteral("Alice"));
    list.refresh();
    auto second = std::make_shared<NickListState>();
    second->users = first->users;
    list.setState(second);
    verify(list.selectedItems().isEmpty(), "conversation selections are independent");
    list.selectNick(QStringLiteral("Carol"), true);
    verify(second->selected == QSet<QString>{QStringLiteral("Carol")}, "programmatic selection");
    list.setState(first);
    verify(list.selectedItems().size() == 2, "selection restored after channel switch");
    first->selected.clear();
    list.refresh();
    verify(list.selectedItems().isEmpty(), "programmatic clear");
    list.selectNick(QStringLiteral("Missing"), true);
    verify(first->selected.isEmpty(), "unknown nickname ignored");
    QTimer::singleShot(0, &list, [&]() {
        list.setState(nullptr);
        chooseWhois();
    });
    list.customContextMenuRequested(list.visualItemRect(list.item(0)).center());
    verify(target.isEmpty(), "closed conversation cannot receive stale menu action");
    auto rich = std::make_shared<NickListState>();
    rich->users = {{QStringLiteral("Alpha"), QStringLiteral("Alpha")}, {QStringLiteral("Bob"), QStringLiteral("+Bob")},
                   {QStringLiteral("zed"), QStringLiteral("@zed")}};
    rich->details[QStringLiteral("Alpha")].rank = 12;
    rich->details[QStringLiteral("Bob")] = {QStringLiteral("user@host"), QStringLiteral("<b>Real</b>"), QStringLiteral("account"), QStringLiteral("irc.test"), true, false, 3};
    rich->details[QStringLiteral("zed")].rank = 0;
    list.setState(rich); list.setRankSorting(true);
    verify(list.item(0)->data(Qt::UserRole).toString() == QStringLiteral("zed"), "privilege sorting uses server rank");
    list.setRankSorting(false);
    verify(list.item(0)->data(Qt::UserRole).toString() == QStringLiteral("Alpha"), "alphabetic sorting ignores privilege prefix");
    list.setSortMode(2);
    verify(list.item(0)->data(Qt::UserRole).toString() == QStringLiteral("Alpha") && list.item(2)->data(Qt::UserRole).toString() == QStringLiteral("zed"), "reverse privilege sorting places operators last");
    list.setSortMode(3);
    verify(list.item(0)->data(Qt::UserRole).toString() == QStringLiteral("zed"), "reverse alphabetic sorting");
    rich->insertionOrder = {QStringLiteral("Bob"), QStringLiteral("zed"), QStringLiteral("Alpha")};
    list.setSortMode(4);
    verify(list.item(0)->data(Qt::UserRole).toString() == QStringLiteral("Bob"), "unsorted preserves insertion order");
    rich->details[QStringLiteral("Bob")].rank = 0; list.refresh();
    verify(list.item(0)->data(Qt::UserRole).toString() == QStringLiteral("Bob"), "metadata updates do not reorder unsorted users");
    int compared = 0;
    rich->compare = [&](const QString &a, const QString &b) { ++compared; return -QString::compare(a, b, Qt::CaseInsensitive); };
    list.setSortMode(1);
    verify(compared > 0 && list.item(0)->data(Qt::UserRole).toString() == QStringLiteral("zed"), "alphabetic modes honor server comparator");
    rich->compare = {}; list.refresh();
    QString activated;
    list.activate = [&](const QString &nick) { activated = nick; };
    list.itemActivated(list.item(0));
    verify(activated == QStringLiteral("Alpha"), "activation callback gets bare nickname");

    verify(list.item(1)->font().italic() && list.item(1)->toolTip().contains(QStringLiteral("&lt;b&gt;")), "away styling and escaped metadata tooltip");
    list.item(0)->setSelected(true); list.item(1)->setSelected(true);
    list.setFilterText(QStringLiteral("ACCOUNT"));
    verify(list.visibleCount() == 1 && rich->selected.size() == 2, "metadata filter preserves hidden selection state");
    QStringList copied;
    list.userAction = [&](NickAction kind, const QStringList &nicks) { verify(kind == NickAction::Copy, "copy filtered users action"); copied = nicks; };
    QTimer::singleShot(0, &list, [&]() {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        menu->setActiveAction(menu->findChild<QAction *>(QStringLiteral("copyNicks")));
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); QApplication::sendEvent(menu, &enter);
    });
    list.customContextMenuRequested(list.visualItemRect(list.item(1)).center());
    verify(copied == QStringList{QStringLiteral("Bob")}, "hidden selections are excluded from user actions");
    list.setFilterText(QString());
    verify(list.visibleCount() == 3 && list.selectedItems().size() == 2, "clearing filter restores selected users");
    list.setAwayColor(QColor(Qt::gray)); list.setNicknameColors([](const QString &) { return QColor(Qt::red); });
    verify(list.item(0)->foreground().color() == QColor(Qt::red) && list.item(1)->foreground().color() == QColor(Qt::gray), "nickname colors apply to present users while away style takes precedence");
    list.setNicknameColors({});
    verify(list.item(0)->foreground().style() == Qt::NoBrush && list.item(1)->foreground().color() == QColor(Qt::gray), "disabling nickname colors restores normal users without removing away styling");
    list.setState(nullptr);
    verify(list.count() == 0, "closing a conversation detaches its user list");
    auto *closing = new NickList;
    closing->setState(rich); closing->resize(200, 200); closing->show(); app.processEvents();
    bool closedAction = false;
    closing->action = [&](const QString &, bool) { closedAction = true; };
    QTimer::singleShot(0, [&]() {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        verify(menu != nullptr, "popup survives closing list long enough to dismiss safely");
        delete closing;
        if (menu) { menu->setActiveAction(menu->actions().last()); QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); QApplication::sendEvent(menu, &enter); }
    });
    closing->customContextMenuRequested(closing->visualItemRect(closing->item(0)).center());
    verify(!closedAction, "closing source list while popup is open suppresses action safely");
    return ok ? 0 : 1;
}
