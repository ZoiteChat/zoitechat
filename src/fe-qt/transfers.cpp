/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "transfers.h"
#include "transfers-bridge.h"
#include <QDialog>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
namespace {
struct Transfer { quint64 id; int type, state; QString nick, file, destination; quint64 size, position, resumable; qint64 speed; bool pending; };
QPointer<QWidget> parentWindow;
std::function<void()> sendFiles, startChat;
QList<Transfer> snapshot()
{
    QList<Transfer> rows;
    qt_core_transfers_read([](const QtTransfer *row, void *data) {
        static_cast<QList<Transfer> *>(data)->append({row->id, row->type, row->state, QString::fromUtf8(row->nick), QString::fromUtf8(row->file), QString::fromUtf8(row->destination), row->size, row->position, row->resumable, row->speed, bool(row->resume_pending)});
    }, &rows); return rows;
}
bool terminal(int state) { return state == 2 || state == 3 || state == 5; }
class TransfersDialog final : public QDialog {
public:
    explicit TransfersDialog(QWidget *parent, bool chat = false) : QDialog(parent), chat_(chat)
    {
        setObjectName(chat_ ? QStringLiteral("directChatsDialog") : QStringLiteral("transfersDialog")); setWindowTitle(chat_ ? QStringLiteral("Direct Chat") : QStringLiteral("File Transfers")); resize(1000, 450); setAttribute(Qt::WA_DeleteOnClose);
        auto *layout = new QVBoxLayout(this);
        model_ = new QStandardItemModel(this); model_->setHorizontalHeaderLabels({QStringLiteral("Direction"), QStringLiteral("Nick"), QStringLiteral("File"), QStringLiteral("Status"), QStringLiteral("Progress"), QStringLiteral("Bytes"), QStringLiteral("Bytes/sec"), QStringLiteral("Destination")});
        table_ = new QTableView(this); table_->setObjectName(chat_ ? QStringLiteral("directChatRows") : QStringLiteral("transferRows")); table_->setModel(model_); table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection); table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_, 1);
        status_ = new QLabel(this); status_->setObjectName(QStringLiteral("transferStatus")); status_->setTextFormat(Qt::PlainText); status_->setWordWrap(true); layout->addWidget(status_);
        auto *buttons = new QHBoxLayout;
        auto button = [&](const QString &label, const QString &id) { auto *b = new QPushButton(label, this); b->setObjectName(id); buttons->addWidget(b); return b; };
        auto *send = button(chat_ ? QStringLiteral("New Direct Chat…") : QStringLiteral("Send File…"), chat_ ? QStringLiteral("newDirectChat") : QStringLiteral("sendTransfer")); accept_ = button(QStringLiteral("Accept"), QStringLiteral("acceptTransfer")); save_ = button(QStringLiteral("Save As…"), QStringLiteral("saveTransfer")); resume_ = button(QStringLiteral("Resume"), QStringLiteral("resumeTransfer")); abort_ = button(QStringLiteral("Abort"), QStringLiteral("abortTransfer")); remove_ = button(QStringLiteral("Remove"), QStringLiteral("removeTransfer"));
        auto *refresh = button(QStringLiteral("Refresh"), QStringLiteral("refreshTransfers")); auto *close = button(QStringLiteral("Close"), QStringLiteral("closeTransfers")); layout->addLayout(buttons);
        connect(send, &QPushButton::clicked, this, [this]() { const auto callback = chat_ ? startChat : sendFiles; if (callback) callback(); }); send->setEnabled(bool(chat_ ? startChat : sendFiles));
        open_ = button(QStringLiteral("Open Conversation"), QStringLiteral("openDirectChat")); open_->setVisible(chat_); buttons->insertWidget(1, open_);
        save_->setVisible(!chat_); resume_->setVisible(!chat_);
        if (chat_) {
            model_->setColumnCount(5);
            model_->setHorizontalHeaderLabels({QStringLiteral("Direction"), QStringLiteral("Nick"), QStringLiteral("Status"), QStringLiteral("Bytes received"), QStringLiteral("Bytes sent")});
            abort_->setText(QStringLiteral("Disconnect"));
        }
        connect(open_, &QPushButton::clicked, this, [this]() { action(5, false); });
        connect(accept_, &QPushButton::clicked, this, [this]() { action(0, false); }); connect(save_, &QPushButton::clicked, this, [this]() { action(0, true); }); connect(resume_, &QPushButton::clicked, this, [this]() { action(1, false); }); connect(abort_, &QPushButton::clicked, this, [this]() { action(2, false); }); connect(remove_, &QPushButton::clicked, this, [this]() { action(3, false); });
        connect(refresh, &QPushButton::clicked, this, [this]() { refreshRows(); }); connect(close, &QPushButton::clicked, this, &QDialog::close); connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { enable(); });
        auto *timer = new QTimer(this); connect(timer, &QTimer::timeout, this, [this]() { refreshRows(); }); timer->start(500); refreshRows();
    }
    void schedule() { if (pending_) return; pending_ = true; QTimer::singleShot(0, this, [this]() { pending_ = false; refreshRows(); }); }
private:
    int selected() const { return table_->currentIndex().row(); }
    void enable() {
        const int index = selected(); const bool valid = index >= 0 && index < rows_.size();
        const bool queued = valid && (rows_[index].type == 1 || (chat_ && rows_[index].type == 2)) && rows_[index].state == 0 && !rows_[index].pending;
        accept_->setEnabled(queued); save_->setEnabled(queued); resume_->setEnabled(queued && rows_[index].resumable > 0);
        open_->setEnabled(chat_ && valid && rows_[index].state == 1);
        abort_->setEnabled(valid && !terminal(rows_[index].state)); remove_->setEnabled(valid && terminal(rows_[index].state));
    }
    void refreshRows() {
        const quint64 id = selected() >= 0 && selected() < rows_.size() ? rows_[selected()].id : 0;
        const auto all = snapshot(); model_->removeRows(0, model_->rowCount()); rows_.clear();
        const QStringList states{QStringLiteral("Queued"), QStringLiteral("Active"), QStringLiteral("Failed"), QStringLiteral("Done"), QStringLiteral("Connecting"), QStringLiteral("Aborted")};
        for (const auto &row : all) if (chat_ ? row.type >= 2 : row.type <= 1) {
            rows_.append(row); const QString progress = row.size ? QStringLiteral("%1%").arg(qMin(100.0, 100.0 * double(row.position) / double(row.size)), 0, 'f', 1) : QStringLiteral("—");
            QList<QStandardItem *> cells;
            const QString state = states.value(row.state, QStringLiteral("Unknown"));
            const QStringList values = chat_ ? QStringList{row.type == 2 ? QStringLiteral("Incoming") : QStringLiteral("Outgoing"), row.nick, state, QString::number(row.position), QString::number(row.size)} : QStringList{row.type ? QStringLiteral("Receive") : QStringLiteral("Send"), row.nick, row.file, row.pending ? QStringLiteral("Resume requested") : states.value(row.state, QStringLiteral("Unknown")), progress, QStringLiteral("%1 / %2").arg(row.position).arg(row.size), QString::number(qMax<qint64>(0, row.speed)), row.destination};
            for (const auto &value : values) cells.append(new QStandardItem(value));
            model_->appendRow(cells); if (row.id == id) { table_->setCurrentIndex(model_->index(rows_.size() - 1, 0)); table_->selectRow(rows_.size() - 1); }
        }
        enable();
    }
    void action(int kind, bool saveAs) {
        const int index = selected(); if (index < 0 || index >= rows_.size()) return;
        const auto row = rows_[index]; QString path;
        if (saveAs) { QPointer<TransfersDialog> guard(this); path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Incoming File"), row.destination); if (!guard || path.isEmpty()) return; }
        char error[1024] = {}; QPointer<TransfersDialog> guard(this);
        const int result = qt_core_transfer_action(row.id, kind, path.toUtf8().constData(), error, sizeof(error));
        if (guard) { status_->setText(result ? QStringLiteral("Request sent to the transfer engine.") : QString::fromUtf8(error)); refreshRows(); }
    }
    QList<Transfer> rows_; QStandardItemModel *model_; QTableView *table_; QLabel *status_; QPushButton *accept_, *save_, *resume_, *abort_, *remove_, *open_; bool pending_ = false, chat_ = false;
};
QPointer<TransfersDialog> dialog, chatDialog;
}
void initializeTransfers(QWidget *parent, std::function<void()> send, std::function<void()> chat) { parentWindow = parent; sendFiles = std::move(send); startChat = std::move(chat); }
void showTransfers() { if (!dialog) dialog = new TransfersDialog(parentWindow); dialog->show(); dialog->raise(); dialog->activateWindow(); }
void showDirectChats() { if (!chatDialog) chatDialog = new TransfersDialog(parentWindow, true); chatDialog->show(); chatDialog->raise(); chatDialog->activateWindow(); }
extern "C" int qt_fe_direct_chats_open(int passive) { const bool existed = bool(chatDialog); if (!chatDialog || !passive) showDirectChats(); return existed; }
extern "C" void qt_fe_transfers_changed(void) { if (dialog) dialog->schedule(); if (chatDialog) chatDialog->schedule(); }
extern "C" int qt_fe_transfers_open(int passive) { const bool existed = bool(dialog); if (!dialog || !passive) showTransfers(); return existed; }
extern "C" void qt_fe_transfer_offer(uint64_t id, const char *message)
{
    const QString text = QString::fromUtf8(message);
    QTimer::singleShot(0, parentWindow, [id, text]() {
        Transfer offer{}; bool found = false;
        for (const auto &row : snapshot()) if (row.id == id && row.state == 0 && !row.pending) { offer = row; found = true; break; }
        if (!found) return;
        QString path; int action = 4;
        if (offer.type == 1) { path = QFileDialog::getSaveFileName(parentWindow, text, offer.destination); if (!path.isEmpty()) action = 0; }
        else if (offer.type == 2 && QMessageBox::question(parentWindow, QStringLiteral("Direct Chat Offer"), text) == QMessageBox::Yes) action = 0;
        char error[1024] = {}; qt_core_transfer_action(id, action, path.toUtf8().constData(), error, sizeof(error));
    });
}
