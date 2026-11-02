/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "addons.h"
#include "addons-test-engine.h"
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QTableView>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); QWidget parent; int identities[3]; bool alive = true, ok = true; int restored = 0;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    AddonsTestEngine::rows = {{&identities[0], QStringLiteral("Native"), QStringLiteral("/tmp/test.so")}, {&identities[1], QStringLiteral("Script"), QStringLiteral("/tmp/test.py"), true}, {&identities[2], QStringLiteral("Builtin"), {}, false, true}};
    showAddons(&parent, [&]() { return AddonContext{&parent, [&]() { return alive; }, [&]() { ++restored; }}; });
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("addonsDialog"));
    auto *table = dialog->findChild<QTableView *>(QStringLiteral("addonRows"));
    auto *unload = dialog->findChild<QPushButton *>(QStringLiteral("unloadAddon"));
    auto select = [&](const QString &name) { for (int i = 0; i < table->model()->rowCount(); ++i) if (table->model()->index(i, 0).data().toString() == name) { table->setCurrentIndex(table->model()->index(i, 0)); table->selectRow(i); } };
    check(table->model()->rowCount() == 3, "loaded addons listed");
    select(QStringLiteral("Builtin")); check(!unload->isEnabled(), "built-in addon cannot unload");
    select(QStringLiteral("Script")); unload->click();
    check(AddonsTestEngine::actions == 1 && AddonsTestEngine::lastFile == QStringLiteral("/tmp/test.py") && restored == 1, "script operation retains filename and restores context");
    alive = false; dialog->findChild<QPushButton *>(QStringLiteral("refreshAddons"))->click();
    check(!unload->isEnabled() && !dialog->findChild<QPushButton *>(QStringLiteral("loadAddon"))->isEnabled(), "closed conversation disables operations");
    alive = true; AddonsTestEngine::enabled = false; dialog->findChild<QPushButton *>(QStringLiteral("refreshAddons"))->click();
    check(!dialog->findChild<QPushButton *>(QStringLiteral("loadAddon"))->isEnabled(), "disabled build does not offer loading");
    return ok ? 0 : 1;
}
