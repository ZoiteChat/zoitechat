/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "moderation.h"
#include "moderation-test-engine.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QTimer>
#include <QWidget>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget parent;
    bool ok = true;
    auto verify = [&](bool condition, const char *message) { if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    int keys[2]; void *first = &keys[0], *second = &keys[1];
    ModerationTestEngine::channels[first] = {"#first", "+ntk secret", "be,k,l,imnpst", "imnpst", 3, 25, 1};
    ModerationTestEngine::channels[second] = {"#second", "+n", "b,k,l,nt", "nt", 1, 0, 0};
    showChannelSettings(first, &parent);
    auto *settings = parent.findChild<QDialog *>(QStringLiteral("channelSettings"));
    auto *current = settings->findChild<QLabel *>(QStringLiteral("currentChannelModes"));
    verify(!current->text().contains(QStringLiteral("secret")), "mode summary does not expose channel key");
    auto *moderated = settings->findChild<QCheckBox *>(QStringLiteral("channelMode_m"));
    verify(!moderated->isChecked() && settings->findChild<QCheckBox *>(QStringLiteral("channelMode_n"))->isChecked(), "server-confirmed checkboxes");
    moderated->click();
    verify(ModerationTestEngine::changes.last().session == first && ModerationTestEngine::changes.last().mode == 'm' &&
           ModerationTestEngine::changes.last().enable && !moderated->isChecked(), "mode request waits for server confirmation");
    ModerationTestEngine::channels[first].modes = "+mntk secret";
    settings->findChild<QPushButton *>(QStringLiteral("refreshChannelModes"))->click();
    verify(moderated->isChecked(), "server mode confirmation updates checkbox");
    auto *key = settings->findChild<QLineEdit *>(QStringLiteral("channelKey"));
    verify(key->echoMode() == QLineEdit::Password && key->text().isEmpty(), "new key starts empty and is masked");
    key->setText(QStringLiteral("newkey")); settings->findChild<QPushButton *>(QStringLiteral("setChannelKey"))->click();
    verify(ModerationTestEngine::changes.last().mode == 'k' && ModerationTestEngine::changes.last().value == QStringLiteral("newkey") && key->text().isEmpty(), "set key sends captured value then clears field");
    settings->findChild<QPushButton *>(QStringLiteral("removeChannelKey"))->click();
    verify(!ModerationTestEngine::changes.last().enable, "remove key request");
    settings->findChild<QSpinBox *>(QStringLiteral("channelLimit"))->setValue(42);
    settings->findChild<QPushButton *>(QStringLiteral("setChannelLimit"))->click();
    verify(ModerationTestEngine::changes.last().mode == 'l' && ModerationTestEngine::changes.last().value == QStringLiteral("42"), "set limit request");
    settings->findChild<QPushButton *>(QStringLiteral("removeChannelLimit"))->click();
    verify(ModerationTestEngine::changes.last().mode == 'l' && !ModerationTestEngine::changes.last().enable, "remove limit request");

    showBanManager(first, &parent);
    auto *bans = parent.findChild<QDialog *>(QStringLiteral("banManager"));
    auto *rows = bans->findChild<QTableView *>(QStringLiteral("banRows"));
    auto *kind = bans->findChild<QComboBox *>(QStringLiteral("banListKind"));
    verify(ModerationTestEngine::lists.last().session == first && ModerationTestEngine::lists.last().mode == 'b' && !kind->isEnabled(), "ban request is scoped and list switching disabled while pending");
    verify(!qt_fe_ban_row(first, "other", "setter", "date", 348), "unrequested list rows are not consumed");
    verify(qt_fe_ban_row(first, "one!*@*", "operator", "today", 367), "ban row consumed");
    qt_fe_ban_row(first, "two!*@*", "other", "yesterday", 367);
    qt_fe_ban_row(first, "one!*@*", "duplicate", "today", 367);
    verify(!qt_fe_ban_end(first, 349) && qt_fe_ban_end(first, 368), "only matching completion ends request");
    verify(rows->model()->rowCount() == 2 && kind->isEnabled(), "deduplicated rows and completed request");
    auto *filter = bans->findChild<QLineEdit *>(QStringLiteral("banFilter")); filter->setText(QStringLiteral("OPERATOR"));
    verify(rows->model()->rowCount() == 1, "ban filtering by setter is case insensitive");
    rows->selectRow(0); bans->findChild<QPushButton *>(QStringLiteral("copyBanMasks"))->click();
    verify(QApplication::clipboard()->text() == QStringLiteral("one!*@*"), "copy mask");
    bans->findChild<QPushButton *>(QStringLiteral("removeBanMasks"))->click();
    verify(ModerationTestEngine::changes.last().mode == 'b' && !ModerationTestEngine::changes.last().enable &&
           ModerationTestEngine::changes.last().value == QStringLiteral("one!*@*"), "remove selected mask request");
    QTimer::singleShot(0, bans, [&]() {
        auto *dialog = bans->findChild<QInputDialog *>(); dialog->setTextValue(QStringLiteral("new!*@*")); dialog->accept();
    });
    bans->findChild<QPushButton *>(QStringLiteral("addBanMask"))->click();
    verify(ModerationTestEngine::changes.last().enable && ModerationTestEngine::changes.last().value == QStringLiteral("new!*@*"), "add literal mask");
    kind->setCurrentIndex(1);
    verify(ModerationTestEngine::lists.last().mode == 'e' && rows->model()->rowCount() == 0, "exception list uses advertised capability and fresh results");
    qt_fe_ban_row(first, "except!*@*", "operator", "today", 348); qt_fe_ban_end(first, 349);
    filter->clear(); verify(rows->model()->rowCount() == 1, "exception results displayed");
    showBanManager(second, &parent);
    verify(ModerationTestEngine::lists.last().session == second && rows->model()->rowCount() == 1, "second channel has independent state");
    qt_fe_ban_end(second, 368);
    const int before = ModerationTestEngine::changes.size();
    QPointer<QDialog> oldBans = bans, oldSettings = settings;
    QTimer::singleShot(0, bans, [&]() {
        auto *dialog = bans->findChild<QInputDialog *>();
        qt_fe_moderation_closed(first);
        // Reuse the same core address while the old prompt is still open.
        ModerationTestEngine::channels[first].channel = "#replacement";
        dialog->setTextValue(QStringLiteral("stale!*@*")); dialog->accept();
    });
    bans->findChild<QPushButton *>(QStringLiteral("addBanMask"))->click();
    verify(ModerationTestEngine::changes.size() == before && !oldBans->isVisible(), "closed/reused channel cannot receive stale mask action");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    verify(oldBans.isNull() && oldSettings.isNull(), "channel reset releases moderation windows");
    showChannelSettings(first, &parent);
    auto *replacement = parent.findChild<QDialog *>(QStringLiteral("channelSettings"));
    verify(replacement && replacement->windowTitle().contains(QStringLiteral("#replacement")), "reused session creates fresh channel settings");
    qt_fe_moderation_closed(first); qt_fe_moderation_closed(second);
    return ok ? 0 : 1;
}
