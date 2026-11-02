/* SPDX-License-Identifier: GPL-2.0-or-later */
// Exercise the real Qt window with an in-memory engine adapter, without IRC.
#include "qtfrontend.cpp"
#include "tools-test-engine.h"
#include "moderation-test-engine.h"
#include "friends-test-engine.h"
#include "customization-test-engine.h"
#include "textevents-test-engine.h"
#include "addons-test-engine.h"
#include "transfers-test-engine.h"
#include "conversationoptions-test-engine.h"
#include "preferences-test-engine.h"
#include <QFile>
#include <QTextBlock>
#include <QPointer>
#include <QTemporaryDir>
#include <QSystemTrayIcon>
#include <cstdio>

namespace {
// Qt 6.12+ supplies a floating-point constructor; retain Qt 6.2 compatibility.
auto dragEnterPosition(const QPoint &point)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    return QPointF(point);
#else
    return point;
#endif
}
QByteArray configPath;
QHash<void *, QtSessionInfo> engine;
struct Command { void *session; QString line; bool literal = false; };
QList<Command> commands;
QString sentTopic;
void *activated = nullptr;
int quits = 0;
int explicitCommands = 0;
bool confirmAwayCommands = true;
struct KeyCall { void *session; unsigned state, keyval, unicode; };
QList<KeyCall> pluginKeys;
std::function<int(void *)> keyHook;
QList<void *> nativeActivityQueue;
QStringList completionTestNicks{QStringLiteral("Arthur")};
}
extern "C" {
const char *qt_core_config_dir(void) { return configPath.constData(); }
int qt_core_nick_color(const char *) { return 4; }
int qt_core_alert_policy(void *, int mention, int privateMessage, QtAlertPolicy *policy) { *policy = {mention || privateMessage, 1, 1, 0, 1, 0}; return 1; }
int qt_core_session_info(void *session, QtSessionInfo *info)
{
    if (!engine.contains(session)) return 0;
    *info = engine.value(session);
    return 1;
}
void qt_core_completion_read(void *, int kind, void (*visit)(const char *, void *), void *data) {
    if (kind == 2) { for (const auto &nick : completionTestNicks) visit(nick.toUtf8().constData(), data); }
    else visit(kind ? "JOIN" : "#completion-test", data);
}
int qt_core_keypress(void *session, unsigned state, unsigned keyval, unsigned unicode) {
    pluginKeys.append({session, state, keyval, unicode});
    const auto hook = keyHook;
    return hook ? hook(session) : 0;
}
void *qt_core_next_activity(void) {
    for (int i = 0; i < nativeActivityQueue.size(); ++i)
        if (qt_fe_session_attached(nativeActivityQueue[i])) return nativeActivityQueue.takeAt(i);
    return nullptr;
}
int qt_core_completion_compare(void *, const char *left, const char *right) { return QString::fromUtf8(left).compare(QString::fromUtf8(right), Qt::CaseInsensitive); }
void qt_core_activate(void *session) { activated = session; }
void qt_core_submit(void *session, const char *line)
{
    commands.append({session, QString::fromUtf8(line)});
    if (commands.last().line == QStringLiteral("/flushq")) engine[session].queued_bytes = 0;
    if (commands.last().line == QStringLiteral("/discon")) engine[session].connected = false;
    if (confirmAwayCommands && commands.last().line == QStringLiteral("/back")) engine[session].away = false;
    if (confirmAwayCommands && (commands.last().line == QStringLiteral("/away") || commands.last().line.startsWith(QStringLiteral("/away ")))) engine[session].away = true;
    if (commands.last().line == QStringLiteral("/close")) { engine.remove(session); qt_fe_remove_session(session); }
}
void qt_core_submit_command(void *session, const char *line) { ++explicitCommands; qt_core_submit(session, line); }
const char *qt_core_command_prefix(void) { for (const auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "input_command_char") return row.value; return "/"; }
void qt_core_submit_literal(void *session, const char *line) { commands.append({session, QString::fromUtf8(line), true}); }
void qt_core_set_topic(void *, const char *topic) { sentTopic = QString::fromUtf8(topic); }
void qt_core_quit(void) { ++quits; }
}
void showNetworkManager(QWidget *) {}

int main(int argc, char **argv)
{
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    configPath = directory.path().toUtf8();
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "input_command_char") qstrncpy(row.value, "/", sizeof(row.value));
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_ulist_doubleclick") qstrncpy(row.value, "QUERY %s", sizeof(row.value));
    for (auto &row : PreferencesTestEngine::rows) {
        if (QByteArray(row.key) == "gui_tray" || QByteArray(row.key) == "gui_tray_blink") qstrncpy(row.value, "1", sizeof(row.value));
        if (QByteArray(row.key) == "gui_tab_pos") qstrncpy(row.value, "5", sizeof(row.value));
        if (QByteArray(row.key) == "gui_ulist_pos") qstrncpy(row.value, "3", sizeof(row.value));
        if (QByteArray(row.key) == "gui_tab_layout") qstrncpy(row.value, "2", sizeof(row.value));
        if (QByteArray(row.key) == "gui_tab_newtofront") qstrncpy(row.value, "2", sizeof(row.value));
        if (QByteArray(row.key) == "gui_tab_trunc") qstrncpy(row.value, "20", sizeof(row.value));
        if (QByteArray(row.key) == "gui_tab_middleclose") qstrncpy(row.value, "1", sizeof(row.value));
        if (QByteArray(row.key) == "text_show_marker") qstrncpy(row.value, "1", sizeof(row.value));
        if (QByteArray(row.key) == "gui_ulist_count") qstrncpy(row.value, "1", sizeof(row.value));
        if (QByteArray(row.key) == "gui_lagometer" || QByteArray(row.key) == "gui_throttlemeter") qstrncpy(row.value, "2", sizeof(row.value));
    }
    for (auto &row : PreferencesTestEngine::rows)
        if (QByteArray(row.key) == "gui_topicbar" || QByteArray(row.key) == "gui_ulist_buttons" || QByteArray(row.key) == "gui_mode_buttons") qstrncpy(row.value, "1", sizeof(row.value));
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    qt_fe_initialize(&argc, argv);
    bool ok = true;
    auto verify = [&](bool condition, const char *message) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; }
    };
    auto action = [&](const char *name) { return window->findChild<QAction *>(QString::fromLatin1(name)); };
    auto trigger = [&](const char *name) {
        auto *item = action(name);
        verify(item != nullptr, name);
        if (item) item->trigger();
    };
    {
        ChatTranscript clipboardView;
        appendIrcMessage(clipboardView.document(), QString(QChar(2)) + QStringLiteral("bold"), 1, clipboardView.palette(), true, QStringLiteral("[copy] "));
        clipboardView.selectAll(); clipboardView.copy();
        verify(QApplication::clipboard()->text() == QStringLiteral("bold"), "manual transcript copy omits timestamps by default");
        clipboardView.copyTimestamps = true; clipboardView.copy();
        verify(QApplication::clipboard()->text() == QStringLiteral("[copy] bold"), "manual copy honors native timestamp behavior");
        clipboardView.copyTimestamps = false; clipboardView.copyColors = true; clipboardView.copy();
        verify(QApplication::clipboard()->text() == QString(QChar(2)) + QStringLiteral("bold") + QChar(15), "manual copy exports actual IRC attributes");
        clipboardView.copyColors = false; clipboardView.autoCopy = true;
        QMouseEvent shiftCopy(QEvent::MouseButtonRelease, QPointF(1, 1), QPointF(1, 1), Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
        QApplication::sendEvent(clipboardView.viewport(), &shiftCopy);
        verify(QApplication::clipboard()->text() == QStringLiteral("[copy] bold"), "Shift-selection includes timestamps without changing preference");
        QMouseEvent controlCopy(QEvent::MouseButtonRelease, QPointF(1, 1), QPointF(1, 1), Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
        QApplication::sendEvent(clipboardView.viewport(), &controlCopy);
        verify(QApplication::clipboard()->text() == QString(QChar(2)) + QStringLiteral("bold") + QChar(15), "Ctrl-selection includes IRC attributes without changing preference");
    }
    action("trayEnabled")->setChecked(false);
    window->startupState(1);
    verify(window->isMinimized(), "startup level one iconifies main window");
    window->showNormal(); window->startupState(2);
    verify(window->isMinimized(), "startup tray mode falls back to iconification when tray disabled");
    window->showNormal(); action("trayEnabled")->setChecked(true);
    PreferencesTestEngine::saves = 0;
    verify(app->desktopFileName() == QStringLiteral("net.zoite.Zoitechat.Qt") && !app->windowIcon().isNull(), "desktop identity matches the installed Qt launcher and has an embedded icon fallback");
    // Compare the supported menu structure with fe-gtk/menu.c.
    QStringList topMenus;
    for (auto *item : window->menuBar()->actions()) {
        QString text = item->text(); text.remove(QLatin1Char('&')); topMenus.append(text);
    }
    verify(topMenus == QStringList({QStringLiteral("ZoiteChat"), QStringLiteral("View"), QStringLiteral("Server"), QStringLiteral("Usermenu"),
           QStringLiteral("Settings"), QStringLiteral("Window"), QStringLiteral("Help")}), "original ZoiteChat top-level menus and order");
    auto *nativeUsermenu = window->findChild<QMenu *>(QStringLiteral("userMenu"));
    verify(nativeUsermenu && !nativeUsermenu->menuAction()->isVisible(), "Usermenu follows native default of hidden");
    auto menuIds = [&](const char *name) {
        QStringList ids;
        auto *menu = window->findChild<QMenu *>(QString::fromLatin1(name));
        verify(menu != nullptr, name);
        if (!menu) return ids;
        for (auto *item : menu->actions())
            ids.append(item->isSeparator() ? QStringLiteral("|") : item->menu() ? item->menu()->objectName() : item->objectName());
        return ids;
    };
    verify(menuIds("zoitechatMenu") == QStringList({QStringLiteral("networkList"), QStringLiteral("|"), QStringLiteral("newMenu"), QStringLiteral("|"),
           QStringLiteral("loadPlugin"), QStringLiteral("|"), QStringLiteral("detachConversation"), QStringLiteral("closeConversation"), QStringLiteral("|"), QStringLiteral("quit")}), "ZoiteChat menu matches original complete file menu order");
    verify(menuIds("viewMenu").mid(0, 7) == QStringList({QStringLiteral("showMenu"), QStringLiteral("showTopic"), QStringLiteral("showUsers"), QStringLiteral("showUserButtons"), QStringLiteral("showChannelModes"), QStringLiteral("channelSwitcherMenu"), QStringLiteral("networkMetersMenu")}), "supported native View controls retain original order");
    verify(menuIds("networkMetersMenu") == QStringList({QStringLiteral("networkMeter0"), QStringLiteral("networkMeter1"), QStringLiteral("networkMeter2"), QStringLiteral("networkMeter3")}), "Network Meters submenu contains all native modes");
    verify(menuIds("newMenu") == QStringList({QStringLiteral("newServerTab"), QStringLiteral("newChannelTab"), QStringLiteral("newServerWindow"), QStringLiteral("newChannelWindow")}), "New submenu includes original window actions");
    verify(menuIds("serverMenu") == QStringList({QStringLiteral("disconnect"), QStringLiteral("reconnect"),
           QStringLiteral("joinChannel"), QStringLiteral("channelBrowserAction"), QStringLiteral("|"),
           QStringLiteral("away"), QStringLiteral("|"), QStringLiteral("serverMoreMenu")}), "Server menu matches original supported actions before extras");
    verify(menuIds("windowMenu") == QStringList({QStringLiteral("banManagerAction"), QStringLiteral("characterChartAction"), QStringLiteral("directChatsAction"), QStringLiteral("transfersAction"), QStringLiteral("friendsAction"), QStringLiteral("ignoreManagerAction"),
           QStringLiteral("addonsAction"), QStringLiteral("rawLogAction"), QStringLiteral("urlHistoryAction"), QStringLiteral("|"), QStringLiteral("resetMarker"), QStringLiteral("moveToMarker"), QStringLiteral("copyChat"),
           QStringLiteral("clearChat"), QStringLiteral("saveChat"), QStringLiteral("searchMenu"), QStringLiteral("|"),
           QStringLiteral("channelMenu"), QStringLiteral("navigationMenu"), QStringLiteral("selectChat")}), "Window menu restores tools and text actions in original order");
    verify(menuIds("searchMenu") == QStringList({QStringLiteral("find"), QStringLiteral("findNext"), QStringLiteral("findPrevious")}), "original Search submenu");
    verify(menuIds("settingsMenu") == QStringList({QStringLiteral("preferences"), QStringLiteral("|"), QStringLiteral("autoReplaceEditor"),
           QStringLiteral("ctcpEditor"), QStringLiteral("dialogButtonsEditor"), QStringLiteral("keyBindingsEditorAction"), QStringLiteral("textEventsEditor"), QStringLiteral("urlHandlersEditor"), QStringLiteral("commandsEditor"), QStringLiteral("userButtonsEditor"), QStringLiteral("userPopupEditor"), QStringLiteral("tabPopupEditor"), QStringLiteral("|"), QStringLiteral("userListSettingsMenu"), QStringLiteral("desktopSettingsMenu")}), "Qt settings are grouped beneath Settings");
    const QMap<QString, QString> originalLabels = {
        {QStringLiteral("networkList"), QStringLiteral("Network List")}, {QStringLiteral("closeConversation"), QStringLiteral("Close")},
        {QStringLiteral("showTopic"), QStringLiteral("Topic Bar")}, {QStringLiteral("joinChannel"), QStringLiteral("Join a Channel…")},
        {QStringLiteral("channelBrowserAction"), QStringLiteral("Channel List")}, {QStringLiteral("away"), QStringLiteral("Marked Away")},
        {QStringLiteral("preferences"), QStringLiteral("Preferences")}, {QStringLiteral("banManagerAction"), QStringLiteral("Ban List")},
        {QStringLiteral("rawLogAction"), QStringLiteral("Raw Log")}, {QStringLiteral("urlHistoryAction"), QStringLiteral("URL Grabber")},
        {QStringLiteral("clearChat"), QStringLiteral("Clear Text")}, {QStringLiteral("saveChat"), QStringLiteral("Save Text…")},
        {QStringLiteral("find"), QStringLiteral("Search Text…")}, {QStringLiteral("about"), QStringLiteral("About")}
    };
    for (auto it = originalLabels.cbegin(); it != originalLabels.cend(); ++it) {
        auto *item = action(it.key().toUtf8().constData());
        verify(item && QString(item->text()).remove(QLatin1Char('&')) == it.value(), "supported original action labels");
    }
    verify(action("networkList")->shortcut() == QKeySequence(QStringLiteral("Ctrl+S")) && action("saveChat")->shortcuts().isEmpty(),
           "Ctrl+S opens Network List rather than exporting chat");
    verify(action("findNext")->shortcut() == QKeySequence(QStringLiteral("Ctrl+G")) &&
           action("findPrevious")->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+G")), "original next/previous search shortcuts");
    verify(action("away")->isCheckable() && action("away")->shortcut() == QKeySequence(QStringLiteral("Alt+A")) &&
           action("nextUnread")->shortcut() == QKeySequence(QStringLiteral("Alt+`")), "away and unread shortcuts do not conflict");
    verify(action("copyChat")->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+C")) &&
           action("showUsers")->shortcut() == QKeySequence(QStringLiteral("Ctrl+F7")) &&
           action("quit")->shortcut() == QKeySequence(QStringLiteral("Ctrl+Q")), "original copy, user-list and quit shortcuts");
    QHash<QString, QAction *> shortcutOwners;
    for (auto *item : window->findChildren<QAction *>()) {
        for (const QKeySequence &key : item->shortcuts()) {
            if (key.isEmpty()) continue;
            const QString text = key.toString();
            verify(!shortcutOwners.contains(text) || shortcutOwners.value(text) == item, "no duplicate action shortcuts");
            shortcutOwners.insert(text, item);
        }
    }
    int keys[3];
    void *server = &keys[0], *channel = &keys[1], *query = &keys[2];
    engine.insert(server, {0, 1, 1, 0, 0, "tester", "irc.test"});
    engine.insert(channel, {1, 0, 1, 0, 0, "tester", "irc.test"});
    engine.insert(query, {0, 0, 1, 0, 0, "tester", "irc.test", 1});
    ModerationTestEngine::channels.insert(channel, {"#test", "+nt", "beI,k,l,imnt", "imnt", 3, 0, 0});
    qt_fe_new_session(server, "Server", 1);
    qt_fe_new_session(channel, "#test", 0);
    qt_fe_new_session(query, "Alice", 0);
    auto setNative = [&](const char *key, const char *value) { for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == key) qstrncpy(row.value, value, sizeof(row.value)); };
    auto *switcher = window->findChild<QTreeWidget *>();
    setNative("gui_tab_icons", "1"); setNative("gui_tab_small", "0"); qt_fe_gui_control(server, 8, 0);
    const QStringList treeKinds{QStringLiteral("server"), QStringLiteral("channel"), QStringLiteral("dialog")};
    for (int i = 0; i < 3; ++i) verify(!switcher->topLevelItem(i)->icon(0).isNull() && switcher->topLevelItem(i)->icon(0).pixmap(16,16).toImage() == QIcon(QStringLiteral(":/zoitechat/tree_%1.png").arg(treeKinds[i])).pixmap(16,16).toImage(), "native tree icons match server channel and dialog session roles");
    const auto joinedIconInfo = engine[channel]; engine[channel].channel = 0; engine[channel].channel_session = 1; qt_fe_gui_control(server, 8, 0);
    verify(switcher->topLevelItem(1)->icon(0).pixmap(16,16).toImage() == QIcon(QStringLiteral(":/zoitechat/tree_channel.png")).pixmap(16,16).toImage(), "unjoined channel containers retain channel icon");
    engine[channel] = joinedIconInfo;
    setNative("gui_tab_icons", "0"); qt_fe_gui_control(server, 8, 0);
    verify(switcher->topLevelItem(0)->icon(0).isNull() && switcher->topLevelItem(1)->icon(0).isNull(), "native icon toggle removes existing tree icons");
    const qreal normalSwitcherSize = switcher->font().pointSizeF();
    const qreal normalTabsSize = window->findChild<QTabBar *>(QStringLiteral("conversationTabs"))->font().pointSizeF();
    setNative("gui_tab_small", "1"); qt_fe_gui_control(server, 8, 0);
    verify(qAbs(switcher->font().pointSizeF() - normalSwitcherSize * 0.8333333333333) < 0.02 && qAbs(window->findChild<QTabBar *>(QStringLiteral("conversationTabs"))->font().pointSizeF() - normalTabsSize * 0.8333333333333) < 0.02, "native Small text scale applies to tree and tabs");
    qt_fe_activity(query, 1);
    verify(switcher->topLevelItem(2)->font(0).bold() && qAbs(switcher->topLevelItem(2)->font(0).pointSizeF() - switcher->font().pointSizeF()) < 0.02, "unread styling preserves configured smaller font size");
    qt_fe_select_session(query);
    verify(!switcher->topLevelItem(2)->font(0).bold() && qAbs(switcher->topLevelItem(2)->font(0).pointSizeF() - switcher->font().pointSizeF()) < 0.02, "reading conversation clears bold without resetting smaller font");
    qt_fe_select_session(server);
    setNative("gui_tab_small", "2"); qt_fe_gui_control(server, 8, 0);
    verify(qAbs(switcher->font().pointSizeF() - normalSwitcherSize * 0.6944444444444) < 0.02, "native Extra small scale is supported");
    setNative("gui_tab_small", "0"); setNative("gui_ulist_style", "1"); qt_fe_gui_control(server, 8, 0);
    verify(switcher->font() == window->findChild<QTextBrowser *>()->font() && switcher->palette().color(QPalette::Text) == window->findChild<QTextBrowser *>()->palette().color(QPalette::Text), "native user-list style shares chat font and colors with tree");
    setNative("gui_ulist_style", "0"); qt_fe_gui_control(server, 8, 0);

    auto *trayDesktop = static_cast<DesktopIntegration *>(window->findChild<QObject *>(QStringLiteral("desktopIntegration")));
    const int beforeTrayAway = commands.size();
    trayDesktop->hiddenChanged(true);
    verify(commands.size() == beforeTrayAway, "disabled native tray-away option leaves server status alone");
    setNative("gui_tray_away", "1"); qt_fe_gui_control(server, 8, 0);
    trayDesktop->hiddenChanged(true);
    verify(commands.size() == beforeTrayAway + 1 && commands.last().line == QStringLiteral("/allserv away") && commands.last().session == server, "tray hide routes native ALLSERV AWAY through explicit command parser");
    trayDesktop->hiddenChanged(false);
    verify(commands.size() == beforeTrayAway + 2 && commands.last().line == QStringLiteral("/allserv back"), "tray restore routes native ALLSERV BACK");
    setNative("gui_tray_away", "0"); qt_fe_gui_control(server, 8, 0);
    trayDesktop->hiddenChanged(false);
    verify(commands.size() == beforeTrayAway + 2, "live disabled away preference suppresses later restore command");
    auto *trayMenu = window->findChild<QSystemTrayIcon *>()->contextMenu();
    QMetaObject::invokeMethod(trayMenu, "aboutToShow", Qt::DirectConnection);
    auto *trayAwayAction = trayMenu->findChild<QAction *>(QStringLiteral("trayAway"));
    auto *trayBackAction = trayMenu->findChild<QAction *>(QStringLiteral("trayBack"));
    verify(trayAwayAction->isEnabled() && !trayBackAction->isEnabled(), "tray menu enables Away for connected back servers");
    trayAwayAction->trigger();
    verify(commands.last().line == QStringLiteral("/allserv away"), "manual tray Away works with automatic away disabled");
    const auto beforeTrayStates = engine;
    engine[channel].away = true;
    QMetaObject::invokeMethod(trayMenu, "aboutToShow", Qt::DirectConnection);
    verify(trayAwayAction->isEnabled() && trayBackAction->isEnabled(), "mixed server away state enables both tray actions");
    for (auto it = engine.begin(); it != engine.end(); ++it) it->connected = false;
    QMetaObject::invokeMethod(trayMenu, "aboutToShow", Qt::DirectConnection);
    verify(!trayAwayAction->isEnabled() && !trayBackAction->isEnabled(), "disconnected servers disable both tray away actions");
    engine = beforeTrayStates;
    for (auto it = engine.begin(); it != engine.end(); ++it) it->away = true;
    QMetaObject::invokeMethod(trayMenu, "aboutToShow", Qt::DirectConnection);
    verify(!trayAwayAction->isEnabled() && trayBackAction->isEnabled(), "tray menu enables Back when all connected servers are away");
    trayBackAction->trigger(); verify(commands.last().line == QStringLiteral("/allserv back"), "manual tray Back uses native all-server command");
    engine = beforeTrayStates;


    setNative("gui_tray", "0"); setNative("gui_tray_minimize", "1"); qt_fe_gui_control(server, 8, 0);
    window->showMinimized(); QCoreApplication::processEvents();
    verify(!action("trayEnabled")->isChecked() && window->isMinimized() && window->isVisible(), "native minimize-to-tray falls back to reachable iconified window when tray disabled");
    window->showNormal(); setNative("gui_tray", "1"); setNative("gui_tray_minimize", "0"); qt_fe_gui_control(server, 8, 0);

    int focusKey;
    setNative("gui_tab_trunc", "3");
    for (int mode = 0; mode < 3; ++mode) for (bool requested : {false, true}) {
        setNative("gui_tab_newtofront", QByteArray::number(mode).constData()); qt_fe_gui_control(server, 8, 0); qt_fe_select_session(server);
        engine.insert(&focusKey, engine.value(query)); qt_fe_new_session(&focusKey, "😀abcdef", requested);
        verify(activated == ((mode == 1 || (mode == 2 && requested)) ? &focusKey : server), "native new-conversation focus policy distinguishes Never Always and Only requested");
        verify(switcher->topLevelItem(3)->text(0) == QStringLiteral("😀ab..") && switcher->topLevelItem(3)->toolTip(0).contains(QStringLiteral("😀abcdef")), "switcher truncation counts Unicode characters and retains full tooltip");
        qt_fe_remove_session(&focusKey); engine.remove(&focusKey);
    }
    setNative("gui_tab_newtofront", "2"); setNative("gui_tab_trunc", "20"); setNative("gui_tab_scrollchans", "1");
    qt_fe_gui_control(server, 8, 0); qt_fe_select_session(server);
    QWheelEvent switchWheel(QPointF(5, 5), QPointF(switcher->viewport()->mapToGlobal(QPoint(5,5))), QPoint(), QPoint(0,-120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(switcher->viewport(), &switchWheel);
    verify(activated == channel, "native wheel option switches to next conversation");
    setNative("gui_tab_scrollchans", "0"); qt_fe_gui_control(server, 8, 0); qt_fe_select_session(server);
    QApplication::sendEvent(switcher->viewport(), &switchWheel);
    verify(activated == server, "disabled wheel switching leaves active conversation unchanged");
    engine.insert(&focusKey, engine.value(query)); qt_fe_new_session(&focusKey, "Middle close", false);
    const QPoint closePoint = switcher->visualItemRect(switcher->topLevelItem(3)).center();
    QMouseEvent middleClose(QEvent::MouseButtonRelease, QPointF(closePoint), QPointF(switcher->viewport()->mapToGlobal(closePoint)), Qt::MiddleButton, Qt::NoButton, Qt::NoModifier);
    const int beforeMiddleClose = commands.size();
    setNative("gui_tab_middleclose", "0"); qt_fe_gui_control(server, 8, 0);
    QApplication::sendEvent(switcher->viewport(), &middleClose);
    verify(commands.size() == beforeMiddleClose, "disabled middle-click close does not submit command");
    setNative("gui_tab_middleclose", "1"); qt_fe_gui_control(server, 8, 0); qt_fe_select_session(server);
    QMouseEvent middlePress(QEvent::MouseButtonPress, QPointF(closePoint), QPointF(switcher->viewport()->mapToGlobal(closePoint)), Qt::MiddleButton, Qt::MiddleButton, Qt::NoModifier);
    QApplication::sendEvent(switcher->viewport(), &middlePress);
    verify(activated == server, "middle press does not focus background conversation before closing");
    QApplication::sendEvent(switcher->viewport(), &middleClose);
    verify(commands.size() == beforeMiddleClose + 1 && commands.last().session == &focusKey && commands.last().line == QStringLiteral("/close") && !engine.contains(&focusKey), "middle click closes clicked conversation through native command path");
    qt_fe_select_session(server);
    verify(!action("editTopic")->isEnabled() && action("disconnect")->isEnabled(), "server action enablement");
    trigger("nextConversation");
    verify(activated == channel && action("editTopic")->isEnabled(), "ordered next conversation and channel actions");
    auto *modeBar = window->findChild<QWidget *>(QStringLiteral("channelModes"));
    auto *topicMode = window->findChild<QToolButton *>(QStringLiteral("channelMode_t"));
    auto *moderatedMode = window->findChild<QToolButton *>(QStringLiteral("channelMode_m"));
    auto *colorMode = window->findChild<QToolButton *>(QStringLiteral("channelMode_c"));
    verify(!modeBar->isHidden() && topicMode->isChecked() && !moderatedMode->isChecked() && colorMode->isHidden(), "mode bar shows confirmed modes and hides unadvertised controls");
    moderatedMode->click();
    verify(!moderatedMode->isChecked() && ModerationTestEngine::changes.last().session == channel && ModerationTestEngine::changes.last().mode == 'm' && ModerationTestEngine::changes.last().enable, "mode toggle requests change without optimistic checked state");
    const int modeRequests = ModerationTestEngine::changes.size();
    ModerationTestEngine::channels[channel].modes = "+klmnt secret-key 25";
    ModerationTestEngine::channels[channel].key_set = 1; ModerationTestEngine::channels[channel].limit = 25; qt_fe_connection_changed();
    verify(moderatedMode->isChecked() && ModerationTestEngine::changes.size() == modeRequests && window->findChild<QToolButton *>(QStringLiteral("channelKeyStatus"))->text() == QStringLiteral("Key: set") && window->findChild<QToolButton *>(QStringLiteral("channelLimitStatus"))->text() == QStringLiteral("Limit: 25"), "confirmed updates show mode, key presence and user limit without sending feedback");
    for (auto *button : modeBar->findChildren<QToolButton *>()) verify(!button->text().contains(QStringLiteral("secret-key")), "inline controls do not display channel key");
    qt_fe_select_session(query); verify(modeBar->isHidden(), "mode controls hide for private conversations");
    qt_fe_select_session(channel);
    ConversationOptionsTestEngine::states[channel] = {"#test", "Network", 1, {2,2,2,2,2,2,2,2}};
    qt_fe_connection_changed(); trigger("conversationOptionsAction");
    auto *conversationOptions = window->findChild<QDialog *>(QStringLiteral("conversationOptions"));
    auto *hideJoins = conversationOptions->findChild<QComboBox *>(QStringLiteral("conversationOption4")); hideJoins->setCurrentIndex(hideJoins->findData(1));
    conversationOptions->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    verify(ConversationOptionsTestEngine::states[channel].values[4] == 1, "conversation editor applies to original channel state");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    trigger("conversationOptionsAction"); conversationOptions = window->findChild<QDialog *>(QStringLiteral("conversationOptions"));
    const int optionsApplied = ConversationOptionsTestEngine::applied; qt_fe_users_clear(channel);
    conversationOptions->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    verify(ConversationOptionsTestEngine::applied == optionsApplied && conversationOptions->isVisible(), "membership replacement invalidates a stale conversation editor");
    conversationOptions->reject(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    auto *connectionStatus = window->findChild<QLabel *>(QStringLiteral("connectionStatus"));
    auto *lagStatus = window->findChild<QLabel *>(QStringLiteral("lagStatus"));
    auto *queueStatus = window->findChild<QLabel *>(QStringLiteral("queueStatus"));
    auto *connectionProgress = window->findChild<QProgressBar *>(QStringLiteral("connectionProgress"));
    const auto originalChannelInfo = engine[channel];
    engine[channel].lag_ms = 230; engine[channel].queued_bytes = 2048; qt_fe_connection_changed();
    auto *lagMeter = window->findChild<QProgressBar *>(QStringLiteral("lagMeter"));
    auto *queueMeter = window->findChild<QProgressBar *>(QStringLiteral("queueMeter"));
    for (int mode = 0; mode < 4; ++mode) {
        for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_lagometer" || QByteArray(row.key) == "gui_throttlemeter") qstrncpy(row.value, QByteArray::number(mode).constData(), sizeof(row.value));
        qt_fe_gui_control(channel, 8, 0);
        verify(lagStatus->isHidden() == !(mode & 2) && queueStatus->isHidden() == !(mode & 2) && lagMeter->isHidden() == !(mode & 1) && queueMeter->isHidden() == !(mode & 1), "native meter modes independently control graphical and text displays");
        verify(lagMeter->value() == 230 && queueMeter->value() == 1024 && queueMeter->toolTip().contains(QStringLiteral("2048")), "meters use native one-second and 1024-byte scales and retain exact tooltip values");
    }
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_lagometer" || QByteArray(row.key) == "gui_throttlemeter") qstrncpy(row.value, "2", sizeof(row.value));
    qt_fe_gui_control(channel, 8, 0);

    verify(lagStatus->text().contains(QStringLiteral("0.2")) && queueStatus->text().contains(QStringLiteral("2048")) && action("flushQueue")->isEnabled(), "status reads latency and queued bytes from active server");
    trigger("flushQueue");
    verify(commands.last().session == channel && commands.last().line == QStringLiteral("/flushq") && !action("flushQueue")->isEnabled(), "queue clear routes through active core session and disables after drain");
    engine[channel].lag_pending = true; engine[channel].lag_ms = 30000; engine[channel].registering = true; qt_fe_connection_changed();
    verify(lagStatus->text().contains(QStringLiteral("≥")) && connectionStatus->text().contains(QStringLiteral("Registering")) && !connectionProgress->isHidden(), "pending latency and registration progress visible");
    engine[channel].connected = false; engine[channel].registering = false; engine[channel].reconnect_wait = true; qt_fe_connection_changed();
    verify(connectionStatus->text().contains(QStringLiteral("Waiting to reconnect")) && action("disconnect")->isEnabled() && lagStatus->isHidden(), "reconnect wait remains cancellable and hides stale latency");
    qt_fe_select_session(query);
    verify(!connectionStatus->text().contains(QStringLiteral("Waiting to reconnect")) && !action("flushQueue")->isEnabled(), "switching servers does not retain queue or reconnect state");
    engine[channel] = originalChannelInfo; qt_fe_select_session(channel); qt_fe_connection_changed();
    auto *input = window->findChild<QLineEdit *>(QStringLiteral("messageInput"));
    trigger("characterChartAction");
    auto *chart = window->findChild<QDialog *>(QStringLiteral("characterChart"));
    chart->findChild<QLineEdit *>(QStringLiteral("characterCode"))->setText(QStringLiteral("U+1F600"));
    chart->findChild<QPushButton *>(QStringLiteral("findCharacter"))->click();
    qt_fe_select_session(query); qt_fe_set_input(query, "replace"); input->selectAll();
    const int beforeCharacterInsert = commands.size();
    chart->findChild<QPushButton *>(QStringLiteral("insertCharacter"))->click();
    verify(QString::fromUtf8(qt_fe_get_input(query)) == QString::fromUtf8("😀") && commands.size() == beforeCharacterInsert, "character chart inserts into current draft after switching tabs without sending");
    chart->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_set_input(query, ""); qt_fe_select_session(channel);
    CustomizationTestEngine::lists[QT_CONFIG_REPLACE] = {{QStringLiteral("teh"), QStringLiteral("the")}};
    qt_fe_set_input(channel, "teh");
    QKeyEvent corrected(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); QApplication::sendEvent(input, &corrected);
    verify(commands.last().line == QStringLiteral("the") && input->text().isEmpty(), "saved replacement reaches actual Enter command routing");
    qt_fe_set_input(channel, "aé😀z");
    qt_fe_set_cursor(channel, 0, 3);
    verify(qt_fe_get_cursor(channel) == 3 && input->cursorPosition() == 4, "cursor counts Unicode codepoints");
    qt_fe_set_cursor(channel, 1, -1);
    verify(qt_fe_get_cursor(channel) == 2, "relative cursor movement");
    qt_fe_set_cursor(channel, 0, -1);
    verify(qt_fe_get_cursor(channel) == 4, "negative absolute cursor means end");
    trigger("nextConversation");
    qt_fe_set_input(query, "query draft");
    qt_fe_set_cursor(channel, 0, 1);
    trigger("previousConversation");
    verify(input->text() == QString::fromUtf8("aé😀z") && qt_fe_get_cursor(channel) == 1,
           "inactive draft and cursor restored");
    qt_fe_activity(query, 3);
    qt_fe_activity(query, 1);
    auto *tree = window->findChild<QTreeWidget *>();
    verify(tree->topLevelItem(2)->font(0).bold(), "unread row is bold");
    nativeActivityQueue = {query};
    trigger("nextUnread");
    verify(activated == query && !tree->topLevelItem(2)->font(0).bold(), "unread navigation clears marker");
    qt_fe_select_session(server);
    qt_fe_activity(channel, 3); qt_fe_activity(query, 2);
    nativeActivityQueue = {query, channel};
    trigger("nextUnread");
    verify(activated == query, "unread navigation honors native query priority over tab order and highlight color");
    trigger("nextUnread");
    verify(activated == channel, "unread navigation follows the remaining native queue entry");
    trigger("nextUnread");
    verify(activated == channel, "empty native activity queue leaves the selected conversation in place");
    {
        QWidget otherWindow;
        otherWindow.show(); otherWindow.activateWindow(); QCoreApplication::processEvents();
        verify(!window->isActiveWindow(), "activity test backgrounds the main window");
        qt_fe_activity(channel, 3);
        verify(tree->currentItem()->font(0).bold(), "selected background conversation can receive unread activity");
        nativeActivityQueue = {channel};
        trigger("nextUnread");
        verify(activated == channel && !tree->currentItem()->font(0).bold(), "selecting the already displayed native activity target clears its unread styling");
        otherWindow.hide(); window->activateWindow(); QCoreApplication::processEvents();
    }
    qt_fe_user_add(channel, "Alice", "@", 0);
    qt_fe_user_add(channel, "Bob", "+", 0);
    auto *nickList = static_cast<NickList *>(window->findChild<QListWidget *>());
    qt_fe_user_details(channel, "Alice", "alice@host", "<b>Literal</b>", "alice-account", "irc.test", 0, 0, 0);
    qt_fe_user_details(channel, "Bob", "bob@host", "Bob", "bob-account", "irc.test", 1, 0, 3);
    QApplication::processEvents();
    verify(window->findChild<QLabel *>(QStringLiteral("userCount"))->text() == QStringLiteral("2/2 users · 1 away"), "user count includes away metadata");
    auto *userFilter = window->findChild<QLineEdit *>(QStringLiteral("userFilter"));
    userFilter->setText(QStringLiteral("bob-account"));
    verify(nickList->visibleCount() == 1, "user filter searches account metadata");
    userFilter->clear();
    const int beforeUserActivation = CustomizationTestEngine::executed.size();
    nickList->activate(QStringLiteral("Alice"));
    verify(CustomizationTestEngine::executed.size() == beforeUserActivation + 1 && CustomizationTestEngine::executed.last() == QStringLiteral("QUERY %s") && CustomizationTestEngine::lastSession == channel && CustomizationTestEngine::lastTarget == QStringLiteral("Alice"), "native activation command uses selected nickname and originating session");
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_ulist_doubleclick") qstrncpy(row.value, "", sizeof(row.value));
    nickList->activate(QStringLiteral("Alice"));
    verify(CustomizationTestEngine::executed.size() == beforeUserActivation + 1, "empty native activation command disables default action");
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_ulist_doubleclick") qstrncpy(row.value, "WHOIS %s", sizeof(row.value));
    nickList->activate(QStringLiteral("Alice"));
    verify(CustomizationTestEngine::executed.last() == QStringLiteral("WHOIS %s"), "activation reads changed native command without hardcoded query");
    nickList->userAction(NickAction::Details, {QStringLiteral("Alice")});
    auto *details = window->findChild<QDialog *>(QStringLiteral("userDetails"));
    verify(details->findChild<QLabel *>(QStringLiteral("detailRealname"))->text() == QStringLiteral("<b>Literal</b>") &&
           details->findChild<QLabel *>(QStringLiteral("detailRealname"))->textFormat() == Qt::PlainText, "user details renders known real name literally");
    details->findChild<QPushButton *>(QStringLiteral("detailsWhois"))->click();
    verify(commands.last().line == QStringLiteral("/whois Alice"), "details WHOIS uses existing command parser");
    qt_fe_user_remove(channel, "Alice");
    const int beforeStaleWhois = commands.size();
    details->findChild<QPushButton *>(QStringLiteral("detailsWhois"))->click();
    verify(commands.size() == beforeStaleWhois, "departed user cannot receive a details action");
    details->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_user_add(channel, "Alice", "@", 0);
    int beforeUserAction = commands.size();
    nickList->userAction(NickAction::Voice, {QStringLiteral("Alice"), QStringLiteral("Bob")});
    verify(commands.size() == beforeUserAction + 2 && commands.last().line == QStringLiteral("/voice Bob") &&
           commands.last().session == channel, "multi-user voice commands stay on original channel");
    nickList->userAction(NickAction::Operator, {QStringLiteral("Alice")});
    verify(commands.last().line == QStringLiteral("/op Alice"), "operator action");
    nickList->userAction(NickAction::Deoperator, {QStringLiteral("Alice")});
    verify(commands.last().line == QStringLiteral("/deop Alice"), "remove operator action");
    nickList->userAction(NickAction::Devoice, {QStringLiteral("Bob")});
    verify(commands.last().line == QStringLiteral("/devoice Bob"), "remove voice action");
    nickList->userAction(NickAction::Copy, {QStringLiteral("Alice"), QStringLiteral("Bob")});
    verify(QApplication::clipboard()->text() == QStringLiteral("Alice Bob"), "copy raw nicknames");
    qt_fe_set_input(channel, "hello ");
    nickList->userAction(NickAction::Insert, {QStringLiteral("Alice")});
    verify(input->text() == QStringLiteral("hello Alice "), "insert nicknames at input cursor");
    QTimer::singleShot(0, window, [&]() {
        qt_fe_user_remove(channel, "Bob");
        auto *dialog = window->findChild<QInputDialog *>(); dialog->setTextValue(QStringLiteral("test reason")); dialog->accept();
    });
    beforeUserAction = commands.size();
    nickList->userAction(NickAction::Kick, {QStringLiteral("Alice"), QStringLiteral("Bob")});
    verify(commands.size() == beforeUserAction + 1 && commands.last().line == QStringLiteral("/kick Alice test reason"),
           "kick revalidates membership after prompt");
    QTimer::singleShot(0, window, [&]() {
        qt_fe_select_session(query);
        auto *dialog = window->findChild<QInputDialog *>(); dialog->setTextValue(QStringLiteral("original channel")); dialog->accept();
    });
    nickList->userAction(NickAction::Kick, {QStringLiteral("Alice")});
    verify(commands.last().session == channel && commands.last().line == QStringLiteral("/kick Alice original channel") &&
           activated == query, "prompt keeps original command target and restores visible engine context");
    qt_fe_select_session(channel);
    nickList->userAction(NickAction::Ban, {QStringLiteral("Alice")});
    verify(commands.last().line == QStringLiteral("/ban Alice"), "ban delegates host-mask generation to core");
    QTimer::singleShot(0, window, [&]() { window->findChild<QInputDialog *>()->accept(); });
    nickList->userAction(NickAction::KickBan, {QStringLiteral("Alice")});
    verify(commands.last().line == QStringLiteral("/kickban Alice Requested"), "kick-and-ban delegates to core");
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QInputDialog *>(); dialog->setTextValue(QStringLiteral("#destination")); dialog->accept();
    });
    nickList->userAction(NickAction::Invite, {QStringLiteral("Alice")});
    verify(commands.last().line == QStringLiteral("/invite Alice #destination"), "invite selected user to another channel");
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QInputDialog *>(); dialog->setTextValue(QStringLiteral("Carol")); dialog->accept();
    });
    trigger("inviteUserAction");
    verify(commands.last().line == QStringLiteral("/invite Carol"), "invite new user to current channel");
    QTimer::singleShot(0, window, [&]() { qt_fe_users_clear(channel); window->findChild<QInputDialog *>()->accept(); });
    beforeUserAction = commands.size();
    nickList->userAction(NickAction::Kick, {QStringLiteral("Alice")});
    verify(commands.size() == beforeUserAction, "channel membership reset invalidates pending action");
    trigger("newServerTab");
    verify(FriendsTestEngine::newTab == 1, "New Server Tab calls core without connecting");
    trigger("newChannelTab");
    verify(FriendsTestEngine::newTab == 0 && FriendsTestEngine::newTabSource == channel, "New Channel Tab uses current server session");
    qt_fe_select_session(channel);
    const auto tabOrderBeforeDefaultMove = tree->topLevelItem(1)->text(0);
    QKeyEvent defaultMove(QEvent::KeyPress, Qt::Key_Right, Qt::AltModifier); QApplication::sendEvent(input, &defaultMove);
    verify(tree->topLevelItem(2)->text(0) == tabOrderBeforeDefaultMove, "default Alt+Right moves the current conversation");
    QKeyEvent defaultMoveBack(QEvent::KeyPress, Qt::Key_Left, Qt::AltModifier); QApplication::sendEvent(input, &defaultMoveBack);
    verify(tree->topLevelItem(1)->text(0) == tabOrderBeforeDefaultMove, "default Alt+Left restores conversation order");
    input->setText(QStringLiteral("format")); input->selectAll(); QKeyEvent defaultBold(QEvent::KeyPress, Qt::Key_B, Qt::ControlModifier); QApplication::sendEvent(input, &defaultBold);
    verify(input->text() == QChar(2) + QStringLiteral("format") + QChar(2), "default native formatting binding preserves selected text");
    input->clear();
    auto *pasteInput = static_cast<ChatInput *>(input);
    qt_fe_set_input(channel, "draft ");
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QDialog *>(QStringLiteral("pastePreview"));
        verify(!dialog->findChild<QCheckBox *>(QStringLiteral("pasteCommands"))->isChecked(), "paste defaults to literal mode");
        qt_fe_select_session(query); dialog->accept();
    });
    int beforePaste = commands.size();
    pasteInput->pasteText(QStringLiteral("first\n/quit"));
    auto *queue = window->findChild<QTimer *>(QStringLiteral("pasteQueue"));
    verify(queue != nullptr && commands.size() == beforePaste, "accepted paste is queued rather than sent during preview");
    if (queue) { QMetaObject::invokeMethod(queue, "timeout", Qt::DirectConnection); QMetaObject::invokeMethod(queue, "timeout", Qt::DirectConnection); }
    verify(commands.size() == beforePaste + 2 && commands.last().session == channel && commands.last().line == QStringLiteral("/quit") &&
           commands.last().literal && activated == query, "slash paste stays literal on original target across tab switch");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_select_session(channel); qt_fe_set_input(channel, "keep draft");
    QTimer::singleShot(0, window, [&]() { window->findChild<QDialog *>(QStringLiteral("pastePreview"))->reject(); });
    pasteInput->pasteText(QStringLiteral("one\ntwo"));
    verify(input->text() == QStringLiteral("keep draft") && commands.size() == beforePaste + 2, "cancel preserves draft and sends nothing");
    qt_fe_set_input(channel, "");
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QDialog *>(QStringLiteral("pastePreview"));
        dialog->findChild<QCheckBox *>(QStringLiteral("pasteCommands"))->setChecked(true); dialog->accept();
    });
    pasteInput->pasteText(QStringLiteral("/echo command\nsecond")); queue = window->findChild<QTimer *>(QStringLiteral("pasteQueue"));
    beforePaste = commands.size();
    if (queue) QMetaObject::invokeMethod(queue, "timeout", Qt::DirectConnection);
    verify(commands.size() == beforePaste + 1 && commands.last().line == QStringLiteral("/echo command") && !commands.last().literal,
           "explicit command mode uses command parser");
    qt_fe_users_clear(channel);
    if (queue) QMetaObject::invokeMethod(queue, "timeout", Qt::DirectConnection);
    verify(commands.size() == beforePaste + 1, "membership reset stops remaining paste queue");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_topic(channel, "<b>literal topic</b>");
    auto *topicDisplay = window->findChild<QLineEdit *>(QStringLiteral("topicDisplay"));
    QApplication::processEvents();
    verify(topicDisplay && !topicDisplay->isReadOnly() && topicDisplay->text() == QStringLiteral("<b>literal topic</b>"),
           "joined channel has a selectable editable topic field with literal text");
    verify(topicDisplay->isVisible() && topicDisplay->height() >= topicDisplay->sizeHint().height(),
           "topic field retains a full-height visible row");
    const QString longTopic = QStringLiteral("Topic start — ") + QString(2000, QLatin1Char('x'));
    qt_fe_topic(channel, longTopic.toUtf8().constData());
    QApplication::processEvents();
    verify(topicDisplay->text() == longTopic && topicDisplay->cursorPosition() == 0 &&
           topicDisplay->height() >= topicDisplay->sizeHint().height(), "long topic starts visibly and retains row height");
    const QString formattedTopic = QStringLiteral("  ") + QChar(3) + QStringLiteral("04red ") + QChar(2) + QStringLiteral("bold <b>literal</b>  ");
    {
        TopicEntry sample;
        sample.resize(500, 40);
        QPalette palette; palette.setColor(QPalette::Base, Qt::white); palette.setColor(QPalette::Text, Qt::black);
        sample.setPalette(palette);
        const QString raw = QChar(4) + QStringLiteral("FF0000Colored العربية <b>literal</b>");
        sample.setText(raw);
        QImage image(sample.size(), QImage::Format_ARGB32); image.fill(Qt::white);
        sample.render(&image);
        int red = 0;
        for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.pixelColor(x, y);
            if (pixel.red() > 180 && pixel.green() < 80 && pixel.blue() < 80) ++red;
        }
        verify(red > 10 && sample.text() == raw, "unfocused topic paints IRC true-color text without modifying editable protocol text");
        sample.selectAll(); sample.copy();
        verify(QApplication::clipboard()->text() == raw, "topic selection copies original attributes and literal markup");
    }
    qt_fe_topic(channel, formattedTopic.toUtf8().constData());
    verify(topicDisplay->text() == formattedTopic, "topic retains protocol attributes and surrounding whitespace for editing");
    topicDisplay->setSelection(2, 3);
    qt_fe_topic(channel, formattedTopic.toUtf8().constData());
    verify(topicDisplay->hasSelectedText(), "unchanged confirmed topic preserves the selection");
    topicDisplay->deselect();
    topicDisplay->returnPressed();
    verify(sentTopic == formattedTopic, "submitting a formatted topic preserves its controls");
    for (auto &pref : PreferencesTestEngine::rows) if (QByteArray(pref.key) == "text_stripcolor_topic") qstrncpy(pref.value, "1", sizeof(pref.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(topicDisplay->text() == QStringLiteral("  red bold <b>literal</b>  "), "native topic stripping preserves literal markup and whitespace");
    for (auto &pref : PreferencesTestEngine::rows) if (QByteArray(pref.key) == "text_stripcolor_topic") qstrncpy(pref.value, "0", sizeof(pref.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(topicDisplay->text() == formattedTopic, "disabling topic stripping restores retained protocol text");
    qt_fe_topic(channel, longTopic.toUtf8().constData());
    qt_fe_topic(query, "Background topic");
    verify(topicDisplay->text() == longTopic, "background topic does not replace current topic");
    qt_fe_select_session(query);
    verify(topicDisplay->text() == QStringLiteral("Background topic"), "switching shows conversation's own topic");
    qt_fe_select_session(channel);
    verify(topicDisplay->text() == longTopic, "switching back restores channel topic");
    trigger("showTopic");
    qt_fe_topic(channel, "Updated while hidden");
    trigger("showTopic");
    QApplication::processEvents();
    verify(topicDisplay->isVisible() && topicDisplay->text() == QStringLiteral("Updated while hidden"),
           "showing topic bar displays latest hidden update");
    qt_fe_topic(channel, "");
    verify(topicDisplay->text().isEmpty() && !topicDisplay->placeholderText().isEmpty(), "empty topic has a visible placeholder");
    topicDisplay->setText(QStringLiteral("#topic entered directly in the bar"));
    topicDisplay->returnPressed();
    verify(sentTopic == QStringLiteral("#topic entered directly in the bar") && topicDisplay->text().isEmpty(),
           "Enter submits literal topic and preserves confirmed display while awaiting server");
    qt_fe_topic(channel, "#topic entered directly in the bar");
    verify(topicDisplay->text() == sentTopic, "server-confirmed topic appears in editable bar");
    topicDisplay->clear(); topicDisplay->returnPressed();
    verify(sentTopic.isEmpty() && topicDisplay->text() == QStringLiteral("#topic entered directly in the bar"),
           "empty topic submission clears through core without assuming success");
    qt_fe_select_session(query);
    verify(topicDisplay->isReadOnly(), "private conversation topic/host field is read-only");
    sentTopic = QStringLiteral("unchanged"); topicDisplay->returnPressed();
    verify(sentTopic == QStringLiteral("unchanged"), "nonchannel cannot submit a topic change");
    qt_fe_select_session(channel);
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QInputDialog *>();
        dialog->setTextValue(QStringLiteral("#topic beginning with channel syntax"));
        dialog->accept();
    });
    trigger("editTopic");
    verify(sentTopic == QStringLiteral("#topic beginning with channel syntax"), "topic uses literal adapter instead of command parsing");
    QTimer::singleShot(0, window, [&]() { window->findChild<QInputDialog *>()->setTextValue(QString());
                                         window->findChild<QInputDialog *>()->accept(); });
    trigger("editTopic");
    verify(sentTopic.isEmpty(), "topic editor allows clearing");
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QInputDialog *>();
        dialog->setTextValue(QStringLiteral("newnick")); dialog->accept();
    });
    trigger("nickname");
    verify(commands.last().session == channel && commands.last().line == QStringLiteral("/nick newnick"), "nickname action");
    QTimer::singleShot(0, window, [&]() { window->findChild<QInputDialog *>()->accept(); });
    trigger("awayReason");
    verify(commands.last().line == QStringLiteral("/away Away") && action("away")->isChecked(), "away message action updates marked-away state");
    trigger("away");
    verify(commands.last().line == QStringLiteral("/back") && !action("away")->isChecked(), "marked-away action returns to back state");
    trigger("away");
    verify(commands.last().line == QStringLiteral("/away") && action("away")->isChecked(), "marked-away toggle uses core default reason without a dialog");
    trigger("away");

    auto *chat = window->findChild<QTextBrowser *>(QStringLiteral("chat"));
    const QString beforeLive = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    qt_fe_print(channel, "live message", 0);
    const QString afterLive = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    verify(chat->document()->lastBlock().text() == QStringLiteral("[%1] live message").arg(beforeLive) ||
           chat->document()->lastBlock().text() == QStringLiteral("[%1] live message").arg(afterLive),
           "enabled timestamps show on live messages without server time");
    qt_fe_print(channel, "Alpha beta ALPHA café", 1);
    trigger("resetMarker");
    const QString markedLine = chat->document()->lastBlock().text();
    qt_fe_print(channel, "after marker", 0); trigger("moveToMarker");
    verify(chat->textCursor().block().text() == markedLine && chat->extraSelections().size() == 1, "marker navigation returns to retained marker block");
    chat->moveCursor(QTextCursor::Start);
    trigger("find");
    auto *search = window->findChild<QLineEdit *>(QStringLiteral("chatSearch"));
    search->setText(QStringLiteral("alpha"));
    trigger("findNext");
    verify(chat->textCursor().selectedText() == QStringLiteral("Alpha"), "case insensitive search");
    trigger("findNext");
    verify(chat->textCursor().selectedText() == QStringLiteral("ALPHA"), "find next");
    trigger("findNext");
    verify(window->findChild<QLabel *>(QStringLiteral("searchResult"))->text() == QStringLiteral("Wrapped"), "search wraps");
    trigger("findPrevious");
    verify(chat->textCursor().selectedText() == QStringLiteral("ALPHA"), "find previous wraps");
    window->findChild<QCheckBox *>(QStringLiteral("matchCase"))->setChecked(true);
    trigger("findNext");
    verify(window->findChild<QLabel *>(QStringLiteral("searchResult"))->text() == QStringLiteral("No match"), "case sensitive search miss");
    search->setText(QStringLiteral("café")); trigger("findNext");
    verify(chat->textCursor().selectedText() == QStringLiteral("café"), "Unicode search");
    trigger("selectChat"); trigger("copyChat");
    verify(QApplication::clipboard()->text().contains(QStringLiteral("Alpha beta")), "copy chat");
    const QByteArray exported = chat->document()->toPlainText().toUtf8();
    const QString exportFile = directory.filePath(QStringLiteral("export.txt"));
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QFileDialog *>();
        dialog->selectFile(exportFile);
        qt_fe_select_session(query);
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
    });
    trigger("saveChat");
    QFile file(exportFile);
    verify(file.open(QIODevice::ReadOnly) && file.readAll() == exported, "export captures original conversation despite switching tabs");
    qt_fe_select_session(channel);
    int sendContext;
    engine.insert(&sendContext, engine.value(query)); qt_fe_new_session(&sendContext, "Send context", 0);
    qt_fe_transfer_send_request(&sendContext, "Alice", 0, 0);
    QTimer::singleShot(0, window, [&]() {
        auto *picker = window->findChild<QFileDialog *>();
        verify(picker != nullptr, "deferred send chooser opened after core callback returned");
        if (!picker) return;
        qt_fe_remove_session(&sendContext); qt_fe_new_session(&sendContext, "Reused send context", 0);
        picker->selectFile(exportFile); QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection);
    });
    app->processEvents();
    verify(TransfersTestEngine::sendRequests == 0, "closed/reused conversation cannot send files from an old chooser");
    qt_fe_remove_session(&sendContext); engine.remove(&sendContext); qt_fe_select_session(channel);
    trigger("zoomIn");
    auto *regexSearch = window->findChild<QCheckBox *>(QStringLiteral("regexSearch"));
    regexSearch->setChecked(true); search->setText(QStringLiteral("caf.")); trigger("findNext");
    verify(chat->textCursor().selectedText() == QStringLiteral("café"), "regular expression search uses real chat document");
    search->setText(QStringLiteral("[")); trigger("findNext");
    verify(window->findChild<QLabel *>(QStringLiteral("searchResult"))->text().contains(QStringLiteral("Invalid")), "invalid regex remains an inline search error");
    search->setText(QStringLiteral("café"));
    window->findChild<QPushButton *>(QStringLiteral("showMatchingLines"))->click();
    verify(commands.last().session == channel && commands.last().line == QStringLiteral("/lastlog -h -m -r -- café"), "filtered search routes options through core lastlog command");
    int lastlogKey;
    engine.insert(&lastlogKey, engine.value(query)); qt_fe_new_session(&lastlogKey, "(lastlog)", 0);
    const QString originalBuffer = chat->toPlainText();
    qt_fe_lastlog(channel, &lastlogKey, "café", BufferHighlight);
    verify(activated == &lastlogKey && chat->toPlainText().contains(QStringLiteral("café")), "lastlog callback selects matching results");
    qt_fe_select_session(channel);
    verify(chat->toPlainText() == originalBuffer, "lastlog retains original conversation history");
    qt_fe_remove_session(&lastlogKey); engine.remove(&lastlogKey);
    regexSearch->setChecked(false);
    const QString backgroundFile = directory.filePath(QStringLiteral("chat background.png"));
    QImage backgroundFixture(8, 8, QImage::Format_RGB32); backgroundFixture.fill(QColor(QStringLiteral("#345678")));
    verify(backgroundFixture.save(backgroundFile), "chat background fixture saved");
    const int fontSize = chat->font().pointSize();
    const int savesBeforePreferences = PreferencesTestEngine::saves;
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QDialog *>();
        verify(dialog->findChild<QListWidget *>(QStringLiteral("preferencesCategories"))->count() >= 7, "preferences exposes categorized settings");
        verify(dialog->findChild<QListWidget *>(QStringLiteral("preferencesCategories"))->findItems(QStringLiteral("Colors"), Qt::MatchExactly).size() == 1, "native stripping options share the existing Colors page");
        dialog->findChild<QLineEdit *>(QStringLiteral("pref_irc_quit_reason"))->setText(QStringLiteral("Qt quit message"));
        dialog->findChild<QSpinBox *>(QStringLiteral("pref_net_proxy_port"))->setValue(1080);
        verify(dialog->findChild<QLineEdit *>(QStringLiteral("pref_net_proxy_pass"))->echoMode() == QLineEdit::Password, "proxy password is masked");
        dialog->findChild<QSpinBox *>(QStringLiteral("pref_completion_amount"))->setValue(1);
        dialog->findChild<QComboBox *>(QStringLiteral("pref_completion_sort"))->setCurrentIndex(1);
        verify(dialog->findChild<QListWidget *>(QStringLiteral("preferencesCategories"))->findItems(QStringLiteral("Input Box"), Qt::MatchExactly).size() == 1, "native and Qt input options share one preferences page");
        dialog->findChild<QSpinBox *>(QStringLiteral("fontSize"))->setValue(fontSize + 2);
        dialog->findChild<QSpinBox *>(QStringLiteral("scrollbackLimit"))->setValue(100);
        dialog->findChild<QCheckBox *>(QStringLiteral("timestamps"))->setChecked(false);
        dialog->findChild<QCheckBox *>(QStringLiteral("wrap"))->setChecked(false);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_gui_input_style"))->setChecked(true);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_gui_input_attr"))->setChecked(true);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_text_color_nicks"))->setChecked(true);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_gui_ulist_color"))->setChecked(true);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_gui_ulist_style"))->setChecked(true);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_text_indent"))->setChecked(true);
        dialog->findChild<QSpinBox *>(QStringLiteral("pref_text_max_indent"))->setValue(256);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_gui_input_nick"))->setChecked(true);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_gui_win_nick"))->setChecked(true);
        dialog->findChild<QLineEdit *>(QStringLiteral("pref_completion_suffix"))->setText(QStringLiteral(","));
        dialog->findChild<QLineEdit *>(QStringLiteral("timestampFormat"))->setText(QStringLiteral("[%H:%M:%S] "));
        dialog->findChild<QLineEdit *>(QStringLiteral("chatBackground"))->setText(backgroundFile);
        dialog->accept();
    });
    trigger("preferences");
    verify(chat->font().pointSize() == fontSize + 2 && chat->document()->maximumBlockCount() == 100,
           "display preferences applied to existing documents");
    verify(PreferencesTestEngine::saves == savesBeforePreferences + 1, "core preferences saved together on OK");
    verify(static_cast<ChatInput *>(input)->ircRendering(), "native input attributes apply to main input");
    qt_fe_print(channel, "Nick\tindented message", 0);
    verify(chat->document()->lastBlock().blockFormat().leftMargin() > 0, "native indentation preferences format new transcript messages");
    verify(!chat->palette().brush(QPalette::Base).texture().isNull() && input->palette().brush(QPalette::Base).texture().isNull(), "background textures transcript without texturing input");
    app->processEvents();
    const QImage renderedChat = chat->viewport()->grab().toImage();
    verify(!renderedChat.isNull() && renderedChat.pixelColor(renderedChat.width() - 4, renderedChat.height() - 4) == QColor(QStringLiteral("#345678")), "background image renders behind the actual transcript viewport");
    verify(input->font().pointSize() == fontSize + 2 && !window->findChild<QPushButton *>(QStringLiteral("inputNickname"))->isHidden(), "input style and nickname visibility follow preferences");
    verify(nickList->font() == chat->font() && nickList->palette().color(QPalette::Base) == chat->palette().color(QPalette::Base), "native user list style uses chat font and solid chat colors");
    verify(window->windowTitle().contains(QString::fromUtf8(engine[channel].nick)), "window title can include nickname");
    auto *nicknameButton = window->findChild<QPushButton *>(QStringLiteral("inputNickname"));
    engine[channel].nick_prefixes = "~&@%+";
    for (auto &pref : PreferencesTestEngine::rows) if (QByteArray(pref.key) == "gui_input_icon") qstrncpy(pref.value, "1", sizeof(pref.value));
    qt_fe_user_add(channel, "tester", "@", 0); qt_fe_gui_control(channel, 8, 0);
    verify(!nicknameButton->icon().isNull(), "native rank icon embedded and shown for own operator status");
    engine[channel].away = true; qt_fe_connection_changed();
    verify(nicknameButton->palette().color(QPalette::ButtonText) == ircPaletteColor(40, nicknameButton->palette()), "nickname box shows native away color");
    engine[channel].away = false; qt_fe_connection_changed();
    verify(nicknameButton->palette().color(QPalette::ButtonText) == QApplication::palette(nicknameButton).color(QPalette::ButtonText), "return from away restores nickname color");
    const int beforeNickname = commands.size();
    QTimer::singleShot(0, [&]() {
        auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        verify(dialog && dialog->textValue() == QStringLiteral("tester"), "nickname button pre-fills current nickname");
        qt_fe_select_session(query);
        if (dialog) { dialog->setTextValue(QStringLiteral("newtester")); dialog->accept(); }
    });
    nicknameButton->click();
    verify(commands.size() == beforeNickname + 1 && commands.last().session == channel && commands.last().line == QStringLiteral("/nick newtester"), "nickname prompt retains originating conversation after tab switch");
    qt_fe_select_session(channel);
    for (auto &pref : PreferencesTestEngine::rows) if (QByteArray(pref.key) == "gui_input_icon") qstrncpy(pref.value, "0", sizeof(pref.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(nicknameButton->icon().isNull(), "native icon preference hides rank icon immediately");
    qt_fe_user_remove(channel, "tester");

    auto *configuredInput = static_cast<ChatInput *>(input); configuredInput->setNicks({QStringLiteral("Arthur")}); configuredInput->setText(QStringLiteral("Ar")); configuredInput->completeNick();
    verify(input->text() == QStringLiteral("Arthur, "), "preferences applies nickname completion suffix"); input->clear();
    verify(chat->lineWrapMode() == QTextEdit::NoWrap, "wrapping preference applied");
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QDialog *>();
        dialog->findChild<QLineEdit *>(QStringLiteral("chatBackground"))->clear();
        dialog->findChild<QLineEdit *>(QStringLiteral("pref_irc_quit_reason"))->setText(QStringLiteral("Cancelled"));
        dialog->findChild<QSpinBox *>(QStringLiteral("fontSize"))->setValue(6);
        dialog->reject();
    });
    trigger("preferences");
    verify(chat->font().pointSize() == fontSize + 2 && PreferencesTestEngine::saves == savesBeforePreferences + 1 && !chat->palette().brush(QPalette::Base).texture().isNull(), "cancel discards display, background and core preference changes");
    const int beforeMeterMenuSaves = PreferencesTestEngine::saves;
    for (int mode = 0; mode < 4; ++mode) {
        action(qPrintable(QStringLiteral("networkMeter%1").arg(mode)))->trigger();
        bool stored = true;
        for (const auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_lagometer" || QByteArray(row.key) == "gui_throttlemeter") stored &= QByteArray(row.value).toInt() == mode;
        verify(stored && action(qPrintable(QStringLiteral("networkMeter%1").arg(mode)))->isChecked(), "meter submenu atomically applies both native preferences and checks selected mode");
    }
    verify(PreferencesTestEngine::saves == beforeMeterMenuSaves + 4, "each meter menu choice saves one native preference transaction");
    auto *conversationTabs = window->findChild<QTabBar *>(QStringLiteral("conversationTabs"));
    auto tabIndexFor = [&](void *key) { for (int i = 0; i < conversationTabs->count(); ++i) if (reinterpret_cast<void *>(conversationTabs->tabData(i).value<quintptr>()) == key) return i; return -1; };
    const QString beforeLayoutDraft = input->text(); input->setText(QStringLiteral("layout draft"));
    action("switcherTabs")->trigger();
    verify(!conversationTabs->isHidden() && tree->isHidden() && conversationTabs->count() == 3 && input->text() == QStringLiteral("layout draft"), "native Tabs mode replaces tree without losing current draft or conversations");
    conversationTabs->setCurrentIndex(tabIndexFor(query));
    verify(activated == query, "tab selection activates corresponding core conversation");
    conversationTabs->setCurrentIndex(tabIndexFor(channel));
    verify(input->text() == QStringLiteral("layout draft"), "tab navigation restores independent drafts");
    qt_fe_activity(query, 3);
    verify(conversationTabs->tabTextColor(tabIndexFor(query)) == ircPaletteColor(38, window->palette()), "tabs carry native unread highlight color");
    conversationTabs->setCurrentIndex(tabIndexFor(query));
    verify(conversationTabs->tabTextColor(tabIndexFor(query)) == window->palette().color(QPalette::Text), "reading tab clears its unread color");
    qt_fe_select_session(channel);
    QWheelEvent tabWheel(QPointF(5,5), QPointF(conversationTabs->mapToGlobal(QPoint(5,5))), QPoint(), QPoint(0,-120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(conversationTabs, &tabWheel);
    verify(activated == channel, "disabled native wheel switching also prevents QTabBar default navigation");
    setNative("gui_tab_scrollchans", "1"); qt_fe_gui_control(channel, 8, 0);
    QApplication::sendEvent(conversationTabs, &tabWheel);
    verify(activated == query && conversationTabs->currentIndex() == tabIndexFor(query), "enabled wheel switching and tab selection stay synchronized");
    setNative("gui_tab_scrollchans", "0"); qt_fe_gui_control(channel, 8, 0); qt_fe_select_session(channel);
    action("showChannels")->setChecked(false);
    verify(conversationTabs->isHidden() && tree->isHidden(), "switcher visibility hides both representations");
    action("showChannels")->setChecked(true);
    verify(!conversationTabs->isHidden() && tree->isHidden(), "switcher visibility restores current native layout");
    int tabCloseKey; engine.insert(&tabCloseKey, engine.value(query)); qt_fe_new_session(&tabCloseKey, "Tab &target", false);
    verify(conversationTabs->tabText(tabIndexFor(&tabCloseKey)) == QStringLiteral("Tab &&target"), "tab labels escape mnemonic ampersands while retaining raw conversation identity");
    const int beforeTabPopup = commands.size();
    QTimer::singleShot(0, [&]() {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        menu->setActiveAction(menu->actions().last()); QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); QApplication::sendEvent(menu, &enter);
    });
    conversationTabs->customContextMenuRequested(conversationTabs->tabRect(tabIndexFor(&tabCloseKey)).center());
    verify(commands.size() == beforeTabPopup + 1 && commands.last().session == &tabCloseKey && !engine.contains(&tabCloseKey) && conversationTabs->count() == 3, "tab context close keeps source identity and removes closed tab");
    qt_fe_select_session(channel);
    engine.insert(&tabCloseKey, engine.value(query)); qt_fe_new_session(&tabCloseKey, "Middle tab target", false);
    const QPoint middleTabPoint = conversationTabs->tabRect(tabIndexFor(&tabCloseKey)).center();
    QMouseEvent tabMiddlePress(QEvent::MouseButtonPress, QPointF(middleTabPoint), QPointF(conversationTabs->mapToGlobal(middleTabPoint)), Qt::MiddleButton, Qt::MiddleButton, Qt::NoModifier);
    QMouseEvent tabMiddleRelease(QEvent::MouseButtonRelease, QPointF(middleTabPoint), QPointF(conversationTabs->mapToGlobal(middleTabPoint)), Qt::MiddleButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(conversationTabs, &tabMiddlePress); QApplication::sendEvent(conversationTabs, &tabMiddleRelease);
    verify(!engine.contains(&tabCloseKey) && activated == channel, "middle-click closes background tab without selecting it");
    QTimer::singleShot(0, [&]() {
        auto *dialog = window->findChild<QDialog *>(QStringLiteral("preferencesDialog"));
        auto *layout = dialog->findChild<QComboBox *>(QStringLiteral("pref_gui_tab_layout"));
        verify(layout && layout->count() == 2 && layout->currentData().toInt() == 0 && layout->itemData(1).toInt() == 2, "layout preferences map Tabs and Tree to native values zero and two");
        layout->setCurrentIndex(1); dialog->accept();
    });
    trigger("preferences");
    verify(!tree->isHidden() && conversationTabs->isHidden() && action("switcherTree")->isChecked() && input->text() == QStringLiteral("layout draft"), "native Tree preference restores tree and keeps selected draft");
    auto *leftDock = window->findChild<QSplitter *>(QStringLiteral("leftDock"));
    auto *rightDock = window->findChild<QSplitter *>(QStringLiteral("rightDock"));
    auto *conversationCenter = window->findChild<QWidget *>(QStringLiteral("conversationCenter"));
    const auto layoutDocument = chat->document();
    action("switcherTabs")->trigger();
    setNative("gui_ulist_pos", "4");
    for (int position = 1; position <= 7; ++position) {
        setNative("gui_tab_pos", QByteArray::number(position).constData()); qt_fe_gui_control(channel, 8, 0); QCoreApplication::processEvents();
        if (position <= 4) {
            auto *dock = position <= 2 ? leftDock : rightDock;
            verify(conversationTabs->parentWidget() == dock && !conversationTabs->isHidden(), "native side positions place tabs in correct dock");
            verify(conversationTabs->tabRect(0).width() > conversationTabs->tabRect(0).height(), "vertical tabs retain horizontal label geometry");
        } else if (position <= 6) {
            auto *layout = qobject_cast<QVBoxLayout *>(conversationCenter->layout());
            verify(conversationTabs->parentWidget() == conversationCenter && layout->indexOf(conversationTabs) == (position == 5 ? 0 : layout->count() - 1), "native top and bottom tabs occupy corresponding center-layout edge");
        } else {
            verify(conversationTabs->isHidden() && tree->isHidden() && !action("showChannels")->isChecked(), "native Hidden position hides switcher and synchronizes visibility action");
            action("showChannels")->setChecked(true);
            verify(!conversationTabs->isHidden(), "View visibility action can restore switcher from native Hidden position");
        }
        verify(chat->document() == layoutDocument && input->text() == QStringLiteral("layout draft"), "pane movement preserves document and draft identity");
    }
    for (int users = 1; users <= 4; ++users) {
        const int position = users % 2 ? users + 1 : users - 1;
        setNative("gui_ulist_pos", QByteArray::number(users).constData()); setNative("gui_tab_pos", QByteArray::number(position).constData()); qt_fe_gui_control(channel, 8, 0);
        auto *dock = users <= 2 ? leftDock : rightDock;
        verify(dock->count() == 2 && dock->indexOf(nickList->parentWidget()) == (users % 2 ? 0 : 1), "upper/lower native user and switcher positions share a vertically split dock in correct order");
    }
    setNative("gui_ulist_pos", "3"); setNative("gui_tab_pos", "3"); qt_fe_gui_control(channel, 8, 0);
    verify(rightDock->indexOf(nickList->parentWidget()) == 0 && rightDock->indexOf(conversationTabs) == 1, "colliding pane positions use native switcher fallback below right-side user list");
    setNative("gui_tab_pos", "5"); action("switcherTree")->trigger();
    verify(tree->parentWidget() == leftDock && !tree->isHidden(), "tree mode falls back from top placement to native upper-left position");
    setNative("gui_tab_pos", "1"); qt_fe_gui_control(channel, 8, 0);
    PaneDrag *paneDrag = nullptr;
    for (auto *child : window->findChildren<QObject *>()) if (auto *controller = dynamic_cast<PaneDrag *>(child)) paneDrag = controller;
    verify(paneDrag != nullptr, "main window installs pane drag controller");
    auto dragPane = [&](PaneDrag::Pane pane, const QPoint &point) {
        std::unique_ptr<QMimeData> mime(paneDrag->mimeFor(pane));
        auto *area = window->centralWidget();
        QDragEnterEvent enter(dragEnterPosition(point), Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::ControlModifier | Qt::ShiftModifier);
        QApplication::sendEvent(area, &enter);
        verify(enter.isAccepted(), "local pane drag accepted by main window");
        QDropEvent drop(point, Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::ControlModifier | Qt::ShiftModifier);
        QApplication::sendEvent(area, &drop);
        verify(drop.isAccepted(), "pane drop saves and applies placement");
    };
    const QSize areaSize = window->centralWidget()->size();
    setNative("gui_tab_pos", "1"); setNative("gui_ulist_pos", "3"); qt_fe_gui_control(channel, 8, 0);
    const int beforePaneSave = PreferencesTestEngine::saves;
    dragPane(PaneDrag::Switcher, QPoint(areaSize.width() - 5, 5));
    verify(tree->parentWidget() == rightDock && rightDock->indexOf(tree) == 0 && rightDock->indexOf(nickList->parentWidget()) == 1,
           "tree drop into occupied upper-right slot moves users to adjoining lower-right slot");
    verify(PreferencesTestEngine::saves == beforePaneSave + 1 && chat->document() == layoutDocument && input->text() == QStringLiteral("layout draft"),
           "pane drop persists both positions in one transaction and retains document/draft");
    dragPane(PaneDrag::Users, QPoint(areaSize.width() - 5, 5));
    verify(rightDock->indexOf(nickList->parentWidget()) == 0 && rightDock->indexOf(tree) == 1, "user-list drop displaces occupied switcher as in native GTK");
    action("switcherTabs")->trigger();
    dragPane(PaneDrag::Switcher, QPoint(areaSize.width() / 2, 5));
    verify(conversationTabs->parentWidget() == conversationCenter && qobject_cast<QVBoxLayout *>(conversationCenter->layout())->indexOf(conversationTabs) == 0,
           "tab pane can be dragged to top center");
    dragPane(PaneDrag::Switcher, QPoint(areaSize.width() / 2, areaSize.height() - 5));
    verify(qobject_cast<QVBoxLayout *>(conversationCenter->layout())->indexOf(conversationTabs) == conversationCenter->layout()->count() - 1, "tab pane can be dragged to bottom center");
    {
        const int savedCount = PreferencesTestEngine::saves;
        PreferencesTestEngine::saveOk = false;
        std::unique_ptr<QMimeData> mime(paneDrag->mimeFor(PaneDrag::Users));
        auto *area = window->centralWidget();
        QDragEnterEvent enter(dragEnterPosition(QPoint(5, 5)), Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(area, &enter);
        QDropEvent drop(QPointF(5, 5), Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(area, &drop);
        verify(!drop.isAccepted() && PreferencesTestEngine::saves == savedCount && nickList->parentWidget()->parentWidget() == rightDock,
               "failed preference save rejects pane drop and keeps previous placement");
        PreferencesTestEngine::saveOk = true;
    }
    {
        QWidget foreignArea;
        PaneDrag foreign(&foreignArea);
        std::unique_ptr<QMimeData> mime(foreign.mimeFor(PaneDrag::Switcher));
        QDragEnterEvent enter(dragEnterPosition(QPoint(5, 5)), Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window->centralWidget(), &enter);
        verify(!enter.isAccepted(), "pane drags from a different window are rejected");
        QMimeData textMime; textMime.setText(QStringLiteral("ordinary text"));
        QDragEnterEvent textEnter(dragEnterPosition(QPoint(5, 5)), Qt::CopyAction, &textMime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window->centralWidget(), &textEnter);
        verify(!textEnter.isAccepted(), "ordinary text is not interpreted as pane movement");
    }
    setNative("gui_tab_pos", "1"); setNative("gui_ulist_pos", "3"); action("switcherTree")->trigger(); qt_fe_gui_control(channel, 8, 0);
    input->setText(beforeLayoutDraft);


    trigger("resetMarker");
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QDialog *>(QStringLiteral("preferencesDialog"));
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_text_show_marker"))->setChecked(false);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_text_autocopy_text"))->setChecked(true);
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_text_autocopy_stamp"))->setChecked(true);
        dialog->accept();
    });
    trigger("preferences");
    verify(chat->extraSelections().isEmpty(), "marker visibility preference hides existing marker");
    chat->selectAll(); QApplication::clipboard()->setText(QStringLiteral("before automatic copy"));
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(1, 1), QPointF(1, 1), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(chat->viewport(), &release);
    verify(QApplication::clipboard()->text() == chat->toPlainText(), "automatic copy honors native timestamp option on mouse release");
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QDialog *>(QStringLiteral("preferencesDialog"));
        dialog->findChild<QCheckBox *>(QStringLiteral("pref_text_show_marker"))->setChecked(true);
        dialog->accept();
    });
    trigger("preferences");
    verify(!chat->extraSelections().isEmpty(), "marker can be shown again without discarding its position");
    qt_fe_print(channel, "without timestamp", 0);
    verify(chat->document()->lastBlock().text() == QStringLiteral("without timestamp"), "timestamps can be disabled for new messages");
    trigger("clearChat");
    verify(chat->document()->isEmpty() && chat->extraSelections().isEmpty(), "clear active chat also clears marker");
    qt_fe_print(channel, "top", 0); qt_fe_print(channel, "middle", 0); trigger("resetMarker"); qt_fe_print(channel, "bottom", 0);
    qt_fe_clear_lines(channel, -1);
    verify(chat->toPlainText() == QStringLiteral("top\nmiddle") && !chat->extraSelections().isEmpty(), "negative clear removes newest lines while retaining earlier marker");
    qt_fe_clear_lines(channel, 1); trigger("moveToMarker");
    verify(chat->toPlainText() == QStringLiteral("middle") && !chat->extraSelections().isEmpty(), "positive clear removes oldest lines and keeps surviving marker identity");
    qt_fe_clear_lines(channel, -2147483647 - 1);
    verify(chat->document()->isEmpty() && chat->extraSelections().isEmpty(), "extreme negative clear safely removes entire buffer and marker");
    input->setText(QStringLiteral("detached draft"));
    qt_fe_gui_control(channel, 7, 1);
    auto *detached = static_cast<ConversationWindow *>(window->findChild<QMainWindow *>(QStringLiteral("detachedConversation")));
    verify(detached && !detached->chat->palette().brush(QPalette::Base).texture().isNull(), "detached transcript inherits configured background");
    verify(detached && detached->isVisible() && detached->input->text() == QStringLiteral("detached draft"), "detach preserves draft in independent window");
    nativeActivityQueue = {query};
    const auto beforeDetachedAuto = activated;
    QKeyEvent detachedAuto(QEvent::KeyPress, Qt::Key_QuoteLeft, Qt::AltModifier);
    QApplication::sendEvent(detached->input, &detachedAuto);
    verify(activated == beforeDetachedAuto && nativeActivityQueue == QList<void *>({query}), "automatic page binding in detached input leaves main-window activity queue untouched");
    qt_fe_select_session(server);
    nativeActivityQueue = {channel, query};
    trigger("nextUnread");
    verify(activated == query && nativeActivityQueue == QList<void *>({channel}), "main unread navigation skips detached conversations without consuming them");
    nativeActivityQueue.clear();
    {
        const auto saved = engine[channel]; const bool mainAway = engine[query].away;
        engine[query].away = true; engine[channel].away = false; qt_fe_connection_changed();
        auto detachedAction = [detached](const char *name) { return detached->findChild<QAction *>(QString::fromLatin1(name)); };
        auto *away = detachedAction("detachedAway");
        verify(away->isCheckable() && !away->isChecked() && action("away")->isChecked(), "detached away check uses its own conversation state");
        confirmAwayCommands = false;
        away->trigger();
        verify(!away->isChecked() && commands.last().session == channel && commands.last().line == QStringLiteral("/away"), "detached away request waits for engine confirmation and targets its source");
        engine[channel].away = true; qt_fe_connection_changed();
        verify(away->isChecked(), "confirmed away event checks detached menu");
        away->trigger();
        verify(away->isChecked() && commands.last().session == channel && commands.last().line == QStringLiteral("/back"), "detached back request preserves confirmed check until response");
        confirmAwayCommands = true;
        engine[channel].connected = false; engine[channel].channel = false; engine[channel].connecting = false; engine[channel].reconnect_wait = false;
        qt_fe_connection_changed();
        verify(!away->isEnabled() && !away->isChecked() && !detachedAction("detachedDisconnect")->isEnabled() && !detachedAction("detachedJoin")->isEnabled() && !detachedAction("detachedChannelList")->isEnabled(), "disconnected detached server actions match native availability");
        verify(!detachedAction("detachedBanList")->isEnabled() && !detachedAction("detachedChannelSettings")->isEnabled() && detached->topic->isReadOnly(), "detached channel tools and topic stop accepting edits on disconnect");
        verify(action("away")->isChecked() && action("away")->isEnabled(), "detached connection changes preserve main conversation availability");
        engine[channel].reconnect_wait = true; qt_fe_connection_changed();
        verify(detachedAction("detachedDisconnect")->isEnabled() && !away->isEnabled(), "pending detached reconnect can be canceled without enabling away");
        engine[channel] = saved; engine[query].away = mainAway; qt_fe_select_session(query); qt_fe_connection_changed();
        verify(detachedAction("detachedJoin")->isEnabled() && detachedAction("detachedBanList")->isEnabled() && !detached->topic->isReadOnly(), "detached action availability recovers on reconnect");
    }
    const auto detachedLayoutDocument = detached->chat->document();
    setNative("gui_ulist_pos", "1"); qt_fe_gui_control(channel, 8, 0);
    verify(detached->splitter->indexOf(detached->userPanel) == 0 && detached->chat->document() == detachedLayoutDocument && detached->input->text() == QStringLiteral("detached draft"), "native left user-list placement updates detached window without losing state");
    setNative("gui_ulist_pos", "4"); qt_fe_gui_control(channel, 8, 0);
    verify(detached->splitter->indexOf(detached->userPanel) == 1, "native right user-list placement restores detached panel side");
    setNative("gui_ulist_pos", "3"); qt_fe_gui_control(channel, 8, 0);

    auto *detachedMeterOff = detached->findChild<QAction *>(QStringLiteral("detachedNetworkMeter0"));
    auto *detachedMeterBoth = detached->findChild<QAction *>(QStringLiteral("detachedNetworkMeter3"));
    verify(detachedMeterBoth && detachedMeterBoth->isChecked(), "new detached meter menu inherits native mode");
    detachedMeterOff->trigger();
    verify(action("networkMeter0")->isChecked() && detached->lagMeter->isHidden() && detached->queueMeter->isHidden(), "detached meter action updates global setting and both window types");
    setNative("gui_lagometer", "1"); setNative("gui_throttlemeter", "2"); qt_fe_gui_control(channel, 8, 0);
    bool anyMeterChecked = false;
    for (auto *item : window->findChild<QMenu *>(QStringLiteral("networkMetersMenu"))->actions()) anyMeterChecked |= item->isChecked();
    verify(!anyMeterChecked, "mixed native lag and throttle modes do not falsely check a common menu mode");
    detachedMeterBoth->trigger();
    verify(action("networkMeter3")->isChecked() && !detached->lagMeter->isHidden() && !detached->lagStatus->isHidden(), "detached Both action resynchronizes menus and visible meters");

    verify(detached->chat->autoCopy && detached->chat->copyTimestamps && !detached->chat->copyColors, "native clipboard preferences apply to detached transcripts");
    auto *detachedFile = detached->findChild<QMenu *>(QStringLiteral("detachedZoitechatMenu"));
    QStringList detachedLabels;
    for (auto *entry : detachedFile->actions()) detachedLabels.append(entry->isSeparator() ? QStringLiteral("|") : QString(entry->text()).remove(QLatin1Char('&')));
    verify(detachedLabels == QStringList({QStringLiteral("Network List"), QStringLiteral("|"), QStringLiteral("New"), QStringLiteral("|"), QStringLiteral("Load Plugin or Script…"), QStringLiteral("|"), QStringLiteral("Attach"), QStringLiteral("Close"), QStringLiteral("|"), QStringLiteral("Quit")}), "detached window keeps the original file menu structure");
    qt_fe_topic(channel, formattedTopic.toUtf8().constData());
    verify(detached->topic->text() == formattedTopic, "detached topics receive confirmed raw updates immediately");
    for (auto &pref : PreferencesTestEngine::rows) if (QByteArray(pref.key) == "text_stripcolor_topic") qstrncpy(pref.value, "1", sizeof(pref.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(detached->topic->text() == QStringLiteral("  red bold <b>literal</b>  "), "GUI APPLY updates native topic stripping in detached view");
    for (auto &pref : PreferencesTestEngine::rows) if (QByteArray(pref.key) == "text_stripcolor_topic") qstrncpy(pref.value, "0", sizeof(pref.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(detached->topic->text() == formattedTopic, "detached topic recovers raw attributes after disabling stripping");
    qt_fe_print(channel, "detached live message", 0);
    verify(detached->chat->toPlainText().contains(QStringLiteral("detached live message")), "detached history receives live messages");
    verify(detached->input->ircRendering(), "new detached input inherits native attribute rendering");
    completionTestNicks = {QStringLiteral("Arthur"), QStringLiteral("Artemis")};
    verify(detached->users->font() == chat->font() && detached->input->font() == input->font() && !detached->nickname->isHidden(), "native input and user list styles apply to newly detached windows");
    for (auto &pref : PreferencesTestEngine::rows)
        if (QByteArray(pref.key) == "gui_input_attr" || QByteArray(pref.key) == "gui_input_style" || QByteArray(pref.key) == "gui_ulist_style" || QByteArray(pref.key) == "gui_input_nick") qstrncpy(pref.value, "0", sizeof(pref.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(input->font() == QApplication::font(input) && detached->input->font() == QApplication::font(detached->input) && nickList->font() == QApplication::font(nickList) && detached->users->font() == QApplication::font(detached->users), "GUI APPLY restores theme fonts in main and detached views");
    verify(input->palette() == QApplication::palette(input) && detached->users->palette() == QApplication::palette(detached->users) && detached->nickname->isHidden(), "GUI APPLY restores theme colors and hides native nickname box");
    for (auto &pref : PreferencesTestEngine::rows)
        if (QByteArray(pref.key) == "gui_input_attr" || QByteArray(pref.key) == "gui_input_style" || QByteArray(pref.key) == "gui_ulist_style" || QByteArray(pref.key) == "gui_input_nick") qstrncpy(pref.value, "1", sizeof(pref.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(detached->input->ircRendering() && static_cast<ChatInput *>(input)->ircRendering(), "GUI APPLY restores attribute rendering in both input windows");
    struct VisibilityCase { const char *key; const char *main; QAction *other; bool inverted; };
    for (const auto &entry : {
        VisibilityCase{"gui_hide_menu", "showMenu", detached->menuVisible, true},
        VisibilityCase{"gui_topicbar", "showTopic", detached->findChild<QAction *>(QStringLiteral("detachedShowTopic")), false},
        VisibilityCase{"gui_ulist_hide", "showUsers", detached->findChild<QAction *>(QStringLiteral("detachedShowUsers")), true},
        VisibilityCase{"gui_ulist_buttons", "showUserButtons", detached->showUserButtons, false},
        VisibilityCase{"gui_mode_buttons", "showChannelModes", detached->showModes, false}}) {
        auto *main = action(entry.main);
        const bool initial = main->isChecked();
        main->trigger();
        bool native = initial;
        for (const auto &pref : PreferencesTestEngine::rows)
            if (QByteArray(pref.key) == entry.key) native = QByteArray(pref.value).toInt() != 0;
        verify(main->isChecked() == !initial && entry.other->isChecked() == !initial && native == (!initial != entry.inverted), "main View action saves native visibility and synchronizes detached window");
        entry.other->trigger();
        verify(main->isChecked() == initial && entry.other->isChecked() == initial, "detached View action saves native visibility and synchronizes main window");
    }
    PreferencesTestEngine::saveOk = false;
    const bool topicBeforeFailure = action("showTopic")->isChecked();
    trigger("showTopic");
    verify(action("showTopic")->isChecked() == topicBeforeFailure && !detached->topic->isHidden(), "failed native visibility save restores menu check and visible widgets");
    PreferencesTestEngine::saveOk = true;
    auto *detachedUsermenu = detached->findChild<QMenu *>(QStringLiteral("userMenu"));
    verify(detachedUsermenu && !detachedUsermenu->menuAction()->isVisible(), "new detached window hides optional Usermenu by default");
    for (const char *value : {"1", "0"}) {
        for (auto &pref : PreferencesTestEngine::rows)
            if (QByteArray(pref.key) == "gui_usermenu") qstrncpy(pref.value, value, sizeof(pref.value));
        qt_fe_gui_control(channel, 8, 0);
        verify(nativeUsermenu->menuAction()->isVisible() == (value[0] == '1') && detachedUsermenu->menuAction()->isVisible() == (value[0] == '1'), "GUI APPLY honors native optional Usermenu in main and detached windows");
    }
    detached->input->setText(QStringLiteral("Ar")); detached->input->completeNick();
    verify(detached->input->text() == QStringLiteral("Art") && detached->chat->toPlainText().contains(QStringLiteral("Arthur Artemis")), "native completion threshold lists matches in detached source and extends prefix");
    for (auto &pref : PreferencesTestEngine::rows) if (QByteArray(pref.key) == "completion_amount") qstrncpy(pref.value, "2", sizeof(pref.value));
    qt_fe_gui_control(channel, 8, 0); detached->input->setText(QStringLiteral("Ar")); detached->input->completeNick();
    verify(detached->input->text() == QStringLiteral("Arthur, "), "GUI APPLY updates threshold in detached input");
    for (const QString &suffix : {QStringLiteral("é"), QString()}) {
        for (auto &pref : PreferencesTestEngine::rows) if (QByteArray(pref.key) == "completion_suffix") qstrncpy(pref.value, suffix.toUtf8().constData(), sizeof(pref.value));
        qt_fe_gui_control(channel, 8, 0);
        detached->input->setText(QStringLiteral("Ar")); detached->input->completeNick();
        verify(detached->input->text() == QStringLiteral("Arthur") + suffix + QLatin1Char(' '), "GUI APPLY updates native Unicode and empty suffixes in detached input");
        input->setText(QStringLiteral("Ar")); static_cast<ChatInput *>(input)->completeNick();
        verify(input->text() == QStringLiteral("Arthur") + suffix + QLatin1Char(' '), "GUI APPLY updates native suffix in main input too");
    }
    const int beforeDetachedNickname = commands.size();
    QTimer::singleShot(0, [&]() {
        auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        verify(dialog && dialog->parentWidget() == detached, "detached nickname prompt belongs to its window");
        if (dialog) { dialog->setTextValue(QStringLiteral("detachednick")); dialog->accept(); }
    });
    detached->nickname->click();
    verify(commands.size() == beforeDetachedNickname + 1 && commands.last().session == channel && commands.last().line == QStringLiteral("/nick detachednick"), "detached nickname button targets its own session");

    completionTestNicks = {QStringLiteral("Arthur")};
    auto detachedAction = [&](const char *name) { return detached->findChild<QAction *>(QString::fromLatin1(name)); };
    detachedAction("detachedFind")->trigger(); detached->search->setText(QStringLiteral("live message")); detachedAction("detachedFindNext")->trigger();
    verify(detached->chat->textCursor().selectedText() == QStringLiteral("live message"), "detached search uses its transcript rather than main selection");
    qt_fe_user_add(channel, "DetachedUser", "@", 0);
    detached->userFilter->setText(QStringLiteral("DetachedUser"));
    verify(detached->users->visibleCount() == 1 && detached->userCount->text().startsWith(QStringLiteral("1/")), "detached filter and count apply to its own membership");
    detached->userFilter->clear();
    engine[channel].lag_ms = 420; engine[channel].queued_bytes = 512;
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_lagometer" || QByteArray(row.key) == "gui_throttlemeter") qstrncpy(row.value, "3", sizeof(row.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(detached->lagMeter->value() == 420 && detached->queueMeter->value() == 512 && !detached->lagStatus->isHidden() && detached->queueStatus->text().contains(QStringLiteral("512")), "detached meters use their own server snapshot and native modes");
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_ulist_count") qstrncpy(row.value, "0", sizeof(row.value));
    qt_fe_gui_control(channel, 8, 0);
    verify(detached->userCount->isHidden() && window->findChild<QLabel *>(QStringLiteral("userCount"))->isHidden(), "native user-count preference hides both main and detached counts");
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "gui_ulist_count") qstrncpy(row.value, "1", sizeof(row.value));
    engine[channel].queued_bytes = 0; engine[channel].lag_ms = 0; qt_fe_gui_control(channel, 8, 0);

    detached->users->activate(QStringLiteral("DetachedUser"));
    verify(CustomizationTestEngine::executed.last() == QStringLiteral("WHOIS %s") && CustomizationTestEngine::lastSession == channel && CustomizationTestEngine::lastTarget == QStringLiteral("DetachedUser"), "detached activation runs native command in its own context");

    qt_fe_user_select(channel, "DetachedUser", 1, 1);
    QListWidgetItem *coloredDetached = nullptr;
    for (int row = 0; row < detached->users->count(); ++row) if (detached->users->item(row)->data(Qt::UserRole).toString() == QStringLiteral("DetachedUser")) coloredDetached = detached->users->item(row);
    verify(coloredDetached && coloredDetached->foreground().color() == ircPaletteColor(4, detached->users->palette()), "detached user list uses native nickname color selection");
    auto *detachedMode = detached->modeButtons.value(QLatin1Char('m'));
    const int beforeDetachedMode = ModerationTestEngine::changes.size();
    detachedMode->click();
    verify(ModerationTestEngine::changes.size() == beforeDetachedMode + 1 && ModerationTestEngine::changes.last().session == channel && !ModerationTestEngine::changes.last().enable && detachedMode->isChecked(), "detached mode toggle uses source and waits for confirmation");
    detached->showModes->setChecked(false); verify(detached->modeRow->isHidden(), "detached mode bar visibility can be changed independently"); detached->showModes->setChecked(true);
    CustomizationTestEngine::lists[QT_CONFIG_BUTTONS] = {{QStringLiteral("Inspect Detached"), QStringLiteral("echo %s")}};
    qt_fe_customization_changed(QT_CONFIG_BUTTONS); detached->showUserButtons->setChecked(true);
    qt_fe_user_select(channel, "DetachedUser", 1, 1);
    auto *detachedUserButton = detached->userButtons->findChild<QPushButton *>(QStringLiteral("customCommandButton"));
    verify(detachedUserButton && detachedUserButton->isEnabled(), "detached custom user button is available for selected nick");
    if (detachedUserButton) detachedUserButton->click();
    verify(CustomizationTestEngine::lastSession == channel && CustomizationTestEngine::lastTarget == QStringLiteral("DetachedUser"), "detached custom button expands nick on its own session");
    engine[channel].connected = false; qt_fe_connection_changed();
    verify(!detachedMode->isEnabled() && !detachedUserButton->isEnabled(), "disconnect disables detached channel controls and buttons");
    engine[channel].connected = true; qt_fe_connection_changed();

    detached->input->clear(); detached->users->userAction(NickAction::Insert, {QStringLiteral("DetachedUser")});
    verify(detached->input->text() == QStringLiteral("DetachedUser ") && input->text() != detached->input->text(), "detached user insertion changes only its draft");
    const int beforeDetachedModeration = commands.size();
    detached->users->userAction(NickAction::Operator, {QStringLiteral("DetachedUser")});
    verify(commands.size() == beforeDetachedModeration + 1 && commands.last().session == channel && commands.last().line == QStringLiteral("/op DetachedUser"), "detached moderation preserves source context");
    detached->input->clear();
    QTimer::singleShot(0, window, [&]() { auto *preview = detached->findChild<QDialog *>(QStringLiteral("pastePreview")); verify(preview != nullptr, "detached paste opens preview in its own window"); if (preview) preview->accept(); });
    const int beforeDetachedPaste = commands.size(); detached->input->pasteText(QStringLiteral("detached first\n/quit"));
    auto *detachedQueue = window->findChild<QTimer *>(QStringLiteral("pasteQueue"));
    verify(detachedQueue != nullptr && commands.size() == beforeDetachedPaste, "detached paste is paced after preview");
    qt_fe_select_session(query);
    if (detachedQueue) { QMetaObject::invokeMethod(detachedQueue, "timeout", Qt::DirectConnection); QMetaObject::invokeMethod(detachedQueue, "timeout", Qt::DirectConnection); }
    verify(commands.size() == beforeDetachedPaste + 2 && commands.last().session == channel && commands.last().literal && commands.last().line == QStringLiteral("/quit"), "detached paste stays literal and bound to source across main tab switch");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_gui_control(channel, 6, 0);
    verify(detached->menuBar()->isHidden() && !window->menuBar()->isHidden(), "GUI MENU targets only its detached window");
    detached->menuVisible->trigger();
    verify(!detached->menuBar()->isHidden(), "detached menu recovery remains available");
    qt_fe_gui_control(channel, 5, 0);
    verify(qt_fe_session_window_status(channel) == 2 && qt_fe_session_window_status(query) != 2, "GUI info reports detached window state independently");
    qt_fe_gui_control(channel, 1, 0);
    qt_fe_set_input(channel, "reattach draft");
    verify(detached->input->text() == QStringLiteral("reattach draft") && QString::fromUtf8(qt_fe_get_input(channel)) == QStringLiteral("reattach draft"), "core input API routes to detached input");
    detached->chat->setLineWrapMode(QTextEdit::WidgetWidth);
    detached->chat->verticalScrollBar()->setValue(detached->chat->verticalScrollBar()->maximum());
    for (int i = 0; i < 5; ++i) qt_fe_print(channel, (QStringLiteral("* peer has quit: ") + QString(900, QLatin1Char('x'))).toUtf8().constData(), 0);
    QCoreApplication::processEvents();
    verify(detached->chat->verticalScrollBar()->value() == detached->chat->verticalScrollBar()->maximum(), "detached transcript follows channel status output");
    detached->resize(700, 500); QCoreApplication::processEvents();
    verify(detached->chat->verticalScrollBar()->value() == detached->chat->verticalScrollBar()->maximum(), "detached follow survives reflow during resize");
    const QSize detachedSize = detached->size(); const QString detachedGeometryKey = detached->geometryKey;
    detached->close();
    verify(input->text() == QStringLiteral("reattach draft") && chat->toPlainText().contains(QStringLiteral("detached live message")), "closing detached window reattaches draft and history");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_gui_control(channel, 7, 1);
    auto *restoredDetached = static_cast<ConversationWindow *>(window->findChild<QMainWindow *>(QStringLiteral("detachedConversation")));
    verify(restoredDetached->geometryKey == detachedGeometryKey && restoredDetached->size() == detachedSize, "reattaching and detaching restores window placement");
    qt_fe_gui_control(channel, 7, 2); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_gui_control(channel, 6, 0);
    verify(window->menuBar()->isHidden() && !action("showMenu")->isChecked(), "GUI MENU toggles menu bar");
    verify(action("showMenu")->shortcut() == QKeySequence(QStringLiteral("Ctrl+F9")), "hidden menu has native recovery shortcut");
    trigger("showMenu");
    verify(!window->menuBar()->isHidden(), "menu can be restored through global action");
    qt_fe_gui_control(channel, 5, 0);
    verify(qt_fe_window_status() == 2, "iconified window reports hidden status");
    qt_fe_gui_control(channel, 2, 0);
    verify(!window->isMinimized() && activated == channel, "GUI FOCUS restores the requested conversation");
    qt_fe_gui_control(channel, 0, 0);
    verify(qt_fe_window_status() == 2, "GUI HIDE retains a recoverable minimized window without tray");
    qt_fe_gui_control(channel, 1, 0);
    for (int i = 0; i < 120; ++i) qt_fe_print(channel, "bounded line", 0);
    verify(chat->document()->blockCount() == 100, "scrollback limit enforced");
    action("autoScroll")->setChecked(true);
    chat->setLineWrapMode(QTextEdit::WidgetWidth);
    chat->verticalScrollBar()->setValue(chat->verticalScrollBar()->maximum());
    for (const auto &status : {QStringLiteral("* peer has joined #test"), QStringLiteral("* server sets mode +o peer\n* peer changed the topic"), QStringLiteral("* peer has quit: ") + QString(1500, QLatin1Char('x'))}) {
        qt_fe_print(channel, status.toUtf8().constData(), 0);
        QCoreApplication::processEvents();
        verify(chat->verticalScrollBar()->value() == chat->verticalScrollBar()->maximum(), "following stays at bottom for channel statuses, multiline and wrapped output");
    }
    qt_fe_select_session(query);
    qt_fe_print(channel, (QStringLiteral("* peer has quit: ") + QString(3000, QLatin1Char('x'))).toUtf8().constData(), 0);
    qt_fe_select_session(channel);
    QCoreApplication::processEvents();
    verify(chat->verticalScrollBar()->value() == chat->verticalScrollBar()->maximum(), "returning to a followed channel shows background status lines at the bottom");
    const QSize beforeStatusResize = window->size();
    window->resize(qMax(window->minimumWidth(), beforeStatusResize.width() - 180), qMax(window->minimumHeight(), beforeStatusResize.height() - 90));
    QCoreApplication::processEvents();
    verify(chat->verticalScrollBar()->value() == chat->verticalScrollBar()->maximum(), "following remains at bottom when channel layout changes");
    window->resize(beforeStatusResize); QCoreApplication::processEvents();
    chat->verticalScrollBar()->setValue(0);
    qt_fe_select_session(query);
    qt_fe_print(channel, "* another peer joined while reading history", 0);
    qt_fe_select_session(channel); QCoreApplication::processEvents();
    verify(chat->verticalScrollBar()->value() == 0, "background statuses preserve intentional reading position");
    chat->setLineWrapMode(QTextEdit::NoWrap);
    trigger("resetMarker");
    for (int i = 0; i < 120; ++i) qt_fe_print(channel, "after old marker trimmed", 0);
    verify(chat->extraSelections().isEmpty(), "trimmed marker block is invalidated safely");
    trigger("showUsers"); trigger("showChannels"); trigger("showTopic"); trigger("showChannelModes");
    verify(!window->findChild<QListWidget *>()->isVisible() && tree->isHidden(), "view toggles hide panels");
    trigger("autoScroll");
    chat->verticalScrollBar()->setValue(0);
    qt_fe_print(channel, "do not follow", 0);
    verify(chat->verticalScrollBar()->value() == 0, "disabled following preserves scroll position");
    trigger("jumpLatest");
    verify(chat->verticalScrollBar()->value() == chat->verticalScrollBar()->maximum(), "jump to latest");
    trigger("disconnect");
    verify(commands.last().line == QStringLiteral("/discon") && !action("editTopic")->isEnabled(), "disconnect command and enablement");
    trigger("reconnect");
    verify(commands.last().line == QStringLiteral("/reconnect"), "reconnect command");
    engine[channel].connected = true;
    qt_fe_select_session(query); qt_fe_select_session(channel);
    trigger("leave");
    verify(commands.last().line == QStringLiteral("/close") && tree->topLevelItemCount() == 2 && activated == query,
           "leave closes conversation and selects adjacent tab");
    engine.insert(channel, {1, 0, 1, 0, 0, "tester", "irc.test"});
    qt_fe_new_session(channel, "#rejoined", 1);
    trigger("leave");
    verify(tree->topLevelItemCount() == 2 && activated == query, "repeated leave does not accumulate empty tabs");
    engine.insert(channel, {1, 0, 1, 0, 0, "tester", "irc.test"});
    qt_fe_new_session(channel, "#rejoined", 1);
    const int beforeLeaveMenu = commands.size();
    QTimer::singleShot(0, window, [&]() {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        for (auto *entry : menu->actions()) if (entry->text() == QStringLiteral("Leave Channel")) menu->setActiveAction(entry);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(menu, &enter);
    });
    tree->customContextMenuRequested(tree->visualItemRect(tree->currentItem()).center());
    verify(commands.size() == beforeLeaveMenu + 1 && commands.last().line == QStringLiteral("/close") &&
           tree->topLevelItemCount() == 2, "context menu leave also closes conversation");
    engine.insert(channel, {1, 0, 1, 0, 0, "tester", "irc.test"});
    qt_fe_new_session(channel, "#rejoined", 1);
    sentTopic = QStringLiteral("unchanged");
    QPointer<QTextDocument> oldDocument = chat->document();
    QTimer::singleShot(0, window, [&]() {
        auto *dialog = window->findChild<QInputDialog *>();
        qt_fe_remove_session(channel);
        qt_fe_new_session(channel, "#replacement", 1);
        dialog->setTextValue(QStringLiteral("stale topic")); dialog->accept();
    });
    trigger("editTopic");
    verify(sentTopic == QStringLiteral("unchanged") && oldDocument.isNull(), "closed/reused session rejects stale topic and frees document");
    const int beforeMenu = commands.size();
    QTimer::singleShot(0, window, [&]() {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        qt_fe_remove_session(channel);
        qt_fe_new_session(channel, "#another", 1);
        menu->setActiveAction(menu->actions().last());
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(menu, &enter);
    });
    tree->customContextMenuRequested(tree->visualItemRect(tree->currentItem()).center());
    verify(commands.size() == beforeMenu, "closed/reused session rejects stale context menu");
    QFile keyboardFile(directory.filePath(QStringLiteral("keybindings.conf")));
    verify(keyboardFile.open(QIODevice::WriteOnly), "keyboard fixture opens");
    keyboardFile.write("ACCEL=<Shift>Page_Down\nChange Selected Nick\nD1!\nD2!\n\nACCEL=<Shift>Page_Up\nChange Selected Nick\nD1:Up\nD2!\n\n");
    keyboardFile.write("ACCEL=<Alt>Up\nLast Command\nD1!\nD2!\n\nACCEL=<Alt>j\nComplete nick/command\nD1!\nD2!\n\nACCEL=<Primary>y\nInsert in Buffer\nD1:\002\nD2!\n\n");
    keyboardFile.write("ACCEL=F6\nChange Page\nD1:999\nD2!\n\nACCEL=F7\nChange Page\nD1:0\nD2!\n\n");
    keyboardFile.write("ACCEL=F10\nRun Command\nD1:/echo keyboard-one\\n/echo keyboard-two\nD2!\n\nACCEL=<Alt>x\nInsert in Buffer\nD1:inserted\nD2!\n\nACCEL=F11\nRun Command\nD1:/close\\n/echo must-not-run\nD2!\n"); keyboardFile.write("\nACCEL=F12\nPush input line into history\nD1!\nD2!\n\nACCEL=F8\nMove tab family left\nD1!\nD2!\n\nACCEL=F9\nMove tab family right\nD1!\nD2!\n"); keyboardFile.close();
    qt_fe_gui_control(channel, 8, 0); qt_fe_set_input(channel, "draft"); input->selectAll();
    QKeyEvent insertBinding(QEvent::KeyPress, Qt::Key_X, Qt::AltModifier); QApplication::sendEvent(input, &insertBinding);
    verify(input->text() == QStringLiteral("inserted"), "native key binding replaces input selection");
    {
        const auto savedCandidates = completionTestNicks;
        completionTestNicks = {QStringLiteral("Alpha"), QStringLiteral("Alpine")};
        const auto selectedUsers = nickList->selectedItems();
        input->setText(QStringLiteral("Al"));
        QKeyEvent nextNick(QEvent::KeyPress, Qt::Key_PageDown, Qt::ShiftModifier);
        QKeyEvent previousNick(QEvent::KeyPress, Qt::Key_PageUp, Qt::ShiftModifier);
        QApplication::sendEvent(input, &nextNick);
        const QString firstCandidate = input->text();
        verify(firstCandidate.startsWith(QStringLiteral("Alpha")), "Change Selected Nick completes the input rather than selecting a user-list row");
        QApplication::sendEvent(input, &nextNick);
        verify(input->text().startsWith(QStringLiteral("Alpine")), "Change Selected Nick cycles completion forward");
        QApplication::sendEvent(input, &previousNick);
        verify(input->text() == firstCandidate && nickList->selectedItems() == selectedUsers, "reverse Change Selected Nick preserves user-list selection");
        completionTestNicks = savedCandidates;
        input->setText(QStringLiteral("inserted"));
    }
    {
        input->setText(QStringLiteral("removed default"));
        QKeyEvent oldBold(QEvent::KeyPress, Qt::Key_B, Qt::ControlModifier);
        QApplication::sendEvent(input, &oldBold);
        verify(input->text() == QStringLiteral("removed default"), "custom binding table disables removed formatting shortcut");
        input->setCursorPosition(input->text().size());
        QKeyEvent movedBold(QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier);
        QApplication::sendEvent(input, &movedBold);
        verify(input->text() == QStringLiteral("removed default") + QChar(2), "remapped formatting shortcut still inserts IRC attributes");
        input->setText(QStringLiteral("Al"));
        QKeyEvent oldTab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QApplication::sendEvent(input, &oldTab);
        verify(input->text() == QStringLiteral("Al"), "custom binding table disables removed Tab completion");
        const auto replacement = static_cast<ChatInput *>(input)->wordReplacement;
        static_cast<ChatInput *>(input)->wordReplacement = [](const QString &) { return QStringLiteral("replacement"); };
        input->setText(QStringLiteral("original"));
        QKeyEvent oldReplacement(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
        QApplication::sendEvent(input, &oldReplacement);
        verify(input->text() == QStringLiteral("original "), "removed Check For Replace binding does not run automatic fallback");
        static_cast<ChatInput *>(input)->wordReplacement = replacement;
        input->setText(QStringLiteral("inserted"));
    }
    const int beforeBinding = commands.size(); QKeyEvent runBinding(QEvent::KeyPress, Qt::Key_F10, Qt::NoModifier); QApplication::sendEvent(input, &runBinding);
    verify(commands.size() == beforeBinding + 2 && commands.last().session == channel && commands.last().line == QStringLiteral("/echo keyboard-two") && input->text() == QStringLiteral("inserted"), "command binding sends escaped commands through original context without changing draft");
    {
        const int beforePluginBinding = commands.size();
        keyHook = [](void *) { return 1; };
        QApplication::sendEvent(input, &runBinding);
        verify(commands.size() == beforePluginBinding && pluginKeys.last().session == channel && pluginKeys.last().keyval == 0xffc7,
               "main plugin Key Press hook receives native F10 and can suppress its command binding");
        keyHook = {};
        int victim;
        engine.insert(&victim, engine.value(channel)); qt_fe_new_session(&victim, "Hook Victim", 1);
        keyHook = [&](void *source) { qt_fe_remove_session(source); qt_fe_new_session(source, "Hook Replacement", 1); return 0; };
        QApplication::sendEvent(input, &runBinding);
        verify(commands.size() == beforePluginBinding && input->text().isEmpty(), "plugin closing/reusing input source prevents binding execution on replacement conversation");
        keyHook = {};
        qt_fe_remove_session(&victim); engine.remove(&victim); qt_fe_select_session(channel);
    }
    QKeyEvent pushHistory(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier); QApplication::sendEvent(input, &pushHistory);
    verify(input->text().isEmpty(), "push input history binding clears draft without submitting");
    const auto beforeInvalidPage = activated;
    QKeyEvent highPage(QEvent::KeyPress, Qt::Key_F6, Qt::NoModifier), zeroPage(QEvent::KeyPress, Qt::Key_F7, Qt::NoModifier);
    QApplication::sendEvent(input, &highPage); QApplication::sendEvent(input, &zeroPage);
    verify(activated == beforeInvalidPage, "out-of-range absolute page bindings leave the current conversation in place");
    QKeyEvent removedHistory(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier); QApplication::sendEvent(input, &removedHistory);
    verify(input->text().isEmpty(), "custom binding table disables removed history shortcut");
    QKeyEvent previousHistory(QEvent::KeyPress, Qt::Key_Up, Qt::AltModifier); QApplication::sendEvent(input, &previousHistory);
    verify(input->text() == QStringLiteral("inserted"), "pushed draft is available in conversation history");
    input->setText(QStringLiteral("/j")); QKeyEvent completeCommand(QEvent::KeyPress, Qt::Key_J, Qt::AltModifier); QApplication::sendEvent(input, &completeCommand);
    verify(input->text() == QStringLiteral("/JOIN "), "main input completes commands supplied by core bridge");
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "input_command_char") qstrncpy(row.value, "!", sizeof(row.value));
    qt_fe_gui_control(channel, 8, 0); input->setText(QStringLiteral("!j")); QApplication::sendEvent(input, &completeCommand);
    verify(input->text() == QStringLiteral("!JOIN "), "GUI APPLY updates configured command prefix in main input");
    const int commandsBeforeAway = explicitCommands; trigger("away");
    verify(explicitCommands == commandsBeforeAway + 1 && commands.last().session == channel, "menu commands use explicit command route with custom prefix");
    const int commandsBeforeBindingSyntax = explicitCommands; QApplication::sendEvent(input, &runBinding);
    verify(explicitCommands == commandsBeforeBindingSyntax, "custom command bindings retain user input syntax route");
    for (auto &row : PreferencesTestEngine::rows) if (QByteArray(row.key) == "input_command_char") qstrncpy(row.value, "/", sizeof(row.value));
    qt_fe_gui_control(channel, 8, 0);
    ToolTestEngine::sessionServers.insert(server, server); ToolTestEngine::sessionServers.insert(channel, server);
    QKeyEvent familyRight(QEvent::KeyPress, Qt::Key_F9, Qt::NoModifier); QApplication::sendEvent(input, &familyRight);
    verify(tree->topLevelItem(0)->text(0) == QStringLiteral("Alice") && tree->topLevelItem(1)->text(0) == QStringLiteral("Server"), "family binding moves server and channel as one ordered group");
    QKeyEvent familyLeft(QEvent::KeyPress, Qt::Key_F8, Qt::NoModifier); QApplication::sendEvent(input, &familyLeft);
    verify(tree->topLevelItem(0)->text(0) == QStringLiteral("Server") && tree->topLevelItem(1)->text(0) == QStringLiteral("Alice") && tree->topLevelItem(0)->childCount() == 1, "family reverse move restores grouped order");
    int familyRootKey, familyZKey, familyAKey, familyDialogKey;
    engine.insert(&familyRootKey, engine.value(server));
    engine.insert(&familyZKey, engine.value(channel)); engine.insert(&familyAKey, engine.value(channel)); engine.insert(&familyDialogKey, engine.value(query));
    for (void *key : {static_cast<void *>(&familyRootKey), static_cast<void *>(&familyZKey), static_cast<void *>(&familyAKey), static_cast<void *>(&familyDialogKey)}) ToolTestEngine::sessionServers.insert(key, &familyRootKey);
    setNative("gui_tab_sort", "1"); qt_fe_gui_control(channel, 8, 0);
    qt_fe_new_session(&familyRootKey, "Other Server", false); qt_fe_new_session(&familyZKey, "#z", false);
    qt_fe_new_session(&familyDialogKey, "ADialog", false); qt_fe_new_session(&familyAKey, "#a", false);
    auto *familyRoot = tree->topLevelItem(2);
    verify(tree->topLevelItemCount() == 3 && familyRoot->text(0) == QStringLiteral("Other Server") && familyRoot->childCount() == 3 && familyRoot->child(0)->text(0) == QStringLiteral("#a") && familyRoot->child(2)->text(0) == QStringLiteral("ADialog"), "switcher groups server families and sorts channels before dialogs");
    setNative("gui_tab_dots", "0"); qt_fe_gui_control(channel, 8, 0); familyRoot->setExpanded(true); tree->scrollToItem(familyRoot->child(0)); QCoreApplication::processEvents();
    const QRect branchRow = tree->visualItemRect(familyRoot->child(0));
    const QRect branchArea(0, branchRow.top(), branchRow.left(), branchRow.height());
    const QImage plainBranches = tree->viewport()->grab().toImage().copy(branchArea);
    setNative("gui_tab_dots", "1"); qt_fe_gui_control(channel, 8, 0); QCoreApplication::processEvents();
    const QImage dottedBranches = tree->viewport()->grab().toImage().copy(branchArea);
    verify(!plainBranches.isNull() && plainBranches != dottedBranches, "native tree-lines toggle changes actual rendered branch pixels");
    setNative("gui_tab_dots", "0"); qt_fe_gui_control(channel, 8, 0);
    qt_fe_select_session(&familyRootKey); familyRoot->setExpanded(false); trigger("nextConversation");
    verify(activated == &familyAKey && familyRoot->isExpanded(), "navigation visits child conversations and expands their family");
    setNative("gui_tab_sort", "0"); qt_fe_gui_control(channel, 8, 0); qt_fe_rename_session(&familyAKey, "#zz");
    verify(familyRoot->child(0)->text(0) == QStringLiteral("#zz"), "disabled sorting retains sibling order after rename");
    setNative("gui_tab_sort", "1"); qt_fe_gui_control(channel, 8, 0);
    verify(familyRoot->child(0)->text(0) == QStringLiteral("#z"), "live sorting uses full conversation names");
    qt_fe_remove_session(&familyRootKey); engine.remove(&familyRootKey);
    verify(tree->topLevelItem(2)->text(0) == QStringLiteral("#z") && tree->topLevelItem(2)->childCount() == 2, "closing family root promotes surviving conversation without deleting its siblings");
    qt_fe_select_session(&familyAKey); qt_fe_print(&familyAKey, "survived root removal", 0);
    verify(chat->toPlainText().contains(QStringLiteral("survived root removal")), "promoted family retains independent conversation document");
    for (void *key : {static_cast<void *>(&familyZKey), static_cast<void *>(&familyAKey), static_cast<void *>(&familyDialogKey)}) { qt_fe_remove_session(key); engine.remove(key); }
    ToolTestEngine::sessionServers.clear(); setNative("gui_tab_sort", "0"); qt_fe_gui_control(channel, 8, 0); qt_fe_select_session(channel);
    qt_fe_gui_control(channel, 7, 1);
    auto *bindingView = static_cast<ConversationWindow *>(window->findChild<QMainWindow *>(QStringLiteral("detachedConversation")));
    bindingView->input->setText(QStringLiteral("detached binding draft"));
    const int beforeDetachedBinding = commands.size();
    QKeyEvent detachedBinding(QEvent::KeyPress, Qt::Key_F10, Qt::NoModifier); QApplication::sendEvent(bindingView->input, &detachedBinding);
    verify(commands.size() == beforeDetachedBinding + 2 && commands.last().session == channel && bindingView->input->text() == QStringLiteral("detached binding draft"), "detached bindings execute on source without changing its draft");
    keyHook = [](void *) { return 1; };
    QApplication::sendEvent(bindingView->input, &detachedBinding);
    verify(commands.size() == beforeDetachedBinding + 2 && pluginKeys.last().session == channel, "detached plugin key hook uses source conversation and suppresses its binding");
    keyHook = {};
    QKeyEvent detachedInsert(QEvent::KeyPress, Qt::Key_X, Qt::AltModifier); bindingView->input->selectAll(); QApplication::sendEvent(bindingView->input, &detachedInsert);
    verify(bindingView->input->text() == QStringLiteral("inserted"), "detached insert binding edits detached input");
    qt_fe_gui_control(channel, 7, 2); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_gui_control(channel, 7, 1);
    bindingView = static_cast<ConversationWindow *>(window->findChild<QMainWindow *>(QStringLiteral("detachedConversation")));
    QPointer<QMainWindow> closingBindingView(bindingView);
    const int beforeBindingClose = commands.size(); QKeyEvent closeBinding(QEvent::KeyPress, Qt::Key_F11, Qt::NoModifier); QApplication::sendEvent(bindingView->input, &closeBinding);
    verify(commands.size() == beforeBindingClose + 1 && commands.last().line == QStringLiteral("/close"), "closing source conversation stops remaining keyboard commands");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    verify(!closingBindingView, "closing a detached binding source destroys its view safely");
    // Stay within the offscreen platform's available screen geometry.
    action("trayEnabled")->setChecked(false);
    action("notificationPreviews")->setChecked(true);
    action("notifications")->setChecked(false);
    action("closeToTray")->setChecked(true);
    action("rankUsers")->setChecked(false);
    window->resize(700, 500);
    window->close();
    verify(quits == 1, "close exits through engine once");
    QSettings saved(directory.filePath(QStringLiteral("qt-frontend.ini")), QSettings::IniFormat);
    verify(saved.contains(detachedGeometryKey + QStringLiteral("/geometry")), "detached placement is persisted to disk on shutdown");
    verify(saved.value(QStringLiteral("display/scrollback")).toInt() == 100 &&
           !saved.contains(QStringLiteral("view/users")) &&
           !saved.value(QStringLiteral("window/geometry")).toByteArray().isEmpty(), "settings saved in engine config directory");
    delete window;
    window = nullptr;
    ui();
    qt_fe_new_session(server, "Server", 1);
    qt_fe_new_session(query, "Alice", 0);
    verify(!action("showChannelModes")->isChecked() && !action("showUsers")->isChecked() && !action("showTopic")->isChecked() &&
           !action("autoScroll")->isChecked(), "view preferences restored");
    verify(!action("trayEnabled")->isChecked() && action("notificationPreviews")->isChecked() &&
           !action("notifications")->isChecked() && action("closeToTray")->isChecked() && !action("rankUsers")->isChecked(),
           "desktop and user sorting preferences restored");
    CustomizationTestEngine::lists[QT_CONFIG_DIALOG_BUTTONS] = {{QStringLiteral("Inspect"), QStringLiteral("echo %s")}, {QStringLiteral("DCC"), QStringLiteral("dcc chat %s")}};
    qt_fe_customization_changed(QT_CONFIG_DIALOG_BUTTONS);
    qt_fe_select_session(query);
    auto customButtons = window->findChildren<QPushButton *>(QStringLiteral("customCommandButton"));
    QPushButton *inspect = nullptr, *dcc = nullptr;
    for (auto *button : customButtons) { if (button->text() == QStringLiteral("Inspect")) inspect = button; if (button->text() == QStringLiteral("DCC")) dcc = button; }
    verify(inspect && inspect->isEnabled() && dcc && dcc->isEnabled(), "dialog button refresh enables file and direct-chat commands");
    if (inspect) inspect->click();
    verify(CustomizationTestEngine::lastSession == query && CustomizationTestEngine::lastTarget == QStringLiteral("Alice"), "dialog custom button expands original private-chat target");
    qt_fe_gui_control(query, 7, 1);
    auto *queryView = static_cast<ConversationWindow *>(window->findChild<QMainWindow *>(QStringLiteral("detachedConversation")));
    queryView->showDialogButtons->setChecked(true);
    verify(queryView->modeRow->isHidden() && queryView->userPanel->isHidden() && !queryView->dialogButtons->isHidden(), "detached query shows dialog buttons while hiding channel-only panels");
    auto *queryInspect = queryView->dialogButtons->findChild<QPushButton *>(QStringLiteral("customCommandButton"));
    qt_fe_select_session(server); if (queryInspect) queryInspect->click();
    verify(CustomizationTestEngine::lastSession == query && CustomizationTestEngine::lastTarget == QStringLiteral("Alice"), "detached query button retains original target after main selection changes");
    qt_fe_gui_control(query, 7, 2); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_select_session(server);
    verify(window->size() == QSize(700, 500), "window size restored");
    chat = window->findChild<QTextBrowser *>(QStringLiteral("chat"));
    verify(chat->font().pointSize() == fontSize + 2 && chat->document()->maximumBlockCount() == 100,
           "font and scrollback restored");
    verify(chat->lineWrapMode() == QTextEdit::NoWrap, "wrapping preference restored");
    verify(!chat->palette().brush(QPalette::Base).texture().isNull(), "background image loads again after frontend restart");
    QTimer::singleShot(0, window, [&]() { auto *dialog = window->findChild<QDialog *>(QStringLiteral("preferencesDialog")); dialog->findChild<QLineEdit *>(QStringLiteral("chatBackground"))->clear(); dialog->accept(); });
    trigger("preferences"); verify(chat->palette().brush(QPalette::Base).texture().isNull(), "clearing background restores plain chat color");
    auto *restoredInput = window->findChild<QLineEdit *>(QStringLiteral("messageInput"));
    verify(restoredInput->font().pointSize() == fontSize + 2 && !window->findChild<QPushButton *>(QStringLiteral("inputNickname"))->isHidden(), "input appearance preferences survive restart");
    qt_fe_gui_control(query, 7, 1);
    auto *closingView = static_cast<ConversationWindow *>(window->findChild<QMainWindow *>(QStringLiteral("detachedConversation")));
    verify(closingView && closingView->isVisible(), "second conversation can detach");
    qt_fe_gui_control(query, 7, 2);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_select_session(server);
    trigger("closeConversation");
    verify(activated == query, "closing active conversation picks remaining row");
    trigger("closeConversation");
    verify(!action("closeConversation")->isEnabled() && chat->document()->isEmpty(), "closing final conversation leaves safe empty UI");
    delete window;
    window = nullptr;
    delete app;
    app = nullptr;
    return ok ? 0 : 1;
}
