/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "urlhistory.h"
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <cstdio>
int clears = 0;
extern "C" void qt_core_urls_clear(void) { ++clears; }
int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QWidget parent;
    initializeUrlHistory(&parent);
    bool ok = true;
    auto verify = [&](bool condition, const char *message) { if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    qt_fe_url_capture("https://example.com/café"); qt_fe_url_capture("https://example.com/café");
    qt_fe_url_capture("file:///tmp/local"); qt_fe_url_capture("https://example.com/path\nBAD");
    showUrlHistory();
    auto *dialog = parent.findChild<QDialog *>(QStringLiteral("urlHistory"));
    auto *table = dialog->findChild<QTableView *>(QStringLiteral("urlRows"));
    verify(table->model()->rowCount() == 2, "URL capture works before opening; duplicates and multiline inputs rejected");
    auto *filter = dialog->findChild<QLineEdit *>(QStringLiteral("urlFilter")); filter->setText(QStringLiteral("CAFÉ"));
    verify(table->model()->rowCount() == 1, "URL filter supports Unicode case folding");
    table->selectRow(0); dialog->findChild<QPushButton *>(QStringLiteral("copyHistoryUrls"))->click();
    verify(QApplication::clipboard()->text() == QStringLiteral("https://example.com/café"), "copy URL preserves Unicode");
    qt_fe_url_capture("https://other.example/new"); app.processEvents();
    verify(table->selectionModel()->selectedRows(0).size() == 1, "new capture preserves user selection");
    filter->setText(QStringLiteral("file:")); table->selectRow(0);
    dialog->findChild<QPushButton *>(QStringLiteral("openHistoryUrl"))->click();
    verify(dialog->findChild<QLabel *>(QStringLiteral("urlHistoryStatus"))->text().startsWith(QStringLiteral("Unsupported")), "unsupported URL cannot launch an application");
    verify(safeHistoryUrl("javascript:alert(1)").isEmpty() && safeHistoryUrl("data:text/html,hello").isEmpty() &&
           safeHistoryUrl("/tmp/file").isEmpty() && safeHistoryUrl("https://").isEmpty(), "unsafe and invalid schemes rejected");
    verify(safeHistoryUrl("www.example.com").scheme() == QStringLiteral("https") &&
           safeHistoryUrl("ircs://irc.example/#test").scheme() == QStringLiteral("ircs"), "supported URL normalization");
    QTemporaryDir directory; const QString fileName = directory.filePath(QStringLiteral("urls.txt"));
    QTimer::singleShot(0, dialog, [&]() {
        qt_fe_url_capture("https://later.example/");
        auto *file = dialog->findChild<QFileDialog *>(); file->selectFile(fileName);
        QMetaObject::invokeMethod(file, "accept", Qt::DirectConnection);
    });
    dialog->findChild<QPushButton *>(QStringLiteral("saveHistoryUrls"))->click();
    QFile saved(fileName); verify(saved.open(QIODevice::ReadOnly), "URL export file created");
    const QString exported = QString::fromUtf8(saved.readAll());
    verify(exported.contains(QStringLiteral("https://example.com/café")) && exported.contains(QStringLiteral("file:///tmp/local")) &&
           !exported.contains(QStringLiteral("later.example")), "export snapshots all retained URLs including filtered rows");
    dialog->findChild<QPushButton *>(QStringLiteral("clearHistoryUrls"))->click();
    verify(clears == 1 && table->model()->rowCount() == 0, "clear also resets core URL deduplication");
    filter->clear();
    for (int i = 0; i < 1005; ++i) qt_fe_url_capture(QStringLiteral("https://example.com/%1").arg(i).toUtf8().constData());
    app.processEvents(); verify(table->model()->rowCount() == 1000, "URL history bounded at 1000 entries");
    filter->setText(QStringLiteral("https://example.com/0"));
    verify(table->model()->rowCount() == 0, "oldest URLs evicted");
    dialog->close(); showUrlHistory(); filter->clear();
    verify(table->model()->rowCount() == 1000, "reopening retains session history");
    return ok ? 0 : 1;
}
