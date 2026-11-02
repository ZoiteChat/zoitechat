/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_CUSTOMIZATION_H
#define ZOITECHAT_QT_CUSTOMIZATION_H
#include <QObject>
#include <QPointer>
#include <QList>
#include <functional>
class QWidget;
class QMenu;
class QMenuBar;
class QAction;
void showConfigEditor(int kind, QWidget *parent);
struct CustomPopupContext {
    std::function<void(const QString &)> command;
    std::function<void(void *, const QString &, bool)> dynamic;
};
QList<QPair<QString, QString>> customDefinitions(int kind);
bool customCommandSupported(const QString &command);
void populateCustomPopup(QMenu *menu, const QString &root, int kind, const CustomPopupContext &context);
class CustomMenus final : public QObject {
public:
    CustomMenus(QMenuBar *bar, QMenu *usermenu, QWidget *parent,
                std::function<void(const QString &)> command,
                std::function<void(void *, const QString &, bool)> dynamic);
    void refreshUsermenu();
    void refreshDynamic();
    void schedule(bool usermenu, bool dynamic);
private:
    QMenuBar *bar_;
    QMenu *usermenu_;
    QWidget *window_;
    QList<QPointer<QAction>> dynamicActions_;
    std::function<void(const QString &)> command_;
    std::function<void(void *, const QString &, bool)> dynamic_;
    bool pending_ = false, userDirty_ = false, dynamicDirty_ = false;
};
#endif
