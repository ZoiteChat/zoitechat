/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "urlhistory.h"
#include "tools-bridge.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace {
struct Entry { QString url; QDateTime time; };
QList<Entry> entries;
QSet<QString> seen;
QPointer<QWidget> historyParent;
std::function<void(QMenu *, const QString &)> popupDecorator;
class HistoryDialog final : public QDialog {
public:
    explicit HistoryDialog(QWidget *parent) : QDialog(parent)
    {
        setObjectName(QStringLiteral("urlHistory")); setWindowTitle(QStringLiteral("URL Grabber")); resize(800, 450);
        auto *layout = new QVBoxLayout(this);
        auto *filter = new QLineEdit(this); filter->setObjectName(QStringLiteral("urlFilter"));
        filter->setPlaceholderText(QStringLiteral("Filter captured URLs")); layout->addWidget(filter);
        model_ = new QStandardItemModel(this); model_->setHorizontalHeaderLabels({QStringLiteral("URL"), QStringLiteral("Captured")});
        proxy_ = new QSortFilterProxyModel(this); proxy_->setSourceModel(model_); proxy_->setFilterKeyColumn(0);
        proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("urlRows")); table_->setModel(proxy_);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSortingEnabled(true);
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch); layout->addWidget(table_, 1);
        table_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(table_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
            const auto index = point.x() < 0 ? table_->currentIndex() : table_->indexAt(point);
            if (!index.isValid()) return;
            const QString text = proxy_->index(index.row(), 0).data().toString();
            const QUrl url = safeHistoryUrl(text); QMenu menu(this);
            auto *open = menu.addAction(QStringLiteral("Open Link")); open->setEnabled(!url.isEmpty());
            connect(open, &QAction::triggered, &menu, [url]() { QDesktopServices::openUrl(url); });
            connect(menu.addAction(QStringLiteral("Copy Link")), &QAction::triggered, &menu, [text]() { QApplication::clipboard()->setText(text); });
            if (popupDecorator) popupDecorator(&menu, text);
            menu.exec(point.x() < 0 ? table_->mapToGlobal(table_->rect().center()) : table_->viewport()->mapToGlobal(point));
        });
        status_ = new QLabel(QStringLiteral("Up to 1,000 URLs retained in memory. Only HTTP(S) and IRC(S) links can be opened here."), this);
        status_->setObjectName(QStringLiteral("urlHistoryStatus")); status_->setWordWrap(true); layout->addWidget(status_);
        auto *buttons = new QHBoxLayout;
        auto *open = new QPushButton(QStringLiteral("Open Selected"), this); open->setObjectName(QStringLiteral("openHistoryUrl"));
        auto *copy = new QPushButton(QStringLiteral("Copy Selected"), this); copy->setObjectName(QStringLiteral("copyHistoryUrls"));
        auto *save = new QPushButton(QStringLiteral("Save All…"), this); save->setObjectName(QStringLiteral("saveHistoryUrls"));
        auto *clear = new QPushButton(QStringLiteral("Clear All"), this); clear->setObjectName(QStringLiteral("clearHistoryUrls"));
        auto *close = new QPushButton(QStringLiteral("Close"), this);
        buttons->addWidget(open); buttons->addWidget(copy); buttons->addWidget(save); buttons->addWidget(clear); buttons->addStretch(); buttons->addWidget(close);
        layout->addLayout(buttons);
        connect(filter, &QLineEdit::textChanged, proxy_, &QSortFilterProxyModel::setFilterFixedString);
        connect(open, &QPushButton::clicked, this, [this]() { openCurrent(); });
        connect(table_, &QTableView::doubleClicked, this, [this]() { openCurrent(); });
        connect(copy, &QPushButton::clicked, this, [this]() {
            QStringList urls; for (const auto &row : table_->selectionModel()->selectedRows(0)) urls.append(row.data().toString());
            QApplication::clipboard()->setText(urls.join(QLatin1Char('\n')));
        });
        connect(clear, &QPushButton::clicked, this, [this]() { entries.clear(); seen.clear(); qt_core_urls_clear(); reload(); });
        connect(save, &QPushButton::clicked, this, [this]() {
            QByteArray data; for (const auto &entry : entries) data += entry.url.toUtf8() + '\n';
            const QPointer<HistoryDialog> guard(this);
            const QString file = QFileDialog::getSaveFileName(this, QStringLiteral("Save URL History"), QStringLiteral("urls.txt"));
            if (!guard || file.isEmpty()) return;
            QSaveFile output(file);
            if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) status_->setText(output.errorString());
            else status_->setText(QStringLiteral("URL history saved."));
        });
        connect(close, &QPushButton::clicked, this, &QDialog::close); reload();
    }
    void reload()
    {
        QSet<QString> selected;
        for (const auto &index : table_->selectionModel()->selectedRows(0)) selected.insert(index.data().toString());
        const auto index = table_->currentIndex();
        const QString current = index.isValid() ? proxy_->index(index.row(), 0).data().toString() : QString();
        model_->removeRows(0, model_->rowCount());
        for (const auto &entry : entries) {
            auto *time = new QStandardItem; time->setData(entry.time, Qt::DisplayRole);
            model_->appendRow({new QStandardItem(entry.url), time});
        }
        for (int row = 0; row < proxy_->rowCount(); ++row) {
            const auto item = proxy_->index(row, 0);
            const QString url = item.data().toString();
            if (url == current) table_->selectionModel()->setCurrentIndex(item, QItemSelectionModel::NoUpdate);
            if (selected.contains(url)) table_->selectionModel()->select(item, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
    void scheduleReload()
    {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(0, this, [this]() { pending_ = false; reload(); });
    }
private:
    void openCurrent()
    {
        const auto current = table_->currentIndex(); if (!current.isValid()) return;
        const QUrl url = safeHistoryUrl(proxy_->index(current.row(), 0).data().toString());
        if (url.isEmpty()) status_->setText(QStringLiteral("Unsupported or invalid URL. Copy it to handle it elsewhere."));
        else if (!QDesktopServices::openUrl(url)) status_->setText(QStringLiteral("Could not open this URL."));
    }
    QStandardItemModel *model_;
    QSortFilterProxyModel *proxy_;
    QTableView *table_;
    QLabel *status_;
    bool pending_ = false;
};
QPointer<HistoryDialog> dialog;
}
QUrl safeHistoryUrl(const QString &text)
{
    QString value = text.trimmed();
    if (value.startsWith(QStringLiteral("www."), Qt::CaseInsensitive)) value.prepend(QStringLiteral("https://"));
    const QUrl url(value, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    return url.isValid() && !url.host().isEmpty() && (scheme == QStringLiteral("http") || scheme == QStringLiteral("https") ||
        scheme == QStringLiteral("irc") || scheme == QStringLiteral("ircs")) ? url : QUrl();
}
void setUrlPopupDecorator(std::function<void(QMenu *, const QString &)> decorator) { popupDecorator = std::move(decorator); }
void initializeUrlHistory(QWidget *parent) { historyParent = parent; }
void showUrlHistory()
{
    if (!dialog) dialog = new HistoryDialog(historyParent);
    dialog->show(); dialog->raise(); dialog->activateWindow();
}
extern "C" void qt_fe_url_capture(const char *url)
{
    if (!url) return;
    const QString value = QString::fromUtf8(url).trimmed();
    if (value.isEmpty() || value.size() > 4096 || value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r')) || seen.contains(value)) return;
    entries.append({value, QDateTime::currentDateTime()}); seen.insert(value);
    if (entries.size() > 1000) seen.remove(entries.takeFirst().url);
    if (dialog) dialog->scheduleReload();
}
