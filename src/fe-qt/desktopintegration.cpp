/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "desktopintegration.h"
#include <QApplication>
#include <QActionGroup>
#include <QIcon>
#include <QFileInfo>
#include <QImageReader>
#include <QMenu>
#include <QPainter>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWidget>

namespace {
QIcon fileIcon(const QString &path)
{
    const QFileInfo file(path);
    if (!file.isFile() || !file.isReadable() || file.size() > 16 * 1024 * 1024) return {};
    QImageReader reader(path); const QSize size = reader.size();
    if (!reader.canRead() || !size.isValid() || size.width() > 4096 || size.height() > 4096) return {};
    reader.setScaledSize(size.scaled(64, 64, Qt::KeepAspectRatio));
    const QImage image = reader.read(); return image.isNull() ? QIcon() : QIcon(QPixmap::fromImage(image));
}
QIcon chatIcon(bool unread)
{
    QPixmap pixmap(32, 32); pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap); painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen); painter.setBrush(unread ? QColor(220, 115, 30) : QColor(45, 135, 165));
    painter.drawRoundedRect(QRectF(2, 3, 28, 23), 6, 6);
    painter.drawPolygon(QPolygonF({QPointF(7, 23), QPointF(7, 31), QPointF(16, 23)}));
    painter.setPen(QPen(Qt::white, 2)); painter.drawLine(8, 11, 24, 11); painter.drawLine(8, 17, 20, 17);
    return QIcon(pixmap);
}
}
DesktopIntegration::DesktopIntegration(QWidget *window, std::function<void(void *)> focus, std::function<void()> quit)
    : QObject(window), window_(window), focus_(std::move(focus))
{
    setObjectName(QStringLiteral("desktopIntegration"));
    clock_.start();
    tray_ = new QSystemTrayIcon(chatIcon(false), this);
    tray_->setToolTip(QStringLiteral("ZoiteChat"));
    flashTimer_ = new QTimer(this); flashTimer_->setObjectName(QStringLiteral("trayFlashTimer"));
    connect(flashTimer_, &QTimer::timeout, this, [this]() {
        if (!flashRequested_) return;
        secondFrame_ = !secondFrame_; tray_->setIcon(secondFrame_ ? secondIcon_ : firstIcon_);
    });
    menu_ = new QMenu(window);
    menu_->addAction(QStringLiteral("Show / Hide ZoiteChat"), this, [this]() { toggleWindow(); });
    menu_->addAction(QStringLiteral("Go to Latest Notification"), this, [this]() { restore(); if (latest_ && focus_) focus_(latest_); });
    menu_->addSeparator();
    auto *away = menu_->addAction(QStringLiteral("Away on All Servers"), this, [this]() { if (awayRequested) awayRequested(true); }); away->setObjectName(QStringLiteral("trayAway"));
    auto *back = menu_->addAction(QStringLiteral("Back on All Servers"), this, [this]() { if (awayRequested) awayRequested(false); }); back->setObjectName(QStringLiteral("trayBack"));
    away->setEnabled(false); back->setEnabled(false);
    menu_->addSeparator(); menu_->addAction(QStringLiteral("Quit"), this, std::move(quit));
    const auto baseActions = menu_->actions();
    connect(menu_, &QMenu::aboutToShow, this, [this, baseActions, away, back]() {
        const auto state = awayState ? awayState() : DesktopAwayState::Disconnected;
        away->setEnabled(bool(awayRequested) && state != DesktopAwayState::Disconnected && state != DesktopAwayState::Away);
        back->setEnabled(bool(awayRequested) && state != DesktopAwayState::Disconnected && state != DesktopAwayState::Back);
        for (auto *action : menu_->actions()) if (!baseActions.contains(action)) {
            menu_->removeAction(action);
            if (action->menu()) action->menu()->deleteLater(); else action->deleteLater();
        }
        const auto groups = menu_->findChildren<QActionGroup *>(QString(), Qt::FindDirectChildrenOnly);
        qDeleteAll(groups);
        if (decoratePopup) decoratePopup(menu_);
    });
    tray_->setContextMenu(menu_);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) toggleWindow();
    });
    connect(tray_, &QSystemTrayIcon::messageClicked, this, [this]() { restore(); if (latest_ && focus_) focus_(latest_); });
    auto *availability = new QTimer(this);
    connect(availability, &QTimer::timeout, this, [this]() { configure(trayRequested_, notifications_, previews_, closeToTray_); });
    availability->start(2000);
}
void DesktopIntegration::configure(bool tray, bool notifications, bool previews, bool closeToTray)
{
    notifications_ = notifications; previews_ = previews; closeToTray_ = closeToTray;
    trayRequested_ = tray;
    const bool wasVisible = tray_->isVisible();
    const bool enabled = tray && QSystemTrayIcon::isSystemTrayAvailable();
    tray_->setVisible(enabled);
    if (enabled && flashRequested_) { if (!flashTimer_->isActive()) flashTimer_->start(); }
    else { flashTimer_->stop(); if (flashRequested_) { secondFrame_ = false; tray_->setIcon(firstIcon_); } }
    if (wasVisible && !enabled && window_ && !window_->isVisible()) restore();
}
bool DesktopIntegration::alert(void *session, const QString &title, const QString &text, bool mention, bool privateMessage, bool focused, bool forceNotifications, bool trayActivity, bool flash)
{
    return alert(session, title, text, mention, privateMessage, focused, DesktopAlertPolicy{true, trayActivity, flash, forceNotifications, false});
}
bool DesktopIntegration::alert(void *session, const QString &title, const QString &text, bool mention, bool privateMessage, bool focused, const DesktopAlertPolicy &policy)
{
    if (!session || focused) return false;
    const bool notify = policy.notifications && (notifications_ || policy.forceNotifications) && (mention || privateMessage || policy.ordinaryMessages || policy.forceNotifications);
    if (!notify && !policy.trayActivity && !policy.flash) return false;
    latest_ = session;
    if (policy.trayActivity) setUnread(true);
    if (policy.flash) attention();
    if (!notify) return false;
    const qint64 now = clock_.elapsed();
    if (last_.contains(session) && now - last_.value(session) < 10000) return false;
    last_.insert(session, now);
    const QString heading = QStringLiteral("ZoiteChat — ") + title.left(100);
    const QString body = previews_ ? text.simplified().left(240) :
        mention ? QStringLiteral("You were mentioned.") : privateMessage ? QStringLiteral("New private message.") : QStringLiteral("New channel message.");
    if (notificationAccepted) notificationAccepted(heading, body);
    if (tray_->isVisible() && QSystemTrayIcon::supportsMessages()) tray_->showMessage(heading, body, QSystemTrayIcon::Information, 5000);
    return true;
}
void DesktopIntegration::attention() { if (window_ && !window_->isActiveWindow()) QApplication::alert(window_, 3000); }
void DesktopIntegration::setUnread(bool unread)
{
    if (unread && normalUnread_) return;
    normalUnread_ = unread; flashTimer_->stop(); secondFrame_ = false;
    if (unread) {
        firstIcon_ = chatIcon(true); secondIcon_ = chatIcon(false); flashRequested_ = unreadBlink_;
        tray_->setIcon(firstIcon_); flashTimer_->setInterval(500);
        if (flashRequested_ && tray_->isVisible() && trayRequested_) flashTimer_->start();
    } else {
        flashRequested_ = false; firstIcon_ = {}; secondIcon_ = {}; tray_->setIcon(chatIcon(false));
    }
}
void DesktopIntegration::setUnreadBlink(bool enabled)
{
    if (unreadBlink_ == enabled) return;
    unreadBlink_ = enabled;
    if (normalUnread_) { normalUnread_ = false; setUnread(true); }
}
bool DesktopIntegration::setCustomIcon(const QString &file)
{
    if (file.isEmpty()) { setUnread(false); return true; }
    const QIcon icon = fileIcon(file); if (icon.isNull()) return false;
    normalUnread_ = false; flashRequested_ = false; flashTimer_->stop(); firstIcon_ = icon; secondIcon_ = {}; secondFrame_ = false;
    tray_->setIcon(icon); return true;
}
bool DesktopIntegration::setFlashingIcons(const QString &first, const QString &second, int interval)
{
    const QIcon one = fileIcon(first), two = second.isEmpty() ? chatIcon(false) : fileIcon(second);
    if (one.isNull() || two.isNull()) return false;
    normalUnread_ = false; flashTimer_->stop(); firstIcon_ = one; secondIcon_ = two; secondFrame_ = false; flashRequested_ = true;
    flashTimer_->setInterval(qBound(50, interval > 0 ? interval : 500, 60000)); tray_->setIcon(firstIcon_);
    if (tray_->isVisible() && trayRequested_) flashTimer_->start();
    return true;
}
void DesktopIntegration::setToolTip(const QString &text) { tray_->setToolTip(text.isEmpty() ? QStringLiteral("ZoiteChat") : text.left(250)); }
void DesktopIntegration::forget(void *session) { last_.remove(session); if (latest_ == session) latest_ = nullptr; }
void DesktopIntegration::markRead(void *session) { if (latest_ == session) latest_ = nullptr; }
bool DesktopIntegration::canHide() const { return tray_->isVisible() && QSystemTrayIcon::isSystemTrayAvailable(); }
bool DesktopIntegration::hideOnClose() const { return closeToTray_ && canHide(); }
bool DesktopIntegration::hideWindow()
{
    if (!window_ || !canHide()) return false;
    const bool notify = !hiddenToTray_; hiddenToTray_ = true;
    const QPointer<DesktopIntegration> guard(this);
    window_->hide();
    if (guard && notify && hiddenChanged) hiddenChanged(true);
    return true;
}
void DesktopIntegration::showWindow() { restore(); }
void DesktopIntegration::restore()
{
    if (!window_) return;
    const bool notify = hiddenToTray_; hiddenToTray_ = false;
    const QPointer<DesktopIntegration> guard(this);
    const QPointer<QWidget> window = window_;
    if (window->isMinimized()) window->showNormal(); else window->show();
    if (!guard || !window) return;
    window->raise(); window->activateWindow();
    if (guard && notify && hiddenChanged) hiddenChanged(false);
}
void DesktopIntegration::toggleWindow()
{
    if (!window_) return;
    if (window_->isVisible() && window_->isActiveWindow() && canHide()) hideWindow(); else restore();
}
