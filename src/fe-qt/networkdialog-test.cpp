/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "networkdialog.h"
#include "qt-network.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QWidget>
#include <QTimer>
#include <cstdlib>
#include <cstring>
#include <cstdio>
namespace { int identity, connections, count = 1; }
extern "C" {
int qt_net_count(void) { return count; }
int qt_net_read(int index, QtNetwork *out) {
    if (index != 0 || !count) return 0;
    *out = {}; out->identity = &identity; out->name = strdup("Test network"); return 1;
}
void qt_net_release(QtNetwork *out) { free(out->name); *out = {}; }
int qt_net_store(const QtNetwork *, void **, char *, size_t) { return 0; }
int qt_net_delete(void *) { return 0; }
int qt_net_connect(void *network) { if (network != &identity) return 0; ++connections; return 1; }
}
int main(int argc, char **argv) {
    QApplication app(argc, argv); QWidget parent; parent.show(); bool ok = true;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    showNetworkManager(&parent);
    QPointer<QDialog> dialog = parent.findChild<QDialog *>(QStringLiteral("networkManager"));
    check(dialog && dialog->isVisible() && !dialog->isModal(), "network list opens without blocking core startup");
    showNetworkManager(&parent);
    check(parent.findChildren<QDialog *>(QStringLiteral("networkManager")).size() == 1, "reopening network list reuses existing window");
    if (dialog) {
        check(dialog->findChild<QListWidget *>(QStringLiteral("networkList"))->count() == 1, "saved network list loaded");
        dialog->findChild<QPushButton *>(QStringLiteral("connectNetwork"))->click();
        check(connections == 1 && !dialog->isVisible(), "network connect runs bridge and closes manager");
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    check(!dialog, "closed manager is deleted");
    count = 0; showNetworkManager(&parent);
    dialog = parent.findChild<QDialog *>(QStringLiteral("networkManager"));
    check(dialog && !dialog->findChild<QPushButton *>(QStringLiteral("connectNetwork"))->isEnabled(), "empty network list disables connect");
    if (dialog) dialog->close();
    // Reopening before deferred deletion must create a usable replacement.
    showNetworkManager(&parent);
    QDialog *visible = nullptr;
    for (auto *candidate : parent.findChildren<QDialog *>(QStringLiteral("networkManager"))) if (candidate->isVisible()) visible = candidate;
    check(visible != nullptr, "immediate reopen survives previous manager's deferred deletion");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (visible) visible->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    count = 1; auto *otherParent = new QWidget; showNetworkManager(otherParent);
    auto *editing = otherParent->findChild<QDialog *>(QStringLiteral("networkManager"));
    QTimer::singleShot(0, [&]() { delete otherParent; });
    auto *edit = editing->findChild<QPushButton *>(QStringLiteral("editNetwork"));
    check(edit != nullptr, "network edit action exists");
    if (edit) edit->click();
    else { QCoreApplication::processEvents(); }
    showNetworkManager(&parent);
    dialog = parent.findChild<QDialog *>(QStringLiteral("networkManager"));
    check(dialog && dialog->isVisible(), "destroying owner clears singleton before next open");
    return ok ? 0 : 1;
}
