/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "friends.h"
#include "friends-bridge.h"
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
namespace {
QPointer<QWidget> friendsParent;
QString timeText(time_t value) { return value > 0 ? QDateTime::fromSecsSinceEpoch(value).toLocalTime().toString(Qt::ISODate) : QStringLiteral("Unknown"); }
class FriendsList final : public QDialog {
public:
    explicit FriendsList(QWidget *parent) : QDialog(parent)
    {
        setObjectName(QStringLiteral("friendsList")); setWindowTitle(QStringLiteral("Friends List")); resize(900, 450);
        auto *layout = new QVBoxLayout(this);
        auto *filter = new QLineEdit(this); filter->setObjectName(QStringLiteral("friendsFilter"));
        filter->setPlaceholderText(QStringLiteral("Filter nicknames, networks or status")); layout->addWidget(filter);
        model_ = new QStandardItemModel(this);
        model_->setHorizontalHeaderLabels({QStringLiteral("Nickname"), QStringLiteral("Status"), QStringLiteral("Network"),
            QStringLiteral("Watched networks"), QStringLiteral("Last seen"), QStringLiteral("Last online"), QStringLiteral("Last offline")});
        proxy_ = new QSortFilterProxyModel(this); proxy_->setSourceModel(model_); proxy_->setFilterKeyColumn(-1);
        proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("friendRows")); table_->setModel(proxy_);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSortingEnabled(true);
        table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_, 1);
        status_ = new QLabel(QStringLiteral("Presence follows the engine's MONITOR/WATCH/ISON support. Unknown means no connected network has reported status."), this);
        status_->setTextFormat(Qt::PlainText); status_->setWordWrap(true); layout->addWidget(status_);
        auto *buttons = new QHBoxLayout;
        auto button = [&](const QString &text, const QString &id) { auto *b = new QPushButton(text, this); b->setObjectName(id); buttons->addWidget(b); return b; };
        auto *add = button(QStringLiteral("Add…"), QStringLiteral("addFriend"));
        auto *edit = button(QStringLiteral("Edit…"), QStringLiteral("editFriend"));
        auto *remove = button(QStringLiteral("Remove"), QStringLiteral("removeFriend"));
        query_ = button(QStringLiteral("Open Dialog"), QStringLiteral("queryFriend")); query_->setEnabled(false);
        auto *refresh = button(QStringLiteral("Refresh"), QStringLiteral("refreshFriends"));
        buttons->addStretch(); auto *close = button(QStringLiteral("Close"), QStringLiteral("closeFriends")); layout->addLayout(buttons);
        connect(filter, &QLineEdit::textChanged, proxy_, &QSortFilterProxyModel::setFilterFixedString);
        connect(add, &QPushButton::clicked, this, [this]() { editor(QString(), QString(), QString()); });
        connect(edit, &QPushButton::clicked, this, [this]() {
            const auto index = current(); if (!index.isValid()) return;
            const QString name = index.data().toString(); editor(name, name, index.data(Qt::UserRole + 2).toString());
        });
        connect(remove, &QPushButton::clicked, this, [this]() {
            const QString name = current().data().toString(); if (!name.isEmpty()) qt_core_friend_remove(name.toUtf8().constData());
        });
        connect(query_, &QPushButton::clicked, this, [this]() { query(); });
        connect(table_, &QTableView::doubleClicked, this, [this]() { query(); });
        connect(table_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { updateSelection(); });
        connect(refresh, &QPushButton::clicked, this, [this]() { reload(); });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        auto *timer = new QTimer(this); connect(timer, &QTimer::timeout, this, [this]() { if (isVisible()) reload(); }); timer->start(5000);
        reload();
    }
    void scheduleReload()
    {
        if (pending_) return;
        pending_ = true; QTimer::singleShot(0, this, [this]() { pending_ = false; reload(); });
    }
    void reload()
    {
        const QString name = current().data().toString(), network = current().data(Qt::UserRole + 3).toString();
        model_->removeRows(0, model_->rowCount());
        qt_core_friends_foreach([](const char *name, const char *networks, const char *network, void *server,
                                  int online, time_t seen, time_t on, time_t off, void *data) {
            auto *self = static_cast<FriendsList *>(data);
            auto *nick = new QStandardItem(QString::fromUtf8(name));
            nick->setData(QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(server)), Qt::UserRole);
            nick->setData(online, Qt::UserRole + 1); nick->setData(QString::fromUtf8(networks ? networks : ""), Qt::UserRole + 2);
            nick->setData(QString::fromUtf8(network ? network : ""), Qt::UserRole + 3);
            self->model_->appendRow({nick, new QStandardItem(online < 0 ? QStringLiteral("Unknown") : online ? QStringLiteral("Online") : QStringLiteral("Offline")),
                new QStandardItem(QString::fromUtf8(network ? network : "")), new QStandardItem(networks && *networks ? QString::fromUtf8(networks) : QStringLiteral("All")),
                new QStandardItem(timeText(seen)), new QStandardItem(timeText(on)), new QStandardItem(timeText(off))});
        }, this);
        for (int row = 0; row < proxy_->rowCount(); ++row) {
            const auto index = proxy_->index(row, 0);
            if (index.data().toString() == name && index.data(Qt::UserRole + 3).toString() == network) table_->selectRow(row);
        }
        updateSelection();
    }
    void editor(const QString &oldName, const QString &name, const QString &networks)
    {
        auto *dialog = new QDialog(this); dialog->setObjectName(QStringLiteral("friendEditor")); dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(oldName.isEmpty() ? QStringLiteral("Add Friend") : QStringLiteral("Edit Friend"));
        auto *form = new QFormLayout(dialog);
        auto *nick = new QLineEdit(name, dialog); nick->setObjectName(QStringLiteral("friendName"));
        auto *nets = new QLineEdit(networks, dialog); nets->setObjectName(QStringLiteral("friendNetworks"));
        nets->setPlaceholderText(QStringLiteral("Comma-separated network names; empty means all"));
        form->addRow(QStringLiteral("Nickname:"), nick); form->addRow(QStringLiteral("Networks:"), nets);
        auto *error = new QLabel(dialog); error->setObjectName(QStringLiteral("friendError")); error->setTextFormat(Qt::PlainText); error->setWordWrap(true); form->addRow(error);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog); form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, dialog, [oldName, nick, nets, error, dialog]() {
            if (!qt_core_friend_set(oldName.toUtf8().constData(), nick->text().trimmed().toUtf8().constData(), nets->text().trimmed().toUtf8().constData())) {
                error->setText(QStringLiteral("Use a unique valid nickname and comma-separated networks. The original friend must still exist.")); return;
            }
            dialog->accept();
        });
        dialog->setWindowModality(Qt::WindowModal); dialog->open();
    }
private:
    QModelIndex current() const { const auto index = table_->currentIndex(); return index.isValid() ? proxy_->index(index.row(), 0) : QModelIndex(); }
    void updateSelection() { query_->setEnabled(current().isValid() && current().data(Qt::UserRole + 1).toInt() == 1); }
    void query()
    {
        const auto index = current(); if (!index.isValid()) return;
        void *server = reinterpret_cast<void *>(index.data(Qt::UserRole).value<quintptr>());
        if (!qt_core_friend_query(index.data().toString().toUtf8().constData(), server, index.data(Qt::UserRole + 3).toString().toUtf8().constData()))
            status_->setText(QStringLiteral("Unable to open dialog: this friend or network is no longer online."));
    }
    QStandardItemModel *model_;
    QSortFilterProxyModel *proxy_;
    QTableView *table_;
    QLabel *status_;
    QPushButton *query_;
    bool pending_ = false;
};
QPointer<FriendsList> friends;
}
void initializeFriends(QWidget *parent) { friendsParent = parent; }
void showFriendsList()
{
    if (!friends) friends = new FriendsList(friendsParent);
    friends->reload(); friends->show(); friends->raise(); friends->activateWindow();
}
extern "C" {
void qt_fe_friends_changed(void) { if (friends) friends->scheduleReload(); }
void qt_fe_friend_ask(const char *name, const char *networks)
{
    showFriendsList(); friends->editor(QString(), QString::fromUtf8(name ? name : ""), QString::fromUtf8(networks ? networks : ""));
}
}
