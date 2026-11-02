/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "customization.h"
#include "menumarkup.h"
#include "customization-bridge.h"
#include "qt-bridge.h"
#include <QActionGroup>
#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <QDialog>
#include <QDir>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
namespace {
QString configTitle(int kind)
{
    switch (kind) { case QT_CONFIG_REPLACE: return QStringLiteral("Auto Replace"); case QT_CONFIG_CTCP: return QStringLiteral("CTCP Replies");
        case QT_CONFIG_COMMANDS: return QStringLiteral("User Commands"); case QT_CONFIG_USERMENU: return QStringLiteral("Usermenu");
        case QT_CONFIG_BUTTONS: return QStringLiteral("User List Buttons");
        case QT_CONFIG_DIALOG_BUTTONS: return QStringLiteral("Dialog Buttons");
        case QT_CONFIG_POPUP: return QStringLiteral("User List Popup");
        case QT_CONFIG_TAB_POPUP: return QStringLiteral("Channel Tab Popup");
        case QT_CONFIG_URL_HANDLERS: return QStringLiteral("URL Handlers"); default: return QString(); }
}
class ConfigEditor final : public QDialog {
public:
    ConfigEditor(int kind, QWidget *parent) : QDialog(parent), kind_(kind)
    {
        setObjectName(QStringLiteral("configEditor%1").arg(kind)); setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(configTitle(kind)); resize(800, 450);
        auto *layout = new QVBoxLayout(this);
        const QString explanation = kind == QT_CONFIG_REPLACE ? QStringLiteral("Replace matching words on Space or Enter in normal chat input. Matching is case-sensitive.") :
            kind == QT_CONFIG_COMMANDS ? QStringLiteral("Aliases use the existing core command expansion, such as %2 for an argument and &2 for remaining arguments. Duplicate names can run multiple commands.") :
            kind == QT_CONFIG_CTCP ? QStringLiteral("Replies use the core's CTCP substitutions. For example TIME → nctcp %s TIME %t.") :
            (kind == QT_CONFIG_BUTTONS || kind == QT_CONFIG_DIALOG_BUTTONS) ? QStringLiteral("Each row creates a button. %s is the target, %a is the selected nicknames, %h is the host and %u is the account. Up to 64 buttons are displayed. !program starts an external program.") :
            QStringLiteral("Use SUB with a label to open a submenu, ENDSUB to close it, SEP for a separator, or TOGGLE label with a boolean preference name. Other rows execute IRC commands; !program starts an external program.");
        auto *help = new QLabel(explanation, this); help->setTextFormat(Qt::PlainText); help->setWordWrap(true); layout->addWidget(help);
        model_ = new QStandardItemModel(this);
        model_->setHorizontalHeaderLabels({kind == QT_CONFIG_REPLACE ? QStringLiteral("Text") : QStringLiteral("Name"),
                                           kind == QT_CONFIG_REPLACE ? QStringLiteral("Replace with") : QStringLiteral("Command")});
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("configRows")); table_->setModel(model_);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_, 1);
        auto *row = new QHBoxLayout;
        auto button = [&](const QString &text, const QString &id) { auto *b = new QPushButton(text, this); b->setObjectName(id); row->addWidget(b); return b; };
        auto *add = button(QStringLiteral("Add"), QStringLiteral("addConfigRow"));
        auto *remove = button(QStringLiteral("Remove"), QStringLiteral("removeConfigRow"));
        auto *up = button(QStringLiteral("Move Up"), QStringLiteral("configUp"));
        auto *down = button(QStringLiteral("Move Down"), QStringLiteral("configDown"));
        row->addStretch(); auto *reload = button(QStringLiteral("Reload"), QStringLiteral("reloadConfig")); layout->addLayout(row);
        error_ = new QLabel(this); error_->setObjectName(QStringLiteral("configError")); error_->setTextFormat(Qt::PlainText); error_->setWordWrap(true); layout->addWidget(error_);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this); layout->addWidget(buttons);
        connect(add, &QPushButton::clicked, this, [this]() { model_->appendRow({new QStandardItem, new QStandardItem}); table_->setCurrentIndex(model_->index(model_->rowCount() - 1, 0)); table_->edit(table_->currentIndex()); });
        connect(remove, &QPushButton::clicked, this, [this]() { if (table_->currentIndex().isValid()) model_->removeRow(table_->currentIndex().row()); });
        connect(up, &QPushButton::clicked, this, [this]() { move(-1); }); connect(down, &QPushButton::clicked, this, [this]() { move(1); });
        connect(reload, &QPushButton::clicked, this, [this]() { load(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() { save(); }); load();
    }
private:
    void move(int delta)
    {
        const int row = table_->currentIndex().row(), target = row + delta;
        if (row < 0 || target < 0 || target >= model_->rowCount()) return;
        const auto items = model_->takeRow(row); model_->insertRow(target, items); table_->selectRow(target);
    }
    void load()
    {
        model_->removeRows(0, model_->rowCount()); char revision[65] = {0};
        if (!qt_core_config_read(kind_, [](const char *name, const char *command, void *data) {
            auto *self = static_cast<ConfigEditor *>(data);
            self->model_->appendRow({new QStandardItem(QString::fromUtf8(name)), new QStandardItem(QString::fromUtf8(command))});
        }, this, revision, sizeof(revision))) error_->setText(QStringLiteral("Unable to load this list."));
        else { revision_ = revision; error_->clear(); }
    }
    void save()
    {
        QList<QByteArray> names, commands; QList<QtConfigRow> rows;
        for (int i = 0; i < model_->rowCount(); ++i) {
            names.append(model_->index(i, 0).data().toString().toUtf8()); commands.append(model_->index(i, 1).data().toString().toUtf8());
        }
        for (int i = 0; i < names.size(); ++i) rows.append(QtConfigRow{names[i].constData(), commands[i].constData()});
        char error[1024] = {0};
        if (!qt_core_config_save(kind_, rows.constData(), rows.size(), revision_.constData(), error, sizeof(error))) error_->setText(QString::fromUtf8(error));
        else accept();
    }
    int kind_;
    QByteArray revision_;
    QStandardItemModel *model_;
    QTableView *table_;
    QLabel *error_;
};
QHash<int, QPointer<ConfigEditor>> editors;
QString normalized(QString value) { return value.remove(QLatin1Char('&')).remove(QLatin1Char('_')).toCaseFolded(); }
QString mnemonic(const QString &value)
{
    QString result;
    for (int i = 0; i < value.size(); ++i) {
        if (value[i] == QLatin1Char('&')) result += QStringLiteral("&&");
        else if (value[i] == QLatin1Char('_')) {
            if (i + 1 < value.size() && value[i + 1] == QLatin1Char('_')) { result += QLatin1Char('_'); ++i; }
            else result += QLatin1Char('&');
        } else result += value[i];
    }
    return result;
}
struct ListRow { QString name, command; };
struct DynamicRow { void *id; QString path, label, command, uncheck, group; bool nullLabel, submenu, checkable, radio; int position, checked, enabled, key, modifiers; QString icon, markup; };
QIcon menuIcon(const QString &name)
{
    if (name.isEmpty()) return {};
    QIcon file(name);
    if (!file.pixmap(16, 16).isNull()) return file;
    const QString config = QString::fromUtf8(qt_core_config_dir());
    if (!config.isEmpty()) {
        QIcon local(QDir(config).filePath(name));
        if (!local.pixmap(16, 16).isNull()) return local;
    }
    static const QHash<QString, QString> aliases{
        {"gtk-add", "list-add"}, {"gtk-remove", "list-remove"}, {"gtk-delete", "edit-delete"},
        {"gtk-open", "document-open"}, {"gtk-save", "document-save"}, {"gtk-save-as", "document-save-as"},
        {"gtk-new", "document-new"}, {"gtk-close", "window-close"}, {"gtk-quit", "application-exit"},
        {"gtk-copy", "edit-copy"}, {"gtk-cut", "edit-cut"}, {"gtk-paste", "edit-paste"},
        {"gtk-find", "edit-find"}, {"gtk-preferences", "preferences-system"},
        {"gtk-help", "help-browser"}, {"gtk-about", "help-about"}, {"gtk-refresh", "view-refresh"},
        {"gtk-go-back", "go-previous"}, {"gtk-go-forward", "go-next"}, {"gtk-home", "go-home"}
    };
    const QIcon theme = QIcon::fromTheme(aliases.value(name, name));
    if (!theme.pixmap(16, 16).isNull()) return theme;
    static const QHash<QString, QStyle::StandardPixmap> standard{
        {"gtk-open", QStyle::SP_DialogOpenButton}, {"gtk-save", QStyle::SP_DialogSaveButton},
        {"gtk-close", QStyle::SP_DialogCloseButton}, {"gtk-cancel", QStyle::SP_DialogCancelButton},
        {"gtk-ok", QStyle::SP_DialogOkButton}, {"gtk-apply", QStyle::SP_DialogApplyButton},
        {"gtk-help", QStyle::SP_DialogHelpButton}, {"gtk-refresh", QStyle::SP_BrowserReload},
        {"gtk-go-back", QStyle::SP_ArrowBack}, {"gtk-go-forward", QStyle::SP_ArrowForward},
        {"gtk-home", QStyle::SP_DirHomeIcon}
    };
    const auto found = standard.constFind(name);
    return found == standard.cend() ? QIcon() : QApplication::style()->standardIcon(found.value());
}
void setMenuIcon(QAction *action, const DynamicRow &row)
{
    if (!row.nullLabel) { action->setIcon(menuIcon(row.icon)); action->setIconVisibleInMenu(true); }
}
QPair<QString, QString> menuLabel(const QString &name)
{
    int start = -1;
    for (int i = 0; i < name.size(); ++i) if (name[i] == QLatin1Char('~') && (i == 0 || name[i - 1] != QLatin1Char('\\'))) {
        if (start < 0) start = i;
        else return {name.left(start), name.mid(start + 1, i - start - 1)};
    }
    return {name, {}};
}
QKeySequence menuShortcut(int key, int modifiers)
{
    int qtKey = 0;
    if (key >= 0xffbe && key <= 0xffe0) qtKey = Qt::Key_F1 + key - 0xffbe;
    else if (key > 0 && key < 0xff00) qtKey = QChar(key).toUpper().unicode();
    if (!qtKey) return {};
    Qt::KeyboardModifiers qtModifiers;
    if (modifiers & 1) qtModifiers |= Qt::ShiftModifier;
    if (modifiers & 4) qtModifiers |= Qt::ControlModifier;
    if (modifiers & 8) qtModifiers |= Qt::AltModifier;
    if (modifiers & (1 << 26)) qtModifiers |= Qt::MetaModifier;
    return QKeySequence(QKeyCombination(qtModifiers, static_cast<Qt::Key>(qtKey)));
}
}
void showConfigEditor(int kind, QWidget *parent)
{
    if (configTitle(kind).isEmpty()) return;
    if (!editors.value(kind)) editors[kind] = new ConfigEditor(kind, parent);
    editors[kind]->show(); editors[kind]->raise(); editors[kind]->activateWindow();
}
CustomMenus::CustomMenus(QMenuBar *bar, QMenu *usermenu, QWidget *parent,
                         std::function<void(const QString &)> command,
                         std::function<void(void *, const QString &, bool)> dynamic)
    : QObject(parent), bar_(bar), usermenu_(usermenu), window_(parent), command_(std::move(command)), dynamic_(std::move(dynamic))
{
    connect(usermenu_, &QMenu::aboutToShow, this, [this]() { refreshUsermenu(); });
    refreshUsermenu(); refreshDynamic();
}
void CustomMenus::schedule(bool usermenu, bool dynamic)
{
    userDirty_ |= usermenu; dynamicDirty_ |= dynamic;
    if (pending_) return;
    pending_ = true;
    QTimer::singleShot(0, this, [this]() {
        pending_ = false; const bool user = userDirty_, dynamic = dynamicDirty_; userDirty_ = dynamicDirty_ = false;
        if (user) refreshUsermenu();
        if (dynamic) refreshDynamic();
    });
}
void CustomMenus::refreshUsermenu()
{
    QList<ListRow> rows; char revision[65];
    qt_core_config_read(QT_CONFIG_USERMENU, [](const char *name, const char *command, void *data) {
        static_cast<QList<ListRow> *>(data)->append({QString::fromUtf8(name), QString::fromUtf8(command)});
    }, &rows, revision, sizeof(revision));
    QList<QPointer<QMenu>> oldSubmenus;
    for (auto *action : usermenu_->actions()) if (action->menu()) oldSubmenus.append(action->menu());
    usermenu_->clear();
    for (const auto &menu : oldSubmenus) if (menu) menu->deleteLater();
    QList<QMenu *> stack{usermenu_};
    for (const auto &row : rows) {
        if (row.name.compare(QStringLiteral("SUB"), Qt::CaseInsensitive) == 0) {
            if (stack.size() <= 16) stack.append(stack.last()->addMenu(mnemonic(row.command)));
            continue;
        }
        if (row.name.compare(QStringLiteral("ENDSUB"), Qt::CaseInsensitive) == 0) { if (stack.size() > 1) stack.removeLast(); continue; }
        if (row.name.compare(QStringLiteral("SEP"), Qt::CaseInsensitive) == 0) { stack.last()->addSeparator(); continue; }
        const bool toggle = row.name.startsWith(QStringLiteral("TOGGLE "), Qt::CaseInsensitive);
        const auto label = toggle ? qMakePair(row.name.mid(7), QString()) : menuLabel(row.name);
        auto *action = stack.last()->addAction(mnemonic(label.first));
        action->setIcon(menuIcon(label.second)); action->setIconVisibleInMenu(true);
        action->setObjectName(QStringLiteral("userMenuAction"));
        if (toggle) {
            const int value = qt_core_custom_pref(row.command.toUtf8().constData());
            action->setCheckable(true); action->setChecked(value > 0); action->setEnabled(value >= 0);
        }
        else action->setEnabled(!row.command.isEmpty());
        connect(action, &QAction::triggered, this, [this, row, toggle](bool checked) {
            command_(toggle ? QStringLiteral("set %1 %2").arg(row.command).arg(checked ? 1 : 0) : row.command);
        });
    }
    usermenu_->addSeparator();
    auto *edit = usermenu_->addAction(QStringLiteral("Edit This Menu…")); edit->setObjectName(QStringLiteral("editUsermenu"));
    connect(edit, &QAction::triggered, this, [this]() { showConfigEditor(QT_CONFIG_USERMENU, window_); });
}
void CustomMenus::refreshDynamic()
{
    for (auto it = dynamicActions_.crbegin(); it != dynamicActions_.crend(); ++it) if (*it) {
        auto *action = it->data(); action->setEnabled(false);
        action->setShortcuts({});
        const auto owners = action->associatedObjects();
        for (auto *owner : owners) if (auto *widget = qobject_cast<QWidget *>(owner)) widget->removeAction(action);
        if (action->menu()) action->menu()->deleteLater(); else action->deleteLater();
    }
    qDeleteAll(findChildren<QActionGroup *>(QString(), Qt::FindDirectChildrenOnly));
    dynamicActions_.clear(); QList<DynamicRow> rows;
    qt_core_menus_foreach([](const QtDynamicMenu *entry, void *data) {
        if (!entry->main_menu) return;
        static_cast<QList<DynamicRow> *>(data)->append({entry->identity, QString::fromUtf8(entry->path ? entry->path : ""),
            QString::fromUtf8(entry->label ? entry->label : ""), QString::fromUtf8(entry->command ? entry->command : ""),
            QString::fromUtf8(entry->uncheck_command ? entry->uncheck_command : ""), QString::fromUtf8(entry->group ? entry->group : ""),
            entry->label == nullptr, entry->label && !entry->command && !entry->uncheck_command && !entry->group,
            entry->uncheck_command || entry->group, entry->group != nullptr, entry->position, entry->checked, entry->enabled, entry->key, entry->modifiers, QString::fromUtf8(entry->icon ? entry->icon : ""), QString::fromUtf8(entry->markup ? entry->markup : "")});
    }, &rows);
    QHash<QString, QActionGroup *> groups;
    for (int pass = 0; !rows.isEmpty() && pass <= 16; ++pass) {
        bool progress = false;
        for (int i = 0; i < rows.size();) {
            const auto row = rows[i]; QWidget *parent = bar_; bool found = true;
            for (const QString &part : row.path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
                QMenu *menu = nullptr;
                for (auto *action : parent->actions()) if (action->menu() && normalized(action->text()) == normalized(part)) { menu = action->menu(); break; }
                if (!menu) { found = false; break; } parent = menu;
            }
            if (!found) { ++i; continue; }
            QAction *action;
            if (row.submenu) { auto *menu = new QMenu(mnemonic(row.label), parent); action = menu->menuAction(); }
            else { action = new QAction(row.nullLabel ? QString() : mnemonic(row.label), parent); action->setSeparator(row.nullLabel); }
            action->setObjectName(QStringLiteral("dynamicMenuAction")); action->setProperty("coreMenuId", QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(row.id)));
            const auto siblings = parent->actions(); int position = row.position;
            if (position < 0) position = qMax(0, int(siblings.size()) + position);
            if (position != 0xffff && position >= 0 && position < siblings.size()) parent->insertAction(siblings[position], action); else parent->addAction(action);
            setMenuIcon(action, row);
            setMenuMarkup(action, row.markup, parent);
            action->setEnabled(row.enabled); action->setCheckable(row.checkable); action->setChecked(row.checked);
            if (row.radio) {
                const QString key = normalized(row.path) + QLatin1Char('/') + normalized(row.group);
                if (!groups.contains(key)) groups[key] = new QActionGroup(this);
                groups[key]->addAction(action);
            }
            const auto shortcut = menuShortcut(row.key, row.modifiers);
            if (!shortcut.isEmpty()) {
                bool conflict = false;
                for (auto *existing : window_->findChildren<QAction *>()) if (existing != action && existing->shortcuts().contains(shortcut)) conflict = true;
                if (!conflict) action->setShortcut(shortcut);
            }
            if (!row.submenu && !row.nullLabel) connect(action, &QAction::triggered, this, [this, row](bool checked) { dynamic_(row.id, row.command, checked); });
            dynamicActions_.append(action); rows.removeAt(i); progress = true;
        }
        if (!progress) break;
    }
}

QList<QPair<QString, QString>> customDefinitions(int kind)
{
    QList<QPair<QString, QString>> rows;
    char revision[65];
    qt_core_config_read(kind, [](const char *name, const char *command, void *data) {
        static_cast<QList<QPair<QString, QString>> *>(data)->append({QString::fromUtf8(name), QString::fromUtf8(command)});
    }, &rows, revision, sizeof(revision));
    return rows;
}

bool customCommandSupported(const QString &command)
{
    QString text = command.trimmed();
    if (text.startsWith(QLatin1Char('/'))) text.remove(0, 1);
    return !text.trimmed().isEmpty();
}

void populateCustomPopup(QMenu *menu, const QString &root, int kind, const CustomPopupContext &context)
{
    const auto definitions = kind >= 0 ? customDefinitions(kind) : QList<QPair<QString, QString>>{};
    if (!definitions.isEmpty()) {
        auto *custom = menu->addMenu(kind == QT_CONFIG_URL_HANDLERS ? QStringLiteral("Open With") : QStringLiteral("Custom Actions"));
        QList<QMenu *> stack{custom};
        for (const auto &row : definitions) {
            if (row.first.compare(QStringLiteral("SUB"), Qt::CaseInsensitive) == 0) {
                if (stack.size() <= 16) stack.append(stack.last()->addMenu(mnemonic(row.second)));
                continue;
            }
            if (row.first.compare(QStringLiteral("ENDSUB"), Qt::CaseInsensitive) == 0) { if (stack.size() > 1) stack.removeLast(); continue; }
            if (row.first.compare(QStringLiteral("SEP"), Qt::CaseInsensitive) == 0) { stack.last()->addSeparator(); continue; }
            const bool toggle = row.first.startsWith(QStringLiteral("TOGGLE "), Qt::CaseInsensitive);
            const auto label = toggle ? qMakePair(row.first.mid(7), QString()) : menuLabel(row.first);
            auto *action = stack.last()->addAction(mnemonic(label.first));
            action->setIcon(menuIcon(label.second)); action->setIconVisibleInMenu(true);
            action->setObjectName(QStringLiteral("customPopupAction"));
            const int value = toggle ? qt_core_custom_pref(row.second.toUtf8().constData()) : 0;
            action->setCheckable(toggle); action->setChecked(value > 0);
            action->setEnabled(bool(context.command) && (toggle ? value >= 0 : customCommandSupported(row.second)));
            if (!toggle && !customCommandSupported(row.second)) action->setToolTip(QStringLiteral("No command is configured."));
            QObject::connect(action, &QAction::triggered, menu, [context, row, toggle](bool checked) {
                if (context.command) context.command(toggle ? QStringLiteral("set %1 %2").arg(row.second).arg(checked ? 1 : 0) : row.second);
            });
        }
    }
    QList<DynamicRow> rows;
    qt_core_menus_foreach([](const QtDynamicMenu *entry, void *data) {
        if (entry->main_menu) return;
        static_cast<QList<DynamicRow> *>(data)->append({entry->identity, QString::fromUtf8(entry->path ? entry->path : ""),
            QString::fromUtf8(entry->label ? entry->label : ""), QString::fromUtf8(entry->command ? entry->command : ""),
            QString::fromUtf8(entry->uncheck_command ? entry->uncheck_command : ""), QString::fromUtf8(entry->group ? entry->group : ""),
            entry->label == nullptr, entry->label && !entry->command && !entry->uncheck_command && !entry->group,
            entry->uncheck_command || entry->group, entry->group != nullptr, entry->position, entry->checked, entry->enabled, entry->key, entry->modifiers, QString::fromUtf8(entry->icon ? entry->icon : ""), QString::fromUtf8(entry->markup ? entry->markup : "")});
    }, &rows);
    QHash<QString, QActionGroup *> groups;
    for (int pass = 0; !rows.isEmpty() && pass <= 16; ++pass) {
        bool progress = false;
        for (int i = 0; i < rows.size();) {
            auto row = rows[i];
            auto parts = row.path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            if (parts.isEmpty() || parts.takeFirst().compare(root, Qt::CaseInsensitive) != 0) { rows.removeAt(i); continue; }
            QMenu *parent = menu; bool found = true;
            for (const auto &part : parts) {
                QMenu *child = nullptr;
                for (auto *action : parent->actions()) if (action->menu() && normalized(action->text()) == normalized(part)) { child = action->menu(); break; }
                if (!child) { found = false; break; }
                parent = child;
            }
            if (!found) { ++i; continue; }
            auto *action = row.submenu ? (new QMenu(mnemonic(row.label), parent))->menuAction() : new QAction(mnemonic(row.label), parent);
            setMenuIcon(action, row);
            setMenuMarkup(action, row.markup, parent);
            action->setSeparator(row.nullLabel); action->setObjectName(QStringLiteral("dynamicPopupAction"));
            const auto siblings = parent->actions(); int position = row.position;
            if (position < 0) position = qMax(0, int(siblings.size()) + position);
            if (position != 0xffff && position >= 0 && position < siblings.size()) parent->insertAction(siblings[position], action); else parent->addAction(action);
            action->setEnabled(row.enabled && (row.submenu || row.nullLabel || (bool(context.dynamic) && customCommandSupported(row.command) && (row.uncheck.isEmpty() || customCommandSupported(row.uncheck)))));
            action->setCheckable(row.checkable); action->setChecked(row.checked);
            if (row.radio) {
                const QString key = normalized(row.path) + QLatin1Char('/') + normalized(row.group);
                if (!groups.contains(key)) groups[key] = new QActionGroup(menu);
                groups[key]->addAction(action);
            }
            if (!row.submenu && !row.nullLabel) QObject::connect(action, &QAction::triggered, menu, [context, row](bool checked) {
                if (context.dynamic) context.dynamic(row.id, row.command, checked);
            });
            rows.removeAt(i); progress = true;
        }
        if (!progress) break;
    }
}
