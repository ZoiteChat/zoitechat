/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_DESKTOPINTEGRATION_H
#define ZOITECHAT_QT_DESKTOPINTEGRATION_H
#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QIcon>
#include <QPointer>
#include <functional>
class QWidget;
class QSystemTrayIcon;
class QMenu;
class QTimer;
enum class DesktopAwayState { Disconnected, Mixed, Away, Back };
struct DesktopAlertPolicy { bool notifications = true, trayActivity = true, flash = true, forceNotifications = false, ordinaryMessages = false; };
class DesktopIntegration final : public QObject {
public:
    DesktopIntegration(QWidget *window, std::function<void(void *)> focus, std::function<void()> quit);
    void configure(bool tray, bool notifications, bool previews, bool closeToTray);
    bool alert(void *session, const QString &title, const QString &text, bool mention, bool privateMessage, bool focused, bool forceNotifications = false, bool trayActivity = true, bool flash = true);
    bool alert(void *session, const QString &title, const QString &text, bool mention, bool privateMessage, bool focused, const DesktopAlertPolicy &policy);
    void attention();
    void setUnread(bool unread);
    void setUnreadBlink(bool enabled);
    bool setCustomIcon(const QString &file);
    bool setFlashingIcons(const QString &first, const QString &second, int interval);
    void setToolTip(const QString &text);
    void forget(void *session);
    void markRead(void *session);
    bool canHide() const;
    bool hideOnClose() const;
    void toggleWindow();
    bool hideWindow();
    void showWindow();
    std::function<void(bool)> hiddenChanged;
    std::function<void(bool)> awayRequested;
    std::function<DesktopAwayState()> awayState;
    std::function<void(QMenu *)> decoratePopup;
    // Accepted notifications can be observed without requiring a desktop daemon.
    std::function<void(const QString &, const QString &)> notificationAccepted;
private:
    void restore();
    QPointer<QWidget> window_;
    QSystemTrayIcon *tray_;
    QMenu *menu_;
    QTimer *flashTimer_;
    QIcon firstIcon_, secondIcon_;
    bool flashRequested_ = false, secondFrame_ = false, normalUnread_ = false;
    std::function<void(void *)> focus_;
    QElapsedTimer clock_;
    QHash<void *, qint64> last_;
    void *latest_ = nullptr;
    bool notifications_ = true, previews_ = false, closeToTray_ = false;
    bool trayRequested_ = true, unreadBlink_ = true, hiddenToTray_ = false;
};
#endif
