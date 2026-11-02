/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "desktopintegration.h"
#include <QApplication>
#include <QSystemTrayIcon>
#include <QWidget>
#include <cstdio>
#include <QImage>
#include <QTemporaryDir>
#include <QTimer>
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget parent;
    int focused = 0, quits = 0, notifications = 0;
    QString heading, body;
    DesktopIntegration desktop(&parent, [&](void *) { ++focused; }, [&]() { ++quits; });
    desktop.notificationAccepted = [&](const QString &title, const QString &text) { ++notifications; heading = title; body = text; };
    desktop.configure(false, true, false, true);
    int visibilityChanges = 0;
    desktop.hiddenChanged = [&](bool) { ++visibilityChanges; };
    parent.show();

    bool ok = true;
    auto verify = [&](bool condition, const char *message) { if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    verify(!desktop.hideWindow() && parent.isVisible() && visibilityChanges == 0, "unavailable tray refuses hide and emits no away transition");
    desktop.showWindow(); desktop.showWindow();
    verify(visibilityChanges == 0, "restoring a window that was not hidden to tray emits no back transition");

    int first, second;
    verify(!desktop.canHide() && !desktop.hideOnClose(), "close-to-tray cannot hide a window without a usable tray");
    verify(!desktop.alert(&first, "#chat", "secret content", true, false, true), "focused window suppresses notification");
    verify(!desktop.alert(&first, "#chat", "ordinary message", false, false, false), "ordinary channel traffic does not notify");
    verify(desktop.alert(&first, "#chat", "secret content", true, false, false), "background mention accepted");
    verify(notifications == 1 && !body.contains(QStringLiteral("secret content")) && heading.contains(QStringLiteral("#chat")), "preview disabled by default");
    verify(!desktop.alert(&first, "#chat", "duplicate", true, false, false), "notification cooldown is per conversation");
    verify(desktop.alert(&second, "Alice", "private", false, true, false), "private message in another conversation accepted");
    desktop.configure(false, false, true, false);
    desktop.forget(&first);
    verify(!desktop.alert(&first, "#chat", "disabled", true, false, false), "notifications can be disabled");
    verify(desktop.alert(&first, "#chat", "explicit per-conversation", false, false, false, true, false, false), "explicit per-conversation On can notify ordinary traffic with global notifications disabled");
    desktop.forget(&first);
    desktop.configure(false, true, true, false);
    verify(desktop.alert(&first, "#chat", QString(400, QLatin1Char('x')), true, false, false) && body.size() == 240, "opt-in preview is bounded");
    desktop.forget(&first);
    verify(desktop.alert(&first, "#new", "after reuse", true, false, false), "closed session address does not retain cooldown");
    desktop.markRead(&first);
    desktop.setUnread(false); desktop.setToolTip(QStringLiteral("Custom tooltip"));
    verify(parent.findChild<QSystemTrayIcon *>()->toolTip() == QStringLiteral("Custom tooltip"), "core tooltip override");
    desktop.setToolTip(QString());
    verify(parent.findChild<QSystemTrayIcon *>()->toolTip() == QStringLiteral("ZoiteChat"), "empty tooltip restores default");
    int trayOnly, ordinary;
    desktop.configure(false, false, true, false); desktop.setUnread(false);
    auto *policyTray = parent.findChild<QSystemTrayIcon *>(); const auto quietIcon = policyTray->icon().cacheKey();
    DesktopAlertPolicy trayPolicy; trayPolicy.notifications = false; trayPolicy.trayActivity = true; trayPolicy.flash = false;
    const int beforeTrayOnly = notifications;
    verify(!desktop.alert(&trayOnly, "#channel", "tray activity", false, false, false, trayPolicy) && notifications == beforeTrayOnly && policyTray->icon().cacheKey() != quietIcon, "tray activity works when notifications are disabled");
    auto *normalTimer = desktop.findChild<QTimer *>(QStringLiteral("trayFlashTimer")); const auto unreadFrame = policyTray->icon().cacheKey();
    QMetaObject::invokeMethod(normalTimer, "timeout", Qt::DirectConnection); const auto normalFrame = policyTray->icon().cacheKey();
    verify(normalFrame != unreadFrame, "normal unread tray activity alternates icon frames");
    desktop.setUnread(true); verify(policyTray->icon().cacheKey() == normalFrame, "repeated unread activity preserves blink phase");
    desktop.setUnreadBlink(false); const auto steadyUnread = policyTray->icon().cacheKey();
    QMetaObject::invokeMethod(normalTimer, "timeout", Qt::DirectConnection);
    verify(!normalTimer->isActive() && policyTray->icon().cacheKey() == steadyUnread, "disabling native blinking retains steady unread icon and stops animation");
    desktop.setUnread(true);
    verify(policyTray->icon().cacheKey() == steadyUnread, "repeated unread activity preserves steady nonblinking icon");
    desktop.setUnreadBlink(true); const auto resumedUnread = policyTray->icon().cacheKey();
    QMetaObject::invokeMethod(normalTimer, "timeout", Qt::DirectConnection);
    verify(policyTray->icon().cacheKey() != resumedUnread, "reenabling blink preference restores unread animation");

    desktop.configure(false, true, true, false);
    DesktopAlertPolicy ordinaryPolicy; ordinaryPolicy.ordinaryMessages = true; ordinaryPolicy.trayActivity = false; ordinaryPolicy.flash = false;
    verify(desktop.alert(&ordinary, "#channel", "ordinary enabled", false, false, false, ordinaryPolicy), "category policy can enable ordinary channel notifications");
    desktop.setUnread(false);
    desktop.setUnreadBlink(false); // Explicit plugin animations remain independent of normal unread blinking.
    QTemporaryDir icons;
    QImage red(32, 32, QImage::Format_ARGB32); red.fill(Qt::red);
    QImage green(32, 32, QImage::Format_ARGB32); green.fill(Qt::green);
    const QString firstIcon = icons.filePath(QStringLiteral("first.png")), secondIcon = icons.filePath(QStringLiteral("second.png"));
    verify(red.save(firstIcon) && green.save(secondIcon), "tray icon fixtures saved");
    auto *tray = parent.findChild<QSystemTrayIcon *>();
    auto *timer = desktop.findChild<QTimer *>(QStringLiteral("trayFlashTimer"));
    auto center = [&]() { return tray->icon().pixmap(32, 32).toImage().pixelColor(16, 16); };
    verify(desktop.setCustomIcon(firstIcon) && center() == QColor(Qt::red), "custom tray file loads without requiring a system tray");
    const auto previousIcon = tray->icon().cacheKey();
    verify(!desktop.setCustomIcon(icons.filePath(QStringLiteral("missing.png"))) && tray->icon().cacheKey() == previousIcon, "invalid icon preserves current tray image");
    verify(desktop.setFlashingIcons(firstIcon, secondIcon, -1) && timer->interval() == 500 && !timer->isActive(), "native default flash interval is stored while unavailable tray stays idle");
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    verify(center() == QColor(Qt::green), "flash advances to second custom image");
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    verify(center() == QColor(Qt::red), "flash returns to first image");
    const auto validFlash = tray->icon().cacheKey();
    verify(!desktop.setFlashingIcons(firstIcon, icons.filePath(QStringLiteral("missing.png")), 100) && tray->icon().cacheKey() == validFlash, "invalid second image does not replace active flash state");
    verify(desktop.setFlashingIcons(firstIcon, QString(), 1) && timer->interval() == 50, "too-fast flash intervals are bounded");
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    verify(tray->icon().pixmap(32, 32).toImage().pixelColor(0, 0).alpha() == 0, "missing second image flashes against normal icon");
    desktop.setUnread(false); const auto resetIcon = tray->icon().cacheKey(); QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    verify(!timer->isActive() && tray->icon().cacheKey() == resetIcon, "normal unread reset cancels custom flashing");
    verify(desktop.setCustomIcon(firstIcon) && desktop.setCustomIcon(QString()) && tray->icon().pixmap(32, 32).toImage().pixelColor(0, 0).alpha() == 0, "empty filename restores normal icon");
    parent.hide(); desktop.toggleWindow();
    verify(parent.isVisible() && focused == 0 && quits == 0, "restore works without a tray and does not quit or select stale context");
    return ok ? 0 : 1;
}
