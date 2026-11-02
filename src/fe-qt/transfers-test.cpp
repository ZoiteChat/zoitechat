/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "transfers.h"
#include "transfers-test-engine.h"
#include <QApplication>
#include <QDialog>
#include <QFileDialog>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); QWidget parent; bool ok = true;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    int started = 0; initializeTransfers(&parent, []() {}, [&]() { ++started; });
    TransfersTestEngine::rows = {{1, 1, 0, QStringLiteral("Alice"), QStringLiteral("incoming"), QStringLiteral("/tmp/incoming"), 100, 0, 20},
        {2, 0, 0, QStringLiteral("Bob"), QStringLiteral("outgoing"), {}}, {3, 1, 3, QStringLiteral("Carol"), QStringLiteral("done"), {}}};
    showTransfers(); auto *dialog = parent.findChild<QDialog *>(QStringLiteral("transfersDialog"));
    auto *table = dialog->findChild<QTableView *>(QStringLiteral("transferRows"));
    auto button = [&](const char *name) { return dialog->findChild<QPushButton *>(QString::fromLatin1(name)); };
    table->setCurrentIndex(table->model()->index(0, 0)); table->selectRow(0);
    check(button("acceptTransfer")->isEnabled() && button("resumeTransfer")->isEnabled() && !button("removeTransfer")->isEnabled(), "queued incoming offer enables appropriate controls");
    button("resumeTransfer")->click(); check(TransfersTestEngine::lastId == 1 && TransfersTestEngine::lastAction == 1, "resume uses selected transfer identity");
    table->setCurrentIndex(table->model()->index(1, 0)); table->selectRow(1);
    check(!button("acceptTransfer")->isEnabled() && button("abortTransfer")->isEnabled(), "queued outgoing send cannot be accepted");
    table->setCurrentIndex(table->model()->index(2, 0)); table->selectRow(2);
    check(!button("abortTransfer")->isEnabled() && button("removeTransfer")->isEnabled(), "completed transfer offers removal instead of abort");
    table->setCurrentIndex(table->model()->index(0, 0)); table->selectRow(0); QTemporaryDir directory;
    QTimer::singleShot(0, dialog, [&]() {
        auto *picker = dialog->findChild<QFileDialog *>(); check(picker != nullptr, "save chooser opened");
        if (!picker) return;
        table->setCurrentIndex(table->model()->index(1, 0));
        picker->selectFile(directory.filePath(QStringLiteral("saved-file"))); QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection);
    });
    button("saveTransfer")->click();
    check(TransfersTestEngine::lastId == 1 && TransfersTestEngine::lastAction == 0, "save chooser retains original transfer despite selection changes");
    TransfersTestEngine::lastAction = -1;
    qt_fe_transfer_offer(1, "Old offer"); TransfersTestEngine::rows.clear(); app.processEvents();
    check(TransfersTestEngine::lastAction == -1 && !QApplication::activeModalWidget(), "expired deferred offer opens no prompt or action");
    button("refreshTransfers")->click(); check(table->model()->rowCount() == 0 && !button("acceptTransfer")->isEnabled(), "removed transfers leave safe empty UI");
    TransfersTestEngine::rows = {{10, 2, 0, QStringLiteral("Incoming"), {}, {}},
        {11, 3, 1, QStringLiteral("Active"), {}, {}, 123, 456}, {12, 3, 0, QStringLiteral("Outgoing"), {}, {}}};
    showDirectChats();
    auto *chats = parent.findChild<QDialog *>(QStringLiteral("directChatsDialog"));
    auto *chatRows = chats->findChild<QTableView *>(QStringLiteral("directChatRows"));
    check(chatRows->model()->rowCount() == 3 && chatRows->model()->columnCount() == 5, "direct-chat manager shows chat-specific columns and entries");
    auto chatButton = [&](const char *name) { return chats->findChild<QPushButton *>(QString::fromLatin1(name)); };
    chatRows->setCurrentIndex(chatRows->model()->index(0, 0)); chatRows->selectRow(0);
    check(chatButton("acceptTransfer")->isEnabled() && !chatButton("openDirectChat")->isEnabled() && chatButton("saveTransfer")->isHidden(), "incoming queued chat offers acceptance without file controls");
    chatButton("acceptTransfer")->click(); check(TransfersTestEngine::lastId == 10 && TransfersTestEngine::lastAction == 0, "chat accept uses stable identity");
    chatRows->setCurrentIndex(chatRows->model()->index(1, 0)); chatRows->selectRow(1);
    check(chatButton("openDirectChat")->isEnabled() && !chatButton("acceptTransfer")->isEnabled() && chatRows->model()->index(1, 3).data().toString() == QStringLiteral("456"), "active chat shows byte counters and can open conversation");
    chatButton("openDirectChat")->click(); check(TransfersTestEngine::lastId == 11 && TransfersTestEngine::lastAction == 5, "conversation action retains selected chat identity");
    chatRows->setCurrentIndex(chatRows->model()->index(2, 0)); chatRows->selectRow(2);
    check(!chatButton("acceptTransfer")->isEnabled(), "outgoing chat offer cannot be accepted locally");
    chatButton("newDirectChat")->click(); check(started == 1, "new chat routes to original window's recipient prompt");
    return ok ? 0 : 1;
}
