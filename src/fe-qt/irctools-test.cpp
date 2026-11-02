/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "irctools.h"
#include "tools-test-engine.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QTimer>
#include <QTemporaryDir>
#include <QWidget>
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QWidget parent;
    initializeIrcTools(&parent);
    bool ok = true;
    auto verify = [&](bool condition, const char *message) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; }
    };
    int keys[2]; void *first = &keys[0], *second = &keys[1];
    ToolTestEngine::servers.insert(first, true); ToolTestEngine::servers.insert(second, true);
    qt_fe_channel_browser(first, "#test*", true);
    auto *browser = parent.findChild<QDialog *>(QStringLiteral("channelBrowser"));
    auto *table = browser->findChild<QTableView *>(QStringLiteral("channelResults"));
    verify(ToolTestEngine::requestedServer == first && ToolTestEngine::requestedFilter == QStringLiteral("#test*"), "LIST request is server-specific");
    qt_fe_channel_row(first, "#small", "2", "Small topic");
    qt_fe_channel_row(first, "#large", "100", "\002Large\002 café topic");
    qt_fe_channel_row(first, "#medium", "10", "Middle");
    qt_fe_channel_row(first, "#small", "3", "Updated");
    qt_fe_channel_end(first);
    verify(table->model()->rowCount() == 3 && table->model()->index(0, 0).data().toString() == QStringLiteral("#large"), "numeric sort and duplicate channel update");
    verify(table->model()->index(0, 2).data().toString() == QStringLiteral("Large café topic"), "topic formatting stripped to plain Unicode");
    auto *search = browser->findChild<QLineEdit *>(QStringLiteral("channelFilter"));
    search->setText(QStringLiteral("CAFÉ"));
    verify(table->model()->rowCount() == 1, "channel topic filter is case insensitive");
    search->clear();
    browser->findChild<QSpinBox *>(QStringLiteral("minimumUsers"))->setValue(11);
    verify(table->model()->rowCount() == 1, "minimum users filter");
    table->selectRow(0);
    browser->findChild<QPushButton *>(QStringLiteral("joinSelectedChannel"))->click();
    verify(ToolTestEngine::joinedServer == first && ToolTestEngine::joinedChannel == QStringLiteral("#large"), "join selected channel uses browser's server");
    qt_fe_channel_browser(second, "", true);
    qt_fe_channel_row(second, "#other", "50", "Other server"); qt_fe_channel_end(second);
    verify(table->model()->rowCount() == 1, "second network does not change first browser");
    browser->findChild<QPushButton *>(QStringLiteral("refreshChannels"))->click();
    verify(ToolTestEngine::requestedFilter == QStringLiteral("#test*") && table->model()->rowCount() == 0, "refresh retains server filter and resets result set");
    const int requests = ToolTestEngine::requests;
    qt_fe_channel_browser(first, "", true);
    verify(ToolTestEngine::requests == requests, "overlapping LIST request suppressed");
    qt_fe_channel_end(first);
    ToolTestEngine::servers[first] = false;
    browser->findChild<QPushButton *>(QStringLiteral("refreshChannels"))->click();
    verify(ToolTestEngine::requests == requests && browser->findChild<QLabel *>(QStringLiteral("channelStatus"))->text() == QStringLiteral("Not connected"), "disconnected refresh does not send a request");
    ToolTestEngine::servers[first] = true;

    showRawLog(first);
    auto *log = parent.findChild<QDialog *>(QStringLiteral("rawLog"));
    auto *text = log->findChild<QPlainTextEdit *>(QStringLiteral("rawLogText"));
    const QByteArray packet("PRIVMSG #test :hello café\r\nPASS supersecret\r\nAUTHENTICATE c2VjcmV0\r\n");
    qt_fe_rawlog(first, packet.constData(), packet.size(), true);
    verify(text->toPlainText().contains(QStringLiteral(">> PRIVMSG #test :hello café")) &&
           !text->toPlainText().contains(QStringLiteral("supersecret")) && !text->toPlainText().contains(QStringLiteral("c2VjcmV0")), "raw log direction, Unicode, multiline data and auth redaction");
    verify(redactedIrcLine(QStringLiteral("@tag=1 :me PRIVMSG NickServ :IDENTIFY nick secret")) ==
           QStringLiteral("@tag=1 :me PRIVMSG NickServ :[redacted]"), "tagged NickServ authentication redacted");
    verify(redactedIrcLine(QStringLiteral("OPER user password")) == QStringLiteral("OPER [redacted]"), "operator authentication redacted");
    verify(redactedIrcLine(QStringLiteral(":nick PRIVMSG #chat :PASS ordinary text")) ==
           QStringLiteral(":nick PRIVMSG #chat :PASS ordinary text"), "ordinary message is preserved");
    auto *pause = log->findChild<QCheckBox *>(QStringLiteral("pauseRawLog")); pause->setChecked(true);
    const QString captured = text->toPlainText();
    qt_fe_rawlog(first, "PING :paused", 12, false);
    verify(text->toPlainText() == captured, "pause drops incoming lines");
    pause->setChecked(false);
    qt_fe_rawlog(first, "PING :active", 12, false);
    verify(text->toPlainText().endsWith(QStringLiteral("<< PING :active")), "inbound capture resumes");
    for (int i = 0; i < 5010; ++i) qt_fe_rawlog(first, "PING :line", 10, false);
    verify(text->document()->blockCount() == 5000, "raw log is bounded");
    log->findChild<QPushButton *>(QStringLiteral("clearRawLog"))->click();
    verify(text->toPlainText().isEmpty(), "raw log clear");
    QPointer<QDialog> oldLog = log, oldBrowser = browser;
    qt_fe_tools_server_closed(first); ToolTestEngine::servers.remove(first);
    verify(!qt_fe_channel_browser_open(first) && !oldLog->isVisible(), "server removal immediately detaches tools");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    verify(oldLog.isNull() && oldBrowser.isNull(), "server removal frees dialogs safely");
    ToolTestEngine::servers.insert(first, true);
    showRawLog(first); qt_fe_rawlog(first, "PING :new", 9, false);
    verify(parent.findChild<QDialog *>(QStringLiteral("rawLog"))->findChild<QPlainTextEdit *>()->toPlainText() == QStringLiteral("<< PING :new"), "reused server pointer has fresh log state");

    showIgnoreManager();
    auto *manager = parent.findChild<QDialog *>(QStringLiteral("ignoreManager"));
    auto *rules = manager->findChild<QTableView *>(QStringLiteral("ignoreRules"));
    QTimer::singleShot(0, manager, [&]() {
        auto *editor = manager->findChild<QDialog *>(QStringLiteral("ignoreEditor"));
        editor->findChild<QLineEdit *>(QStringLiteral("ignoreMask"))->setText(QStringLiteral("nick!*@*"));
        editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    });
    manager->findChild<QPushButton *>(QStringLiteral("addIgnore"))->click();
    verify(ToolTestEngine::ignores.value(QStringLiteral("nick!*@*")) == 159 && rules->model()->rowCount() == 1, "ignore add persists chosen core flags and updates UI");
    rules->selectRow(0);
    QTimer::singleShot(0, manager, [&]() {
        auto *editor = manager->findChild<QDialog *>(QStringLiteral("ignoreEditor"));
        editor->findChild<QLineEdit *>(QStringLiteral("ignoreMask"))->setText(QStringLiteral("changed!*@*"));
        editor->findChild<QCheckBox *>(QStringLiteral("ignoreFlag6"))->setChecked(true);
        editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    });
    manager->findChild<QPushButton *>(QStringLiteral("editIgnore"))->click();
    verify(!ToolTestEngine::ignores.contains(QStringLiteral("nick!*@*")) &&
           (ToolTestEngine::ignores.value(QStringLiteral("changed!*@*")) & 64), "ignore edit renames rule and supports temporary flag");
    QTimer::singleShot(0, manager, [&]() { manager->findChild<QDialog *>(QStringLiteral("ignoreEditor"))->reject(); });
    manager->findChild<QPushButton *>(QStringLiteral("addIgnore"))->click();
    verify(ToolTestEngine::ignores.size() == 1, "ignore cancel leaves rules unchanged");
    ToolTestEngine::ignores.insert(QStringLiteral("external!*@*"), 1); qt_fe_ignore_changed();
    verify(rules->model()->rowCount() == 2, "external ignore changes refresh manager");
    QTimer::singleShot(0, manager, [&]() {
        auto *editor = manager->findChild<QDialog *>(QStringLiteral("ignoreEditor"));
        editor->findChild<QLineEdit *>(QStringLiteral("ignoreMask"))->setText(QStringLiteral("external!*@*"));
        editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        verify(!editor->findChild<QLabel *>(QStringLiteral("ignoreValidation"))->text().isEmpty(), "duplicate ignore is rejected inline");
        editor->reject();
    });
    manager->findChild<QPushButton *>(QStringLiteral("addIgnore"))->click();
    verify(ToolTestEngine::ignores.size() == 2, "duplicate ignore leaves existing rules intact");
    rules->selectRow(0); manager->findChild<QPushButton *>(QStringLiteral("removeIgnore"))->click();
    verify(ToolTestEngine::ignores.size() == 1 && rules->model()->rowCount() == 1, "ignore removal refreshes UI");

    qt_fe_prompt_password("Password", nullptr, nullptr);
    auto *passwordDialog = parent.findChild<QInputDialog *>();
    verify(passwordDialog && passwordDialog->textEchoMode() == QLineEdit::Password,
           "password prompt masks credentials");
    passwordDialog->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    struct Result { int calls = 0; int cancelled = -1; QString text; int number = 0; } result;
    qt_fe_prompt_string("Text", "initial", [](int cancel, char *value, void *data) {
        auto *r = static_cast<Result *>(data); ++r->calls; r->cancelled = cancel; r->text = QString::fromUtf8(value);
    }, &result);
    auto *stringDialog = parent.findChild<QInputDialog *>();
    verify(result.calls == 0, "string prompt is asynchronous");
    stringDialog->setTextValue(QStringLiteral("café")); stringDialog->accept();
    verify(result.calls == 0, "accepted prompt defers core callback until Qt response unwinds");
    QCoreApplication::processEvents();
    verify(result.calls == 1 && !result.cancelled && result.text == QStringLiteral("café"), "string callback accepts Unicode exactly once");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_prompt_integer("Number", 7, [](int cancel, int value, void *data) {
        auto *r = static_cast<Result *>(data); ++r->calls; r->cancelled = cancel; r->number = value;
    }, &result);
    auto *integerDialog = parent.findChild<QInputDialog *>(); integerDialog->setIntValue(-123); integerDialog->reject();
    QCoreApplication::processEvents();
    verify(result.calls == 2 && result.cancelled && result.number == -123, "integer cancel callback preserves GTK cancellation contract");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_prompt_bool("Question", "<b>literal</b>", [](int value, void *data) {
        auto *r = static_cast<Result *>(data); ++r->calls; r->number = value;
    }, &result);
    auto *question = parent.findChild<QMessageBox *>();
    verify(question->textFormat() == Qt::PlainText, "question text is literal");
    question->done(QMessageBox::Yes);
    QCoreApplication::processEvents();
    verify(result.calls == 3 && result.number == 1, "yes callback");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt_fe_prompt_bool("Question", "No?", [](int value, void *data) {
        auto *r = static_cast<Result *>(data); ++r->calls; r->number = value;
    }, &result);
    parent.findChild<QMessageBox *>()->reject();
    QCoreApplication::processEvents();
    verify(result.calls == 4 && result.number == 0, "closing boolean dialog returns false exactly once");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTemporaryDir directory;
    verify(directory.isValid(), "file test directory created");
    const QString firstFile = directory.filePath(QStringLiteral("first.txt"));
    const QString secondFile = directory.filePath(QStringLiteral("second.txt"));
    for (const QString &file : {firstFile, secondFile}) {
        QFile output(file); verify(output.open(QIODevice::WriteOnly), "file test fixture created");
    }
    struct Files { QStringList paths; int endings = 0; } files;
    auto fileCallback = [](void *data, char *file) {
        auto *r = static_cast<Files *>(data);
        if (file) r->paths.append(QString::fromUtf8(file)); else ++r->endings;
    };
    qt_fe_prompt_file("Multiple files", directory.path().toUtf8().constData(), 2, fileCallback, &files);
    auto *fileDialog = parent.findChild<QFileDialog *>();
    verify(files.endings == 0 && fileDialog->fileMode() == QFileDialog::ExistingFiles, "file prompt is asynchronous and supports multiselect");
    fileDialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"))->setText(QStringLiteral("\"first.txt\" \"second.txt\""));
    QMetaObject::invokeMethod(fileDialog, "accept", Qt::DirectConnection);
    QCoreApplication::processEvents();
    verify(files.paths.size() == 2 && files.paths.contains(firstFile) && files.paths.contains(secondFile) && files.endings == 1,
           "multiple files delivered before exactly one completion sentinel");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    files = {};
    qt_fe_prompt_file("Folder", directory.path().toUtf8().constData(), 8, fileCallback, &files);
    fileDialog = parent.findChild<QFileDialog *>();
    verify(fileDialog->fileMode() == QFileDialog::Directory, "folder request uses directory mode");
    fileDialog->reject();
    QCoreApplication::processEvents();
    verify(files.paths.isEmpty() && files.endings == 1, "cancel delivers only completion sentinel");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    files = {};
    qt_fe_prompt_file("Save", directory.filePath(QStringLiteral("new.txt")).toUtf8().constData(), 1, fileCallback, &files);
    fileDialog = parent.findChild<QFileDialog *>();
    verify(fileDialog->acceptMode() == QFileDialog::AcceptSave, "save request uses save mode");
    fileDialog->reject();
    QCoreApplication::processEvents();
    verify(files.endings == 1, "save cancel completion");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    Result destroyedString;
    qt_fe_prompt_string("Destroy string", "initial", [](int cancel, char *value, void *data) {
        auto *r = static_cast<Result *>(data); ++r->calls; r->cancelled = cancel; r->text = QString::fromUtf8(value);
    }, &destroyedString);
    auto *destroyedDialog = parent.findChild<QInputDialog *>();
    destroyedDialog->setTextValue(QString::fromUtf8("edited café"));
    delete destroyedDialog;
    QCoreApplication::processEvents();
    verify(destroyedString.calls == 1 && destroyedString.cancelled && destroyedString.text == QString::fromUtf8("edited café"), "destroyed string prompt reports cancellation with last value exactly once");
    {
        auto *owner = new QWidget;
        initializeIrcTools(owner);
        Result textResult, numberResult, boolResult; Files fileResult;
        qt_fe_prompt_string("Owner text", "draft", [](int cancel, char *value, void *data) {
            auto *r = static_cast<Result *>(data); ++r->calls; r->cancelled = cancel; r->text = QString::fromUtf8(value);
        }, &textResult);
        qt_fe_prompt_integer("Owner integer", 9, [](int cancel, int value, void *data) {
            auto *r = static_cast<Result *>(data); ++r->calls; r->cancelled = cancel; r->number = value;
        }, &numberResult);
        for (auto *dialog : owner->findChildren<QInputDialog *>()) if (dialog->inputMode() == QInputDialog::IntInput) dialog->setIntValue(-77);
        qt_fe_prompt_bool("Owner boolean", "Question", [](int value, void *data) {
            auto *r = static_cast<Result *>(data); ++r->calls; r->number = value;
        }, &boolResult);
        qt_fe_prompt_file("Owner file", directory.path().toUtf8().constData(), 2, fileCallback, &fileResult);
        delete owner;
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        verify(textResult.calls == 1 && textResult.cancelled && textResult.text == QStringLiteral("draft"), "parent destruction cancels string callback");
        verify(numberResult.calls == 1 && numberResult.cancelled && numberResult.number == -77, "parent destruction cancels integer with last value");
        verify(boolResult.calls == 1 && boolResult.number == 0, "parent destruction answers boolean false exactly once");
        verify(fileResult.endings == 1 && fileResult.paths.isEmpty(), "parent destruction completes file request with one sentinel");
    }
    {
        struct ReentrantFiles { QWidget *owner; QStringList paths; int endings = 0; } reentrant{new QWidget, {}, 0};
        initializeIrcTools(reentrant.owner);
        qt_fe_prompt_file("Reentrant multiple files", directory.path().toUtf8().constData(), 2, [](void *data, char *file) {
            auto *r = static_cast<ReentrantFiles *>(data);
            if (file) {
                r->paths.append(QString::fromUtf8(file));
                if (r->owner) { auto *owner = r->owner; r->owner = nullptr; delete owner; }
            } else ++r->endings;
        }, &reentrant);
        auto *dialog = reentrant.owner->findChild<QFileDialog *>();
        dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"))->setText(QStringLiteral("\"first.txt\" \"second.txt\""));
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        verify(reentrant.owner && reentrant.paths.isEmpty(), "file callback waits until QFileDialog accept returns");
        QCoreApplication::processEvents();
        verify(!reentrant.owner && reentrant.paths.size() == 2 && reentrant.endings == 1, "file callback can destroy parent while remaining selections and one completion sentinel are delivered");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        verify(reentrant.endings == 1, "deferred destruction does not duplicate file completion");
    }
    initializeIrcTools(&parent);
    return ok ? 0 : 1;
}
