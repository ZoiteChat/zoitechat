/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "moderation.h"
#include "moderation-bridge.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDeadlineTimer>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <climits>

namespace {
class ChannelDialog : public QDialog {
public:
    ChannelDialog(void *session, QWidget *parent) : QDialog(parent), session_(session)
    {
        QtChannelState state{};
        if (qt_core_channel_state(session, &state)) channel_ = QString::fromUtf8(state.channel);
    }
    void invalidate() { valid_ = false; hide(); deleteLater(); }
protected:
    bool state(QtChannelState &snapshot) const
    {
        return valid_ && qt_core_channel_state(session_, &snapshot) &&
               channel_ == QString::fromUtf8(snapshot.channel);
    }
    bool change(char mode, bool enable, const QString &value)
    {
        QtChannelState snapshot{};
        return state(snapshot) && qt_core_channel_mode(session_, channel_.toUtf8().constData(), mode, enable,
                                                       value.toUtf8().constData());
    }
    void *session_;
    QString channel_;
    bool valid_ = true;
};
class ChannelSettings final : public ChannelDialog {
public:
    ChannelSettings(void *session, QWidget *parent) : ChannelDialog(session, parent)
    {
        setObjectName(QStringLiteral("channelSettings"));
        setWindowTitle(QStringLiteral("Channel Settings — ") + channel_);
        auto *form = new QFormLayout(this);
        current_ = new QLabel(this); current_->setObjectName(QStringLiteral("currentChannelModes"));
        current_->setTextFormat(Qt::PlainText); form->addRow(current_);
        const QString letters = QStringLiteral("ntimsp");
        const QStringList descriptions = {QStringLiteral("No outside messages (+n)"), QStringLiteral("Only operators change topic (+t)"),
            QStringLiteral("Invite only (+i)"), QStringLiteral("Moderated (+m)"), QStringLiteral("Secret (+s)"), QStringLiteral("Private (+p)")};
        for (int i = 0; i < letters.size(); ++i) {
            auto *check = new QCheckBox(descriptions[i], this);
            check->setObjectName(QStringLiteral("channelMode_") + letters[i]);
            modes_.insert(letters[i], check); form->addRow(check);
            connect(check, &QCheckBox::clicked, this, [this, mode = letters[i]](bool enabled) {
                report(change(mode.toLatin1(), enabled, QString())); refresh();
            });
        }
        keyState_ = new QLabel(this); form->addRow(keyState_);
        key_ = new QLineEdit(this); key_->setEchoMode(QLineEdit::Password);
        key_->setObjectName(QStringLiteral("channelKey")); key_->setMaxLength(63);
        form->addRow(QStringLiteral("New key:"), key_);
        auto *keyButtons = new QHBoxLayout;
        auto *setKey = new QPushButton(QStringLiteral("Set Key"), this); setKey->setObjectName(QStringLiteral("setChannelKey"));
        auto *clearKey = new QPushButton(QStringLiteral("Remove Key"), this); clearKey->setObjectName(QStringLiteral("removeChannelKey"));
        keyButtons->addWidget(setKey); keyButtons->addWidget(clearKey); form->addRow(keyButtons);
        limit_ = new QSpinBox(this); limit_->setRange(1, INT_MAX); limit_->setObjectName(QStringLiteral("channelLimit"));
        QtChannelState initial{};
        if (state(initial)) limit_->setValue(qMax(1, initial.limit));
        form->addRow(QStringLiteral("New user limit:"), limit_);
        auto *limitButtons = new QHBoxLayout;
        auto *setLimit = new QPushButton(QStringLiteral("Set Limit"), this); setLimit->setObjectName(QStringLiteral("setChannelLimit"));
        auto *clearLimit = new QPushButton(QStringLiteral("Remove Limit"), this); clearLimit->setObjectName(QStringLiteral("removeChannelLimit"));
        limitButtons->addWidget(setLimit); limitButtons->addWidget(clearLimit); form->addRow(limitButtons);
        const QString supported = QString::fromUtf8(initial.supported ? initial.supported : "");
        setKey->setEnabled(supported.contains(QLatin1Char('k'))); clearKey->setEnabled(setKey->isEnabled());
        setLimit->setEnabled(supported.contains(QLatin1Char('l'))); clearLimit->setEnabled(setLimit->isEnabled());
        status_ = new QLabel(QStringLiteral("Server permissions apply. Changes appear after server confirmation."), this);
        status_->setObjectName(QStringLiteral("channelSettingsStatus")); status_->setWordWrap(true); form->addRow(status_);
        auto *buttons = new QHBoxLayout;
        auto *refreshButton = new QPushButton(QStringLiteral("Refresh"), this);
        refreshButton->setObjectName(QStringLiteral("refreshChannelModes"));
        auto *close = new QPushButton(QStringLiteral("Close"), this);
        buttons->addWidget(refreshButton); buttons->addStretch(); buttons->addWidget(close); form->addRow(buttons);
        connect(setKey, &QPushButton::clicked, this, [this]() { const bool sent = change('k', true, key_->text()); if (sent) key_->clear(); report(sent); });
        connect(clearKey, &QPushButton::clicked, this, [this]() { report(change('k', false, QString())); });
        connect(setLimit, &QPushButton::clicked, this, [this]() { report(change('l', true, QString::number(limit_->value()))); });
        connect(clearLimit, &QPushButton::clicked, this, [this]() { report(change('l', false, QString())); });
        connect(refreshButton, &QPushButton::clicked, this, [this]() {
            QtChannelState snapshot{};
            report(state(snapshot) && qt_core_channel_refresh(session_, channel_.toUtf8().constData())); refresh();
        });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this, setKey, clearKey, setLimit, clearLimit]() {
            QtChannelState snapshot{};
            const bool available = state(snapshot);
            const QString supported = available ? QString::fromUtf8(snapshot.supported) : QString();
            setKey->setEnabled(supported.contains(QLatin1Char('k'))); clearKey->setEnabled(setKey->isEnabled());
            setLimit->setEnabled(supported.contains(QLatin1Char('l'))); clearLimit->setEnabled(setLimit->isEnabled());
            refresh();
        });
        timer->start(1000);
        refresh();
        qt_core_channel_refresh(session_, channel_.toUtf8().constData());
    }
    void refresh()
    {
        QtChannelState snapshot{};
        if (!state(snapshot)) { invalidate(); return; }
        const QString active = QString::fromUtf8(snapshot.modes).section(QLatin1Char(' '), 0, 0);
        const QString simple = QString::fromUtf8(snapshot.simple);
        current_->setText(QStringLiteral("Confirmed modes: %1").arg(active));
        keyState_->setText(QStringLiteral("Key: %1 · Current user limit: %2")
            .arg(snapshot.key_set ? QStringLiteral("enabled") : QStringLiteral("disabled"))
            .arg(snapshot.limit ? QString::number(snapshot.limit) : QStringLiteral("none")));
        for (auto it = modes_.cbegin(); it != modes_.cend(); ++it) {
            it.value()->setEnabled(simple.contains(it.key())); it.value()->setChecked(active.contains(it.key()));
        }
    }
private:
    void report(bool sent)
    {
        status_->setText(sent ? QStringLiteral("Request sent. Waiting for server confirmation; errors appear in chat.") :
                               QStringLiteral("Request not sent: check the channel, supported modes and value (no spaces)."));
    }
    QMap<QChar, QCheckBox *> modes_;
    QLabel *current_, *keyState_, *status_;
    QLineEdit *key_;
    QSpinBox *limit_;
};

char rowMode(int code)
{
    switch (code) { case 367: case 368: return 'b'; case 348: case 349: return 'e';
                    case 346: case 347: return 'I'; case 728: case 729: return 'q'; default: return 0; }
}
class BanManager final : public ChannelDialog {
public:
    BanManager(void *session, QWidget *parent) : ChannelDialog(session, parent)
    {
        setObjectName(QStringLiteral("banManager")); setWindowTitle(QStringLiteral("Ban List — ") + channel_); resize(800, 450);
        auto *layout = new QVBoxLayout(this);
        kind_ = new QComboBox(this); kind_->setObjectName(QStringLiteral("banListKind"));
        QtChannelState snapshot{}; state(snapshot);
        const QStringList labels = {QStringLiteral("Bans (+b)"), QStringLiteral("Ban exceptions (+e)"),
                                    QStringLiteral("Invite exceptions (+I)"), QStringLiteral("Quiets (+q)")};
        const QString letters = QStringLiteral("beIq");
        for (int i = 0; i < 4; ++i) if (snapshot.lists & (1 << i)) kind_->addItem(labels[i], letters[i]);
        layout->addWidget(kind_);
        filter_ = new QLineEdit(this); filter_->setObjectName(QStringLiteral("banFilter"));
        filter_->setPlaceholderText(QStringLiteral("Filter masks or setters")); layout->addWidget(filter_);
        model_ = new QStandardItemModel(this);
        model_->setHorizontalHeaderLabels({QStringLiteral("Mask"), QStringLiteral("Set by"), QStringLiteral("When")});
        proxy_ = new QSortFilterProxyModel(this); proxy_->setSourceModel(model_);
        proxy_->setFilterKeyColumn(-1); proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("banRows")); table_->setModel(proxy_);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSortingEnabled(true);
        table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_, 1);
        status_ = new QLabel(this); status_->setObjectName(QStringLiteral("banListStatus")); status_->setWordWrap(true); layout->addWidget(status_);
        auto *buttons = new QHBoxLayout;
        refresh_ = new QPushButton(QStringLiteral("Refresh"), this); refresh_->setObjectName(QStringLiteral("refreshBans"));
        auto *add = new QPushButton(QStringLiteral("Add Mask…"), this); add->setObjectName(QStringLiteral("addBanMask"));
        auto *remove = new QPushButton(QStringLiteral("Remove Selected"), this); remove->setObjectName(QStringLiteral("removeBanMasks"));
        auto *copy = new QPushButton(QStringLiteral("Copy Selected"), this); copy->setObjectName(QStringLiteral("copyBanMasks"));
        auto *close = new QPushButton(QStringLiteral("Close"), this);
        buttons->addWidget(refresh_); buttons->addWidget(add); buttons->addWidget(remove); buttons->addWidget(copy); buttons->addStretch(); buttons->addWidget(close);
        layout->addLayout(buttons);
        connect(filter_, &QLineEdit::textChanged, proxy_, &QSortFilterProxyModel::setFilterFixedString);
        connect(refresh_, &QPushButton::clicked, this, [this]() { request(); });
        connect(kind_, &QComboBox::currentIndexChanged, this, [this]() { request(); });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        connect(copy, &QPushButton::clicked, this, [this]() { QApplication::clipboard()->setText(selectedMasks().join(QLatin1Char('\n'))); });
        connect(remove, &QPushButton::clicked, this, [this]() {
            const char mode = currentMode(); const QStringList masks = selectedMasks();
            bool sent = false; for (const QString &mask : masks) sent |= change(mode, false, mask);
            if (sent) status_->setText(QStringLiteral("Removal requested; Refresh after the server confirms changes in chat."));
        });
        connect(add, &QPushButton::clicked, this, [this]() {
            const QPointer<BanManager> guard(this);
            const char mode = currentMode(); bool accepted = false;
            const QString mask = QInputDialog::getText(this, QStringLiteral("Add Channel Mask"),
                QStringLiteral("Mask (for example nick!*@*):"), QLineEdit::Normal, QString(), &accepted).trimmed();
            if (guard && accepted && valid_) status_->setText(change(mode, true, mask) ?
                QStringLiteral("Mask requested; Refresh after the server confirms changes in chat.") : QStringLiteral("Mask not sent: check value, connection and supported list."));
        });
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            QtChannelState snapshot{}; if (!state(snapshot)) { invalidate(); return; }
            if (pending_ && deadline_.hasExpired()) finish(QStringLiteral("List request timed out or was denied. Check chat, then Refresh."));
        });
        timer->start(1000);
        request();
    }
    void request()
    {
        QtChannelState snapshot{};
        if (pending_ || !state(snapshot) || !currentMode()) return;
        model_->removeRows(0, model_->rowCount()); masks_.clear();
        pending_ = currentMode(); refresh_->setEnabled(false); kind_->setEnabled(false); deadline_.setRemainingTime(15000);
        status_->setText(QStringLiteral("Receiving list…"));
        if (!qt_core_ban_request(session_, channel_.toUtf8().constData(), pending_)) finish(QStringLiteral("List unavailable."));
    }
    bool addRow(const char *mask, const char *who, const char *when, int code)
    {
        if (!valid_ || !pending_ || rowMode(code) != pending_) return false;
        const QString value = QString::fromUtf8(mask ? mask : "");
        if (!masks_.contains(value) && model_->rowCount() < 10000) {
            masks_.insert(value);
            model_->appendRow({new QStandardItem(value), new QStandardItem(QString::fromUtf8(who ? who : "")),
                               new QStandardItem(QString::fromUtf8(when ? when : ""))});
        }
        return true;
    }
    bool end(int code)
    {
        if (!valid_ || !pending_ || rowMode(code) != pending_) return false;
        finish(QStringLiteral("%1 masks received. Server permissions apply.").arg(model_->rowCount())); return true;
    }
private:
    char currentMode() const { return kind_->currentData().toString().isEmpty() ? 0 : kind_->currentData().toString()[0].toLatin1(); }
    QStringList selectedMasks() const
    {
        QStringList result;
        for (const auto &index : table_->selectionModel()->selectedRows(0)) result.append(index.data().toString());
        return result;
    }
    void finish(const QString &text) { pending_ = 0; refresh_->setEnabled(true); kind_->setEnabled(true); status_->setText(text); }
    QComboBox *kind_;
    QLineEdit *filter_;
    QStandardItemModel *model_;
    QSortFilterProxyModel *proxy_;
    QTableView *table_;
    QPushButton *refresh_;
    QLabel *status_;
    QSet<QString> masks_;
    QDeadlineTimer deadline_;
    char pending_ = 0;
};
QHash<void *, QPointer<ChannelSettings>> settings;
QHash<void *, QPointer<BanManager>> bans;
}
void showChannelSettings(void *session, QWidget *parent)
{
    QtChannelState snapshot{}; if (!qt_core_channel_state(session, &snapshot)) return;
    if (!settings.value(session)) settings[session] = new ChannelSettings(session, parent);
    settings[session]->show(); settings[session]->raise(); settings[session]->activateWindow();
}
void showBanManager(void *session, QWidget *parent)
{
    QtChannelState snapshot{}; if (!qt_core_channel_state(session, &snapshot)) return;
    if (!bans.value(session)) bans[session] = new BanManager(session, parent);
    bans[session]->show(); bans[session]->raise(); bans[session]->activateWindow();
}
extern "C" {
void qt_fe_moderation_closed(void *session)
{
    if (auto *dialog = settings.take(session).data()) dialog->invalidate();
    if (auto *dialog = bans.take(session).data()) dialog->invalidate();
}
int qt_fe_ban_row(void *session, const char *mask, const char *who, const char *when, int code)
{ return bans.value(session) && bans[session]->addRow(mask, who, when, code); }
int qt_fe_ban_end(void *session, int code) { return bans.value(session) && bans[session]->end(code); }
}
