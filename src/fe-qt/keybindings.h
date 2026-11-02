/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_KEYBINDINGS_H
#define ZOITECHAT_QT_KEYBINDINGS_H
#include <QObject>
#include <QKeySequence>
#include <QStringList>
#include <functional>
class QWidget;
class QKeyEvent;
struct PluginKey { unsigned state = 0, keyval = 0, unicode = 0; };
PluginKey pluginKey(const QKeyEvent &event);
struct InputBinding { QString key, action, first, second; };
QList<InputBinding> defaultInputBindings(const QString &commandPrefix = QStringLiteral("/"));
QKeySequence inputBindingKey(const QString &key);
bool inputBindingSupported(const QString &action);
QStringList inputBindingCommands(const QString &data);
class InputBindings final : public QObject {
public:
    InputBindings(QWidget *input, const QString &path, std::function<bool(const InputBinding &)> run, std::function<QString()> commandPrefix = {});
    bool reload(QString *error = nullptr);
    void edit(QWidget *parent);
    std::function<void()> applied;
    std::function<bool(const QKeyEvent &)> keypress;
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    std::function<QString()> commandPrefix_;
    QWidget *input_; QString path_; QList<QPair<QKeySequence, InputBinding>> bindings_; std::function<bool(const InputBinding &)> run_;
};
#endif
