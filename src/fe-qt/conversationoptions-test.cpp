/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "conversationoptions.h"
#include "conversationoptions-test-engine.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); QWidget parent; int session; bool alive = true, ok = true; int applied = 0;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    ConversationOptionsTestEngine::states[&session] = {"#test", "Network", 1, {2,2,2,2,2,2,2,2}};
    showConversationOptions(&session, &parent, [&]() { return alive; }, [&]() { ++applied; });
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("conversationOptions"));
    auto *option = dialog->findChild<QComboBox *>(QStringLiteral("conversationOption5"));
    auto *apply = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    check(option->currentData().toInt() == 2, "native default shown"); option->setCurrentIndex(option->findData(1));
    check(ConversationOptionsTestEngine::states[&session].values[5] == 2, "edits remain draft until Apply");
    ConversationOptionsTestEngine::states[&session].values[5] = 0; apply->click();
    check(!dialog->findChild<QLabel *>(QStringLiteral("conversationOptionsError"))->text().isEmpty() && applied == 0, "concurrent core change blocks stale draft");
    dialog->findChild<QPushButton *>(QStringLiteral("reloadConversationOptions"))->click();
    check(option->currentData().toInt() == 0, "Reload follows actual core state"); option->setCurrentIndex(option->findData(1)); apply->click();
    check(applied == 1 && ConversationOptionsTestEngine::states[&session].values[5] == 1, "Apply dispatches draft and refresh callback");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    showConversationOptions(&session, &parent, [&]() { return alive; }, [&]() { ++applied; });
    dialog = parent.findChild<QDialog *>(QStringLiteral("conversationOptions")); alive = false;
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    check(applied == 1 && dialog->isVisible(), "closed/reused conversation cannot apply draft"); dialog->reject();
    return ok ? 0 : 1;
}
