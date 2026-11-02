/*
 * Qt 6 network manager using the existing ZoiteChat servlist configuration.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "networkdialog.h"
#include "qt-network.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QStringList>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QPointer>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QString utf(const char *value)
{
    return QString::fromUtf8(value ? value : "");
}

class NetworkEditor final : public QDialog {
public:
    explicit NetworkEditor(QWidget *parent, const QtNetwork *existing = nullptr)
        : QDialog(parent), identity_(existing ? existing->identity : nullptr)
    {
        setWindowTitle(existing ? QStringLiteral("Edit IRC Network") :
                                  QStringLiteral("Add IRC Network"));
        resize(700, 680);
        auto *layout = new QVBoxLayout(this);
        auto *tabs = new QTabWidget(this);
        layout->addWidget(tabs, 1);

        auto *serversPage = new QWidget(tabs);
        auto *serversForm = new QFormLayout(serversPage);
        name_ = new QLineEdit(serversPage);
        name_->setPlaceholderText(QStringLiteral("My IRC Network"));
        servers_ = new QPlainTextEdit(serversPage);
        servers_->setPlaceholderText(
            QStringLiteral("irc.example.org/+6697\nirc-backup.example.org/+6697"));
        servers_->setMinimumHeight(160);
        selected_ = new QSpinBox(serversPage);
        selected_->setRange(1, 1);
        selected_->setToolTip(QStringLiteral(
            "Preferred server from the list above (1 is the first server)"));
        serversForm->addRow(QStringLiteral("Network name:"), name_);
        serversForm->addRow(QStringLiteral("Servers (one per line):"), servers_);
        serversForm->addRow(QStringLiteral("Preferred server number:"), selected_);
        auto *serverHelp = new QLabel(
            QStringLiteral("Enter hostnames, optionally followed by /port or /+port for TLS. "
                           "Example: irc.libera.chat/+6697"), serversPage);
        serverHelp->setWordWrap(true);
        serversForm->addRow(serverHelp);
        tabs->addTab(serversPage, QStringLiteral("Servers"));

        auto *identityPage = new QWidget(tabs);
        auto *identityForm = new QFormLayout(identityPage);
        globalIdentity_ = new QCheckBox(
            QStringLiteral("Use global ZoiteChat identity settings"), identityPage);
        nick_ = new QLineEdit(identityPage);
        nick2_ = new QLineEdit(identityPage);
        user_ = new QLineEdit(identityPage);
        real_ = new QLineEdit(identityPage);
        password_ = new QLineEdit(identityPage);
        password_->setEchoMode(QLineEdit::Password);
        password_->setPlaceholderText(QStringLiteral("Optional authentication password"));
        auth_ = new QComboBox(identityPage);
        auth_->addItem(QStringLiteral("Default (server PASS)"), QT_NET_LOGIN_DEFAULT);
        auth_->addItem(QStringLiteral("NickServ MESSAGE"), QT_NET_LOGIN_MSG_NICKSERV);
        auth_->addItem(QStringLiteral("NickServ IDENTIFY"), QT_NET_LOGIN_NICKSERV);
        auth_->addItem(QStringLiteral("SASL PLAIN"), QT_NET_LOGIN_SASL);
        auth_->addItem(QStringLiteral("Server password (PASS)"), QT_NET_LOGIN_PASS);
        auth_->addItem(QStringLiteral("QuakeNet CHALLENGEAUTH"), QT_NET_LOGIN_CHALLENGE);
        auth_->addItem(QStringLiteral("Custom connect command"), QT_NET_LOGIN_CUSTOM);
        auth_->addItem(QStringLiteral("SASL EXTERNAL"), QT_NET_LOGIN_SASL_EXTERNAL);
        auth_->addItem(QStringLiteral("SASL SCRAM SHA-1"), QT_NET_LOGIN_SCRAM_SHA1);
        auth_->addItem(QStringLiteral("SASL SCRAM SHA-256"), QT_NET_LOGIN_SCRAM_SHA256);
        auth_->addItem(QStringLiteral("SASL SCRAM SHA-512"), QT_NET_LOGIN_SCRAM_SHA512);
        identityForm->addRow(globalIdentity_);
        identityForm->addRow(QStringLiteral("Nickname:"), nick_);
        identityForm->addRow(QStringLiteral("Backup nickname:"), nick2_);
        identityForm->addRow(QStringLiteral("Username:"), user_);
        identityForm->addRow(QStringLiteral("Real name:"), real_);
        identityForm->addRow(QStringLiteral("Authentication method:"), auth_);
        identityForm->addRow(QStringLiteral("Password:"), password_);
        auto *warning = new QLabel(
            QStringLiteral("ZoiteChat's legacy servlist.conf stores authentication passwords "
                           "as plaintext. Protect your configuration directory."), identityPage);
        warning->setWordWrap(true);
        identityForm->addRow(warning);
        tabs->addTab(identityPage, QStringLiteral("Identity & login"));

        auto *optionsPage = new QWidget(tabs);
        auto *optionsLayout = new QVBoxLayout(optionsPage);
        auto *form = new QFormLayout;
        channels_ = new QPlainTextEdit(optionsPage);
        channels_->setPlaceholderText(
            QStringLiteral("#channel\n#private-channel,key"));
        channels_->setMinimumHeight(110);
        commands_ = new QPlainTextEdit(optionsPage);
        commands_->setPlaceholderText(
            QStringLiteral("Commands to execute after connecting, one per line"));
        commands_->setMinimumHeight(90);
        encoding_ = new QLineEdit(optionsPage);
        encoding_->setPlaceholderText(QStringLiteral("UTF-8 (Unicode)"));
        form->addRow(QStringLiteral("Auto-join channels:"), channels_);
        form->addRow(QStringLiteral("Connect commands:"), commands_);
        form->addRow(QStringLiteral("Character encoding:"), encoding_);
        optionsLayout->addLayout(form);
        tls_ = new QCheckBox(QStringLiteral("Use TLS for this network"), optionsPage);
        invalidCert_ = new QCheckBox(QStringLiteral(
            "Allow invalid TLS certificates (not recommended)"), optionsPage);
        autoConnect_ = new QCheckBox(QStringLiteral("Automatically connect at startup"), optionsPage);
        cycle_ = new QCheckBox(QStringLiteral("Cycle servers when reconnecting"), optionsPage);
        proxy_ = new QCheckBox(QStringLiteral("Use configured proxy"), optionsPage);
        favorite_ = new QCheckBox(QStringLiteral("Mark as favourite"), optionsPage);
        for (auto *check : {tls_, invalidCert_, autoConnect_, cycle_, proxy_, favorite_})
            optionsLayout->addWidget(check);
        optionsLayout->addStretch();
        tabs->addTab(optionsPage, QStringLiteral("Channels & options"));

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                             QDialogButtonBox::Cancel, this);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() { save(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        connect(servers_, &QPlainTextEdit::textChanged, this, [this]() {
            const auto entries = servers_->toPlainText().split(QLatin1Char('\n'),
                                                               Qt::SkipEmptyParts);
            selected_->setMaximum(qMax(1, int(entries.size())));
        });
        connect(globalIdentity_, &QCheckBox::toggled, this, [this](bool checked) {
            for (auto *field : {nick_, nick2_, user_, real_})
                field->setEnabled(!checked);
        });

        if (existing) {
            name_->setText(utf(existing->name));
            servers_->setPlainText(utf(existing->servers));
            channels_->setPlainText(utf(existing->channels));
            commands_->setPlainText(utf(existing->commands));
            nick_->setText(utf(existing->nick));
            nick2_->setText(utf(existing->nick2));
            user_->setText(utf(existing->user));
            real_->setText(utf(existing->real));
            password_->setText(utf(existing->password));
            encoding_->setText(utf(existing->encoding));
            int index = auth_->findData(existing->login_type);
            auth_->setCurrentIndex(index >= 0 ? index : 0);
            globalIdentity_->setChecked(existing->flags & QT_NET_GLOBAL_IDENTITY);
            tls_->setChecked(existing->flags & QT_NET_TLS);
            invalidCert_->setChecked(existing->flags & QT_NET_ALLOW_INVALID_CERT);
            autoConnect_->setChecked(existing->flags & QT_NET_AUTOCONNECT);
            cycle_->setChecked(existing->flags & QT_NET_CYCLE);
            proxy_->setChecked(existing->flags & QT_NET_PROXY);
            favorite_->setChecked(existing->flags & QT_NET_FAVORITE);
            selected_->setValue(existing->selected + 1);
        } else {
            globalIdentity_->setChecked(true);
            tls_->setChecked(true);
            cycle_->setChecked(true);
            proxy_->setChecked(true);
            encoding_->setText(QStringLiteral("UTF-8 (Unicode)"));
        }
    }

    void *savedNetwork() const { return saved_; }

private:
    void save()
    {
        const QString networkName = name_->text().trimmed();
        if (networkName.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Network"),
                                 QStringLiteral("Enter a network name."));
            return;
        }
        // QByteArrays own their bytes until the synchronous C call completes.
        QByteArray name = networkName.toUtf8();
        QByteArray servers = servers_->toPlainText().toUtf8();
        QByteArray channels = channels_->toPlainText().toUtf8();
        QByteArray commands = commands_->toPlainText().toUtf8();
        QByteArray nick = nick_->text().toUtf8();
        QByteArray nick2 = nick2_->text().toUtf8();
        QByteArray user = user_->text().toUtf8();
        QByteArray real = real_->text().toUtf8();
        QByteArray password = password_->text().toUtf8();
        QByteArray encoding = encoding_->text().trimmed().toUtf8();

        QtNetwork record{};
        record.identity = identity_;
        record.name = name.data();
        record.servers = servers.data();
        record.channels = channels.data();
        record.commands = commands.data();
        record.nick = nick.data();
        record.nick2 = nick2.data();
        record.user = user.data();
        record.real = real.data();
        record.password = password.data();
        record.encoding = encoding.data();
        record.login_type = auth_->currentData().toInt();
        record.selected = selected_->value() - 1;
        if (globalIdentity_->isChecked()) record.flags |= QT_NET_GLOBAL_IDENTITY;
        if (tls_->isChecked()) record.flags |= QT_NET_TLS;
        if (invalidCert_->isChecked()) record.flags |= QT_NET_ALLOW_INVALID_CERT;
        if (autoConnect_->isChecked()) record.flags |= QT_NET_AUTOCONNECT;
        if (cycle_->isChecked()) record.flags |= QT_NET_CYCLE;
        if (proxy_->isChecked()) record.flags |= QT_NET_PROXY;
        if (favorite_->isChecked()) record.flags |= QT_NET_FAVORITE;

        char error[256] = {0};
        void *saved = nullptr;
        const bool ok = qt_net_store(&record, &saved, error, sizeof(error));
        // Do not leave extra password buffers in memory after saving.
        password.fill('\0');
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("Unable to save network"),
                                 QString::fromUtf8(error));
            // An I/O error may leave the edit in memory; the manager refreshes
            // if the editor is reopened.
            return;
        }
        saved_ = saved;
        accept();
    }

    void *identity_ = nullptr;
    void *saved_ = nullptr;
    QLineEdit *name_ = nullptr, *nick_ = nullptr, *nick2_ = nullptr;
    QLineEdit *user_ = nullptr, *real_ = nullptr, *password_ = nullptr;
    QLineEdit *encoding_ = nullptr;
    QPlainTextEdit *servers_ = nullptr, *channels_ = nullptr, *commands_ = nullptr;
    QSpinBox *selected_ = nullptr;
    QComboBox *auth_ = nullptr;
    QCheckBox *globalIdentity_ = nullptr, *tls_ = nullptr, *invalidCert_ = nullptr;
    QCheckBox *autoConnect_ = nullptr, *cycle_ = nullptr, *proxy_ = nullptr, *favorite_ = nullptr;
};

class NetworkManager final : public QDialog {
public:
    explicit NetworkManager(QWidget *parent) : QDialog(parent)
    {
        setObjectName(QStringLiteral("networkManager"));
        setWindowTitle(QStringLiteral("ZoiteChat — Network List"));
        resize(650, 520);
        auto *layout = new QVBoxLayout(this);
        auto *intro = new QLabel(
            QStringLiteral("Manage saved IRC networks. Changes are saved to ZoiteChat's "
                           "existing servlist.conf file."), this);
        intro->setWordWrap(true);
        layout->addWidget(intro);
        networks_ = new QListWidget(this); networks_->setObjectName(QStringLiteral("networkList"));
        networks_->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(networks_, 1);

        auto *actions = new QHBoxLayout;
        auto *add = new QPushButton(QStringLiteral("&Add"), this);
        auto *edit = new QPushButton(QStringLiteral("&Edit…"), this); edit->setObjectName(QStringLiteral("editNetwork"));
        auto *remove = new QPushButton(QStringLiteral("&Remove"), this);
        auto *connectButton = new QPushButton(QStringLiteral("&Connect"), this); connectButton->setObjectName(QStringLiteral("connectNetwork"));
        for (auto *button : {edit, remove, connectButton}) button->setEnabled(false);
        actions->addWidget(add);
        actions->addWidget(edit);
        actions->addWidget(remove);
        actions->addStretch();
        actions->addWidget(connectButton);
        layout->addLayout(actions);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        layout->addWidget(buttons);

        connect(add, &QPushButton::clicked, this, [this]() {
            runEditor(new NetworkEditor(this));
        });
        connect(edit, &QPushButton::clicked, this, [this]() { editSelected(); });
        connect(remove, &QPushButton::clicked, this, [this]() { removeSelected(); });
        connect(connectButton, &QPushButton::clicked, this, [this]() { connectSelected(); });
        connect(networks_, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem *) { editSelected(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(networks_, &QListWidget::itemSelectionChanged, this, [this, edit, remove, connectButton]() {
            const bool selected = networks_->currentItem() != nullptr;
            edit->setEnabled(selected);
            remove->setEnabled(selected);
            connectButton->setEnabled(selected);
        });
        refresh(nullptr);
    }

private:
    void *selectedIdentity() const
    {
        auto *item = networks_->currentItem();
        if (!item) return nullptr;
        return reinterpret_cast<void *>(item->data(Qt::UserRole).value<quintptr>());
    }
    void refresh(void *preferred)
    {
        if (!preferred) preferred = selectedIdentity();
        networks_->clear();
        int selectedRow = -1;
        for (int i = 0; i < qt_net_count(); ++i) {
            QtNetwork record{};
            if (!qt_net_read(i, &record)) continue;
            auto *item = new QListWidgetItem(utf(record.name), networks_);
            item->setData(Qt::UserRole,
                          QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(record.identity)));
            QStringList details;
            if (record.flags & QT_NET_AUTOCONNECT) details << QStringLiteral("autoconnect");
            if (record.flags & QT_NET_TLS) details << QStringLiteral("TLS");
            item->setToolTip(details.join(QStringLiteral(" · ")));
            if (record.identity == preferred) selectedRow = networks_->count() - 1;
            qt_net_release(&record);
        }
        if (networks_->count())
            networks_->setCurrentRow(selectedRow >= 0 ? selectedRow : 0);
    }
    void runEditor(NetworkEditor *editor)
    {
        const QPointer<NetworkManager> guard(this); QPointer<NetworkEditor> draft(editor);
        const int result = draft->exec();
        if (guard && draft) refresh(result == QDialog::Accepted ? draft->savedNetwork() : nullptr);
        if (draft) delete draft;
    }
    void editSelected()
    {
        if (!networks_->currentItem()) return;
        QtNetwork record{};
        if (!qt_net_read(networks_->currentRow(), &record)) return;
        auto *editor = new NetworkEditor(this, &record);
        qt_net_release(&record);
        runEditor(editor);
    }
    void removeSelected()
    {
        void *identity = selectedIdentity();
        if (!identity) return;
        const QString name = networks_->currentItem()->text();
        const QPointer<NetworkManager> guard(this);
        if (QMessageBox::question(this, QStringLiteral("Remove network"),
                QStringLiteral("Remove saved network \"%1\"?").arg(name)) != QMessageBox::Yes)
            return;
        if (!guard) return;
        if (!qt_net_delete(identity))
            QMessageBox::warning(this, QStringLiteral("Unable to save"),
                                 QStringLiteral("Network removed from memory, but servlist.conf "
                                                "could not be written. Check directory permissions."));
        if (guard) refresh(nullptr);
    }
    void connectSelected()
    {
        void *identity = selectedIdentity();
        if (!identity) return;
        const QPointer<NetworkManager> guard(this);
        const bool connected = qt_net_connect(identity);
        if (!guard) return;
        if (!connected) {
            QMessageBox::warning(this, QStringLiteral("Connect"),
                                 QStringLiteral("Select a network with at least one server."));
            return;
        }
        accept();
    }
    QListWidget *networks_ = nullptr;
};
} // namespace

void showNetworkManager(QWidget *parent)
{
    static QPointer<NetworkManager> active;
    if (active) { if (active->isMinimized()) active->showNormal(); else active->show(); active->raise(); active->activateWindow(); return; }
    auto *dialog = new NetworkManager(parent); active = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(dialog, &QDialog::finished, dialog, [dialog]() { if (active == dialog) active.clear(); });
    dialog->show();
}
