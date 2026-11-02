/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "customization.h"
#include "menumarkup.h"
#include "customization-test-engine.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QImage>
#include <QTemporaryDir>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QTableView>
#include <QWidget>
#include <cstdio>
CustomMenus *activeMenus = nullptr;
QByteArray iconConfigPath;
extern "C" const char *qt_core_config_dir(void) { return iconConfigPath.constData(); }
extern "C" void qt_fe_customization_changed(int kind) { if (activeMenus) activeMenus->schedule(kind == QT_CONFIG_USERMENU, kind == QT_CONFIG_USERMENU); }
extern "C" void qt_fe_menus_changed(void) { if (activeMenus) activeMenus->schedule(false, true); }
int main(int argc, char **argv)
{
    QApplication app(argc, argv); QWidget parent; QMenuBar bar(&parent);
    auto *user = bar.addMenu(QStringLiteral("&Usermenu")); auto *window = bar.addMenu(QStringLiteral("&Window"));
    auto *builtin = window->addAction(QStringLiteral("Built In")); builtin->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    CustomizationTestEngine::lists[QT_CONFIG_USERMENU] = {{QStringLiteral("SUB"), QStringLiteral("Tools")},
        {QStringLiteral("Echo"), QStringLiteral("echo user %c")}, {QStringLiteral("SEP"), QString()},
        {QStringLiteral("TOGGLE Hide version"), QStringLiteral("irc_hide_version")}, {QStringLiteral("ENDSUB"), QString()}};
    int session, identities[10];
    CustomMenus menus(&bar, user, &parent, [&](const QString &command) { qt_core_custom_run(&session, command.toUtf8().constData()); },
        [&](void *id, const QString &expected, bool checked) { qt_core_menu_activate(&session, id, expected.toUtf8().constData(), checked); });
    activeMenus = &menus;
    bool ok = true;
    auto verify = [&](bool condition, const char *message) { if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    auto redPixels = [](QWidget *widget) {
        widget->ensurePolished(); widget->adjustSize();
        QImage rendered(widget->size(), QImage::Format_ARGB32); rendered.fill(Qt::transparent); widget->render(&rendered);
        int count = 0;
        for (int y = 0; y < rendered.height(); ++y) for (int x = 0; x < rendered.width(); ++x) {
            const auto color = rendered.pixelColor(x, y);
            if (color.red() > 180 && color.green() < 80 && color.blue() < 80) ++count;
        }
        return count;
    };
    auto dynamicAction = [&](void *id) -> QAction * {
        for (auto *action : parent.findChildren<QAction *>())
            if (action->objectName() == QStringLiteral("dynamicMenuAction") && action->property("coreMenuId").value<quintptr>() == reinterpret_cast<quintptr>(id) && action->isEnabled()) return action;
        return nullptr;
    };
    auto *tools = user->actions().first()->menu();
    verify(tools && tools->title() == QStringLiteral("Tools") && tools->actions().size() == 3, "Usermenu submenu and separator built from core list");
    tools->actions().first()->trigger();
    verify(CustomizationTestEngine::executed.last() == QStringLiteral("echo user %c"), "Usermenu commands use core callback");
    tools->actions().last()->trigger(); verify(CustomizationTestEngine::preference == 1, "Usermenu preference toggle executes set command");
    QPointer<QMenu> oldTools = tools; user->aboutToShow(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    verify(oldTools.isNull() && user->actions().first()->menu()->actions().last()->isChecked(), "Usermenu rebuild refreshes checked state and frees old submenus");

    QTemporaryDir icons;
    iconConfigPath = icons.path().toUtf8();
    const QString iconPath = icons.filePath(QStringLiteral("menu icon.png"));
    QImage iconImage(16, 16, QImage::Format_ARGB32); iconImage.fill(QColor(12, 190, 50));
    verify(icons.isValid() && iconImage.save(iconPath), "menu icon fixture created");
    using Entry = CustomizationTestEngine::Menu;
    Entry leaf{&identities[0], QStringLiteral("Window/Custom"), QStringLiteral("Echo"), QStringLiteral("echo dynamic")};
    Entry root{&identities[1], QStringLiteral("Window"), QStringLiteral("Custom"), QString()}; root.hasCommand = false;
    Entry toggle{&identities[2], QStringLiteral("Window/Custom"), QStringLiteral("Toggle"), QStringLiteral("echo on"), QStringLiteral("echo off")}; toggle.toggle = true;
    Entry conflicting{&identities[3], QStringLiteral("Window/Custom"), QStringLiteral("Conflict"), QStringLiteral("echo conflict")}; conflicting.key = 's'; conflicting.modifiers = 4;
    Entry fkey{&identities[4], QStringLiteral("Window/Custom"), QStringLiteral("Function"), QStringLiteral("echo fkey")}; fkey.key = 0xffc2;
    Entry popup{&identities[5], QStringLiteral("$NICK"), QStringLiteral("Hidden popup"), QStringLiteral("echo popup")}; popup.main = false;
    leaf.icon = iconPath; root.icon = QStringLiteral("gtk-open");
    CustomizationTestEngine::menus = {leaf, root, toggle, conflicting, fkey, popup}; qt_fe_menus_changed(); app.processEvents();
    auto *item = dynamicAction(&identities[0]); verify(item != nullptr && item->parent() != &bar, "child-before-parent dynamic menu paths resolved");
    if (item) item->trigger();
    verify(item && item->isIconVisibleInMenu() && item->icon().pixmap(16, 16).toImage().pixelColor(8, 8) == QColor(12, 190, 50), "runtime main action renders supplied icon pixels");
    verify(!dynamicAction(&identities[1])->icon().isNull(), "legacy GTK stock icon has a Qt theme/style fallback");
    verify(CustomizationTestEngine::executed.last() == QStringLiteral("echo dynamic"), "dynamic leaf executes only when selected");
    auto *toggleAction = dynamicAction(&identities[2]); if (toggleAction) toggleAction->trigger(); app.processEvents();
    verify(CustomizationTestEngine::executed.last() == QStringLiteral("echo on") && dynamicAction(&identities[2])->isChecked(), "dynamic toggle executes on command and refreshes checked state");
    dynamicAction(&identities[2])->trigger(); app.processEvents();
    verify(CustomizationTestEngine::executed.last() == QStringLiteral("echo off") && !dynamicAction(&identities[2])->isChecked(), "dynamic toggle executes uncheck command");
    verify(dynamicAction(&identities[3])->shortcuts().isEmpty() && builtin->shortcut() == QKeySequence(QStringLiteral("Ctrl+S")), "dynamic shortcut does not steal built-in shortcut");
    verify(dynamicAction(&identities[4])->shortcut() == QKeySequence(Qt::Key_F5), "GDK function-key shortcut maps to Qt");
    verify(dynamicAction(&identities[5]) == nullptr, "popup-root entries are not inserted into main menu");
    QPointer<QAction> stale = dynamicAction(&identities[0]); const int before = CustomizationTestEngine::executed.size();
    CustomizationTestEngine::menus.removeFirst(); stale->trigger();
    verify(CustomizationTestEngine::executed.size() == before, "removed core identity cannot execute stale Qt action");
    qt_fe_menus_changed(); app.processEvents(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    verify(stale.isNull(), "deleted menu entry releases old action");
    CustomizationTestEngine::menus.clear(); qt_fe_menus_changed(); app.processEvents(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    verify(window->actions().size() == 1 && window->actions().first() == builtin, "deleting dynamic hierarchy preserves static menus");

    Entry one{&identities[6], QStringLiteral("Window"), QStringLiteral("One"), QStringLiteral("echo one")}; one.radio = true; one.group = QStringLiteral("One"); one.checked = true;
    Entry two{&identities[7], QStringLiteral("Window"), QStringLiteral("Two"), QStringLiteral("echo two")}; two.radio = true; two.group = QStringLiteral("One");
    CustomizationTestEngine::menus = {one, two}; qt_fe_menus_changed(); app.processEvents();
    dynamicAction(&identities[7])->trigger(); app.processEvents();
    verify(!dynamicAction(&identities[6])->isChecked() && dynamicAction(&identities[7])->isChecked(), "radio state remains exclusive after refresh");
    CustomizationTestEngine::menus[1].enabled = false; qt_fe_menus_changed(); app.processEvents();
    verify(dynamicAction(&identities[7]) == nullptr, "core sensitivity update disables action");

    CustomizationTestEngine::lists[QT_CONFIG_COMMANDS] = {{QStringLiteral("A"), QStringLiteral("echo one")}, {QStringLiteral("B"), QStringLiteral("echo two")}};
    showConfigEditor(QT_CONFIG_COMMANDS, &parent);
    auto *editor = parent.findChild<QDialog *>(QStringLiteral("configEditor2")); auto *table = editor->findChild<QTableView *>(QStringLiteral("configRows"));
    table->selectRow(1); editor->findChild<QPushButton *>(QStringLiteral("configUp"))->click();
    verify(table->model()->index(0, 0).data().toString() == QStringLiteral("B"), "editor reorders entries without changing command data");
    table->model()->setData(table->model()->index(0, 0), QString());
    editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    verify(!editor->findChild<QLabel *>(QStringLiteral("configError"))->text().isEmpty() && CustomizationTestEngine::lists[QT_CONFIG_COMMANDS].first().name == QStringLiteral("A"), "invalid save keeps live core unchanged");
    table->model()->setData(table->model()->index(0, 0), QStringLiteral("B"));
    ++CustomizationTestEngine::revisions[QT_CONFIG_COMMANDS]; editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    verify(editor->findChild<QLabel *>(QStringLiteral("configError"))->text().contains(QStringLiteral("changed")), "stale editor conflict displayed inline");
    editor->findChild<QPushButton *>(QStringLiteral("reloadConfig"))->click();
    table->model()->setData(table->model()->index(0, 1), QStringLiteral("echo edited"));
    editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    verify(CustomizationTestEngine::lists[QT_CONFIG_COMMANDS].first().command == QStringLiteral("echo edited"), "valid editor save updates core");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    showConfigEditor(QT_CONFIG_COMMANDS, &parent); editor = parent.findChild<QDialog *>(QStringLiteral("configEditor2"));
    table = editor->findChild<QTableView *>(QStringLiteral("configRows")); table->model()->setData(table->model()->index(0, 1), QStringLiteral("discarded")); editor->reject();
    verify(CustomizationTestEngine::lists[QT_CONFIG_COMMANDS].first().command == QStringLiteral("echo edited"), "Cancel discards unsaved edits");
    CustomizationTestEngine::lists[QT_CONFIG_POPUP] = {{QStringLiteral("SUB"), QStringLiteral("Tools")},
        {QStringLiteral("Who~menu icon.png~"), QStringLiteral("whois %s")}, {QStringLiteral("Empty"), QString()},
        {QStringLiteral("ENDSUB"), QString()}};
    QString popupCommand;
    CustomPopupContext context{[&](const QString &command) { popupCommand = command; },
        [&](void *id, const QString &command, bool checked) { qt_core_menu_activate(&session, id, command.toUtf8().constData(), checked); }};
    for (const QString &root : {QStringLiteral("$NICK"), QStringLiteral("$CHAN"), QStringLiteral("$TAB"), QStringLiteral("$URL"), QStringLiteral("$TRAY")}) {
        Entry child{&identities[8], root + QStringLiteral("/Extra"), QStringLiteral("Inspect"), QStringLiteral("echo %s")}; child.main = false; child.icon = iconPath; child.markup = QStringLiteral("<span foreground='#ff0000'>Inspect</span>");
        Entry submenu{&identities[9], root, QStringLiteral("Extra"), QString()}; submenu.main = false; submenu.hasCommand = false;
        CustomizationTestEngine::menus = {child, submenu};
        QMenu popup;
        populateCustomPopup(&popup, root, QT_CONFIG_POPUP, context);
        const auto saved = popup.actions().first()->menu()->actions().first()->menu()->actions();
        verify(saved.first()->text() == QStringLiteral("Who") && saved.first()->icon().pixmap(16, 16).toImage().pixelColor(8, 8) == QColor(12, 190, 50), "saved popup icon resolves relative to active configuration directory without changing label");
        saved.first()->trigger();
        verify(popupCommand == QStringLiteral("whois %s") && !saved.last()->isEnabled(), "saved nested popup dispatches and disables empty commands");
        auto *dynamic = popup.actions().last()->menu()->actions().first();
        verify(dynamic->isIconVisibleInMenu() && dynamic->icon().pixmap(16, 16).toImage().pixelColor(8, 8) == QColor(12, 190, 50), "runtime popup roots render supplied icon pixels");
        verify(redPixels(popup.actions().last()->menu()) > 10, "styled labels render through all popup roots");
        const int count = CustomizationTestEngine::executed.size(); dynamic->trigger();
        verify(CustomizationTestEngine::executed.size() == count + 1, "dynamic popup roots resolve child before parent");
        CustomizationTestEngine::menus.clear(); dynamic->trigger();
        verify(CustomizationTestEngine::executed.size() == count + 1, "deleted dynamic popup identity cannot execute");
    }
    const int beforeIconRefresh = CustomizationTestEngine::executed.size();
    leaf.path = QStringLiteral("Window"); leaf.icon = icons.filePath(QStringLiteral("missing.png"));
    CustomizationTestEngine::menus = {leaf}; qt_fe_menus_changed(); app.processEvents();
    verify(dynamicAction(leaf.id) && dynamicAction(leaf.id)->icon().isNull(), "missing icon leaves menu command usable without icon");
    CustomizationTestEngine::menus[0].icon = QStringLiteral("gtk-open"); qt_fe_menus_changed(); app.processEvents();
    verify(!dynamicAction(leaf.id)->icon().isNull(), "runtime icon updates refresh existing menu entries");
    CustomizationTestEngine::menus[0].icon.clear(); qt_fe_menus_changed(); app.processEvents();
    verify(dynamicAction(leaf.id)->icon().isNull() && CustomizationTestEngine::executed.size() == beforeIconRefresh, "clearing icon refreshes without executing menu commands");
    CustomizationTestEngine::lists[QT_CONFIG_USERMENU] = {{QStringLiteral("Saved Icon~menu icon.png~"), QStringLiteral("echo saved icon")},
        {QStringLiteral("Literal\\~label"), QStringLiteral("echo literal")}};
    activeMenus->refreshUsermenu();
    verify(user->actions().first()->text() == QStringLiteral("Saved Icon") && user->actions().first()->icon().pixmap(16, 16).toImage().pixelColor(8, 8) == QColor(12, 190, 50), "saved Usermenu renders configuration-relative icons");
    verify(user->actions()[1]->text() == QStringLiteral("Literal\\~label") && user->actions()[1]->icon().isNull(), "escaped tilde remains literal and does not become an icon marker");
    user->actions().first()->trigger();
    verify(CustomizationTestEngine::executed.last() == QStringLiteral("echo saved icon"), "saved icon decoration preserves command execution");
    Entry markedRoot{&identities[1], QStringLiteral("Window"), QStringLiteral("_Rich"), QString()};
    markedRoot.hasCommand = false; markedRoot.markup = QStringLiteral("<b>_Rich</b>");
    Entry markedChild{&identities[0], QStringLiteral("Window/Rich"), QStringLiteral("_Styled"), QStringLiteral("echo marked")};
    markedChild.markup = QStringLiteral("<span foreground='#00ff00' size='x-large' weight='bold'>_Styled</span>");
    CustomizationTestEngine::menus = {markedChild, markedRoot}; qt_fe_menus_changed(); app.processEvents();
    auto *marked = dynamicAction(markedChild.id);
    verify(marked && marked->text() == QStringLiteral("&Styled"), "styled child-before-parent paths use plain accessible labels and mnemonics");
    auto *richMenu = dynamicAction(markedRoot.id)->menu(); richMenu->ensurePolished(); richMenu->adjustSize();
    QImage menuImage(richMenu->size(), QImage::Format_ARGB32); menuImage.fill(Qt::transparent); richMenu->render(&menuImage);
    int green = 0;
    for (int y = 0; y < menuImage.height(); ++y) for (int x = 0; x < menuImage.width(); ++x) {
        const auto color = menuImage.pixelColor(x, y);
        if (color.green() > 150 && color.red() < 80 && color.blue() < 80) ++green;
    }
    verify(green > 10, "actual menu style renders Pango foreground pixels");
    marked->trigger(); verify(CustomizationTestEngine::executed.last() == QStringLiteral("echo marked"), "styled actions preserve native command routing");
    markedRoot.path.clear(); markedRoot.label = QStringLiteral("_Top__Level");
    markedRoot.markup = QStringLiteral("<span foreground='#ff0000' weight='bold'>_Top__Level</span>");
    markedChild.path = QStringLiteral("Top__Level");
    CustomizationTestEngine::menus = {markedChild, markedRoot}; qt_fe_menus_changed(); app.processEvents();
    verify(dynamicAction(markedRoot.id)->text() == QStringLiteral("&Top_Level") && dynamicAction(markedChild.id), "escaped mnemonic underscores preserve visible labels and submenu paths");
    verify(redPixels(&bar) > 10, "top-level menu bar renders styled label pixels");
    activeMenus = nullptr;
    return ok ? 0 : 1;
}
