/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "irctools.h"
#include "tools-bridge.h"
#include "ircformat.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <climits>
#include <functional>
#include <memory>

namespace {
QPointer<QWidget> toolsParent;
struct PromptCompletion {
    bool completed = false;
    bool claim() { if (completed) return false; completed = true; return true; }
};
std::shared_ptr<PromptCompletion> promptCompletion(QObject *dialog, std::function<void()> cancel)
{
    auto state = std::make_shared<PromptCompletion>();
    QObject::connect(dialog, &QObject::destroyed, [state, cancel]() {
        if (state->claim()) QTimer::singleShot(0, qApp, cancel);
    });
    return state;
}
QString cleanText(const char *text)
{
    QString result;
    for (const auto &run : parseIrcText(QString::fromUtf8(text ? text : ""), QPalette())) result += run.text;
    return result;
}
QString serverName(void *server)
{
    const char *name = nullptr;
    int connected = 0;
    return qt_core_server_info(server, &name, &connected) ? QString::fromUtf8(name ? name : "") : QString();
}
void saveText(QWidget *parent, const QByteArray &text, const QString &defaultName)
{
    const QPointer<QWidget> guard(parent);
    const QString file = QFileDialog::getSaveFileName(parent, QStringLiteral("Save Text"), defaultName);
    if (!guard || file.isEmpty()) return;
    QSaveFile output(file);
    if (!output.open(QIODevice::WriteOnly) || output.write(text) != text.size() || !output.commit())
        QMessageBox::warning(parent, QStringLiteral("Save Text"), output.errorString());
}
class ChannelFilter final : public QSortFilterProxyModel {
public:
    int minimum = 0;
    explicit ChannelFilter(QObject *parent) : QSortFilterProxyModel(parent) {}
    void setMinimum(int value)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange(); minimum = value; endFilterChange(Direction::Rows);
#else
        minimum = value; invalidateFilter();
#endif
    }
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        return sourceModel()->index(row, 1, parent).data().toInt() >= minimum &&
               QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }
};
class ChannelBrowser final : public QDialog {
public:
    explicit ChannelBrowser(void *server) : QDialog(toolsParent), server_(server)
    {
        setObjectName(QStringLiteral("channelBrowser"));
        setWindowTitle(QStringLiteral("Channel List — ") + serverName(server));
        resize(800, 500);
        auto *layout = new QVBoxLayout(this);
        serverFilter_ = new QLineEdit(this);
        serverFilter_->setObjectName(QStringLiteral("serverChannelFilter"));
        serverFilter_->setPlaceholderText(QStringLiteral("Server LIST filter (optional, e.g. #linux*)"));
        layout->addWidget(serverFilter_);
        auto *filters = new QHBoxLayout;
        search_ = new QLineEdit(this);
        search_->setObjectName(QStringLiteral("channelFilter"));
        search_->setPlaceholderText(QStringLiteral("Filter channel names or topics"));
        auto *minimum = new QSpinBox(this);
        minimum->setObjectName(QStringLiteral("minimumUsers"));
        minimum->setRange(0, INT_MAX);
        filters->addWidget(search_, 1);
        filters->addWidget(new QLabel(QStringLiteral("Minimum users:"), this));
        filters->addWidget(minimum);
        layout->addLayout(filters);
        model_ = new QStandardItemModel(this);
        model_->setHorizontalHeaderLabels({QStringLiteral("Channel"), QStringLiteral("Users"), QStringLiteral("Topic")});
        proxy_ = new ChannelFilter(this);
        proxy_->setSourceModel(model_);
        proxy_->setFilterKeyColumn(-1);
        proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
        table_ = new QTableView(this);
        table_->setObjectName(QStringLiteral("channelResults"));
        table_->setModel(proxy_);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->horizontalHeader()->setStretchLastSection(true);
        table_->setSortingEnabled(true);
        table_->sortByColumn(1, Qt::DescendingOrder);
        layout->addWidget(table_, 1);
        status_ = new QLabel(this);
        status_->setObjectName(QStringLiteral("channelStatus"));
        layout->addWidget(status_);
        auto *buttons = new QHBoxLayout;
        refresh_ = new QPushButton(QStringLiteral("Refresh"), this);
        refresh_->setObjectName(QStringLiteral("refreshChannels"));
        auto *join = new QPushButton(QStringLiteral("Join Selected"), this);
        join->setObjectName(QStringLiteral("joinSelectedChannel"));
        auto *close = new QPushButton(QStringLiteral("Close"), this);
        buttons->addWidget(refresh_); buttons->addWidget(join); buttons->addStretch(); buttons->addWidget(close);
        layout->addLayout(buttons);
        connect(search_, &QLineEdit::textChanged, this, [this](const QString &text) {
            proxy_->setFilterFixedString(text); updateCount();
        });
        connect(minimum, &QSpinBox::valueChanged, this, [this](int value) { proxy_->setMinimum(value); updateCount(); });
        connect(refresh_, &QPushButton::clicked, this, [this]() {
            const QByteArray filter = serverFilter_->text().toUtf8(); request(filter.constData());
        });
        connect(join, &QPushButton::clicked, this, [this]() { joinSelected(); });
        connect(table_, &QTableView::doubleClicked, this, [this]() { joinSelected(); });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            const char *name = nullptr; int connected = 0;
            if (!qt_core_server_info(server_, &name, &connected) || !connected) {
                listing_ = false; refresh_->setEnabled(true);
                status_->setText(QStringLiteral("Disconnected — reconnect before refreshing or joining"));
            }
        });
        timer->start(1000);
    }
    void request(const char *filter)
    {
        if (listing_) return;
        const char *name = nullptr; int connected = 0;
        if (!qt_core_server_info(server_, &name, &connected) || !connected) {
            status_->setText(QStringLiteral("Not connected")); return;
        }
        serverFilter_->setText(QString::fromUtf8(filter ? filter : ""));
        model_->removeRows(0, model_->rowCount());
        rows_.clear(); dropped_ = 0;
        listing_ = true; refresh_->setEnabled(false);
        proxy_->setDynamicSortFilter(false);
        if (!qt_core_request_channels(server_, filter ? filter : "")) end();
        updateCount();
    }
    void add(const char *channel, const char *users, const char *topic)
    {
        const QString name = QString::fromUtf8(channel ? channel : "");
        if (name.isEmpty()) return;
        int row = rows_.value(name, -1);
        if (row < 0) {
            if (model_->rowCount() >= 50000) { ++dropped_; return; }
            row = model_->rowCount(); rows_.insert(name, row);
            model_->appendRow({new QStandardItem(name), new QStandardItem, new QStandardItem});
        }
        model_->item(row, 1)->setData(qMax(0, QString::fromUtf8(users ? users : "0").toInt()), Qt::DisplayRole);
        model_->item(row, 2)->setText(cleanText(topic));
        // Avoid repainting counters for every line of a large LIST response.
        if (row % 100 == 0) updateCount();
    }
    void end()
    {
        listing_ = false; refresh_->setEnabled(true);
        proxy_->setDynamicSortFilter(true); proxy_->invalidate(); updateCount();
    }
private:
    void updateCount()
    {
        status_->setText(QStringLiteral("%1 shown / %2 received%3%4").arg(proxy_->rowCount()).arg(model_->rowCount())
            .arg(listing_ ? QStringLiteral(" — receiving…") : QString())
            .arg(dropped_ ? QStringLiteral(" — limit reached; refine the server-side /LIST filter") : QString()));
    }
    void joinSelected()
    {
        const QModelIndex current = table_->currentIndex();
        if (!current.isValid()) return;
        const QByteArray name = proxy_->index(current.row(), 0).data().toString().toUtf8();
        if (!qt_core_join_channel(server_, name.constData())) status_->setText(QStringLiteral("Unable to join — check connection"));
    }
    void *server_;
    QStandardItemModel *model_;
    ChannelFilter *proxy_;
    QTableView *table_;
    QLineEdit *search_;
    QLineEdit *serverFilter_;
    QPushButton *refresh_;
    QLabel *status_;
    QHash<QString, int> rows_;
    bool listing_ = false;
    int dropped_ = 0;
};
class RawLog final : public QDialog {
public:
    explicit RawLog(void *server) : QDialog(toolsParent)
    {
        setObjectName(QStringLiteral("rawLog"));
        setWindowTitle(QStringLiteral("Raw Log — ") + serverName(server));
        resize(850, 500);
        auto *layout = new QVBoxLayout(this);
        text_ = new QPlainTextEdit(this);
        text_->setObjectName(QStringLiteral("rawLogText"));
        text_->setReadOnly(true);
        text_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        text_->setMaximumBlockCount(5000);
        layout->addWidget(text_, 1);
        auto *row = new QHBoxLayout;
        pause_ = new QCheckBox(QStringLiteral("Pause capture"), this);
        pause_->setObjectName(QStringLiteral("pauseRawLog"));
        auto *clear = new QPushButton(QStringLiteral("Clear"), this);
        clear->setObjectName(QStringLiteral("clearRawLog"));
        auto *save = new QPushButton(QStringLiteral("Save As…"), this);
        auto *close = new QPushButton(QStringLiteral("Close"), this);
        row->addWidget(pause_); row->addStretch(); row->addWidget(clear); row->addWidget(save); row->addWidget(close);
        layout->addLayout(row);
        connect(clear, &QPushButton::clicked, text_, &QPlainTextEdit::clear);
        connect(save, &QPushButton::clicked, this, [this]() { saveText(this, text_->toPlainText().toUtf8(), QStringLiteral("irc-log.txt")); });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
    }
    void append(const char *text, int length, bool outbound)
    {
        if (pause_->isChecked() || !text || length <= 0) return;
        const int scroll = text_->verticalScrollBar()->value();
        const bool bottom = scroll == text_->verticalScrollBar()->maximum();
        const auto lines = QString::fromUtf8(text, length).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (QString line : lines) {
            if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
            if (!line.isEmpty()) text_->appendPlainText((outbound ? QStringLiteral(">> ") : QStringLiteral("<< ")) + redactedIrcLine(line));
        }
        if (!bottom) text_->verticalScrollBar()->setValue(scroll);
    }
private:
    QPlainTextEdit *text_;
    QCheckBox *pause_;
};
QHash<void *, QPointer<ChannelBrowser>> browsers;
QHash<void *, QPointer<RawLog>> logs;

class IgnoreManager final : public QDialog {
public:
    IgnoreManager() : QDialog(toolsParent)
    {
        setObjectName(QStringLiteral("ignoreManager"));
        setWindowTitle(QStringLiteral("Ignore List")); resize(800, 400);
        auto *layout = new QVBoxLayout(this);
        model_ = new QStandardItemModel(this);
        model_->setHorizontalHeaderLabels({QStringLiteral("Mask"), QStringLiteral("Rules")});
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("ignoreRules"));
        table_->setModel(model_); table_->horizontalHeader()->setStretchLastSection(true);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        layout->addWidget(table_);
        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton(QStringLiteral("Add…"), this); add->setObjectName(QStringLiteral("addIgnore"));
        auto *edit = new QPushButton(QStringLiteral("Edit…"), this); edit->setObjectName(QStringLiteral("editIgnore"));
        auto *remove = new QPushButton(QStringLiteral("Remove"), this); remove->setObjectName(QStringLiteral("removeIgnore"));
        auto *close = new QPushButton(QStringLiteral("Close"), this);
        buttons->addWidget(add); buttons->addWidget(edit); buttons->addWidget(remove); buttons->addStretch(); buttons->addWidget(close);
        layout->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, [this]() { editRule(false); });
        connect(edit, &QPushButton::clicked, this, [this]() { editRule(true); });
        connect(table_, &QTableView::doubleClicked, this, [this]() { editRule(true); });
        connect(remove, &QPushButton::clicked, this, [this]() {
            const QModelIndex index = table_->currentIndex();
            if (index.isValid()) qt_core_ignore_remove(model_->item(index.row(), 0)->text().toUtf8().constData());
        });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        reload();
    }
    void reload()
    {
        const QString selected = table_->currentIndex().isValid() ? model_->item(table_->currentIndex().row(), 0)->text() : QString();
        model_->removeRows(0, model_->rowCount());
        qt_core_ignore_foreach([](const char *mask, int flags, void *data) {
            auto *self = static_cast<IgnoreManager *>(data);
            QStringList names;
            for (int i = 0; i < 8; ++i) if (flags & (1 << i)) names.append(flagNames()[i]);
            auto *item = new QStandardItem(QString::fromUtf8(mask)); item->setData(flags, Qt::UserRole);
            self->model_->appendRow({item, new QStandardItem(names.join(QStringLiteral(", ")))});
        }, this);
        for (int row = 0; row < model_->rowCount(); ++row)
            if (model_->item(row, 0)->text() == selected) table_->selectRow(row);
        table_->resizeColumnToContents(0);
    }
private:
    static QStringList flagNames()
    {
        return {QStringLiteral("Private messages"), QStringLiteral("Notices"), QStringLiteral("Channel messages"),
                QStringLiteral("CTCP"), QStringLiteral("Invites"), QStringLiteral("Exception"),
                QStringLiteral("Temporary (not saved)"), QStringLiteral("DCC")};
    }
    void editRule(bool editing)
    {
        const QModelIndex current = table_->currentIndex();
        if (editing && !current.isValid()) return;
        const QString oldMask = editing ? model_->item(current.row(), 0)->text() : QString();
        const int oldFlags = editing ? model_->item(current.row(), 0)->data(Qt::UserRole).toInt() : 159;
        QDialog dialog(this); dialog.setObjectName(QStringLiteral("ignoreEditor"));
        dialog.setWindowTitle(editing ? QStringLiteral("Edit Ignore") : QStringLiteral("Add Ignore"));
        auto *form = new QFormLayout(&dialog);
        auto *mask = new QLineEdit(oldMask, &dialog); mask->setObjectName(QStringLiteral("ignoreMask"));
        mask->setPlaceholderText(QStringLiteral("nick!*@* or *!*@hostname"));
        form->addRow(QStringLiteral("Mask:"), mask);
        QList<QCheckBox *> checks;
        for (int i = 0; i < 8; ++i) {
            auto *check = new QCheckBox(flagNames()[i], &dialog);
            check->setObjectName(QStringLiteral("ignoreFlag%1").arg(i));
            check->setChecked(oldFlags & (1 << i)); checks.append(check); form->addRow(check);
        }
        auto *error = new QLabel(&dialog); error->setObjectName(QStringLiteral("ignoreValidation"));
        error->setTextFormat(Qt::PlainText); form->addRow(error);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
            int flags = 0;
            for (int i = 0; i < checks.size(); ++i) if (checks[i]->isChecked()) flags |= 1 << i;
            const QByteArray old = oldMask.toUtf8(), value = mask->text().trimmed().toUtf8();
            if (!qt_core_ignore_set(old.constData(), value.constData(), flags)) {
                error->setText(QStringLiteral("Use a unique, nonempty mask without spaces and choose a rule type. The original rule must still exist.")); return;
            }
            dialog.accept();
        });
        dialog.exec();
    }
    QStandardItemModel *model_;
    QTableView *table_;
};
QPointer<IgnoreManager> ignores;
}

QString redactedIrcLine(const QString &line)
{
    static const QRegularExpression authentication(QStringLiteral(
        "^((?:@[^ ]+ )?(?::[^ ]+ )?)(PASS|AUTHENTICATE|OPER|WEBIRC)(?: .*)?$"), QRegularExpression::CaseInsensitiveOption);
    const auto match = authentication.match(line);
    if (match.hasMatch()) return match.captured(1) + match.captured(2) + QStringLiteral(" [redacted]");
    static const QRegularExpression nickserv(QStringLiteral(
        "^((?:@[^ ]+ )?(?::[^ ]+ )?(?:PRIVMSG|NOTICE) NickServ(?:@[^ ]+)? :)"
        "(?:IDENTIFY|REGISTER|GHOST|RECOVER|RELEASE|SET PASSWORD)\\b.*$"), QRegularExpression::CaseInsensitiveOption);
    const auto service = nickserv.match(line);
    return service.hasMatch() ? service.captured(1) + QStringLiteral("[redacted]") : line;
}
void initializeIrcTools(QWidget *parent) { toolsParent = parent; }
void showRawLog(void *server)
{
    if (!server) return;
    if (!logs.value(server)) logs[server] = new RawLog(server);
    logs[server]->show(); logs[server]->raise(); logs[server]->activateWindow();
}
void showIgnoreManager()
{
    if (!ignores) ignores = new IgnoreManager;
    ignores->reload(); ignores->show(); ignores->raise(); ignores->activateWindow();
}
extern "C" {
void qt_fe_channel_browser(void *server, const char *filter, int refresh)
{
    if (!server) return;
    const bool created = !browsers.value(server);
    if (created) browsers[server] = new ChannelBrowser(server);
    auto *browser = browsers.value(server).data();
    browser->show(); browser->raise(); browser->activateWindow();
    if (created || refresh) browser->request(filter);
}
int qt_fe_channel_browser_open(void *server) { return browsers.value(server) != nullptr; }
void qt_fe_channel_row(void *server, const char *channel, const char *users, const char *topic)
{ if (browsers.value(server)) browsers[server]->add(channel, users, topic); }
void qt_fe_channel_end(void *server) { if (browsers.value(server)) browsers[server]->end(); }
void qt_fe_rawlog(void *server, const char *text, int length, int outbound)
{ if (logs.value(server)) logs[server]->append(text, length, outbound != 0); }
void qt_fe_tools_server_closed(void *server)
{
    // Hide immediately; defer deletion so callbacks and file dialogs can unwind.
    if (auto *dialog = browsers.take(server).data()) { dialog->hide(); dialog->deleteLater(); }
    if (auto *dialog = logs.take(server).data()) { dialog->hide(); dialog->deleteLater(); }
}
void qt_fe_ignore_changed(void) { if (ignores) ignores->reload(); }
void qt_fe_prompt_bool(const char *title, const char *prompt, void (*callback)(int, void *), void *data)
{
    auto *dialog = new QMessageBox(QMessageBox::Question, QString::fromUtf8(title ? title : "Question"),
        QString::fromUtf8(prompt ? prompt : ""), QMessageBox::Yes | QMessageBox::No, toolsParent);
    dialog->setTextFormat(Qt::PlainText); dialog->setAttribute(Qt::WA_DeleteOnClose);
    const auto completion = promptCompletion(dialog, [callback, data]() { if (callback) callback(0, data); });
    QObject::connect(dialog, &QDialog::finished, dialog, [completion, callback, data](int result) {
        if (completion->claim() && callback) QTimer::singleShot(0, qApp, [callback, data, result]() { callback(result == QMessageBox::Yes, data); });
    });
    dialog->open();
}
static void promptString(const char *prompt, const char *initial, void (*callback)(int, char *, void *), void *data, bool password)
{
    auto *dialog = new QInputDialog(toolsParent); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("ZoiteChat")); dialog->setLabelText(QString::fromUtf8(prompt ? prompt : ""));
    dialog->setTextValue(QString::fromUtf8(initial ? initial : ""));
    if (password) dialog->setTextEchoMode(QLineEdit::Password);
    const auto value = std::make_shared<QString>(dialog->textValue());
    QObject::connect(dialog, &QInputDialog::textValueChanged, dialog, [value](const QString &text) { *value = text; });
    const auto completion = promptCompletion(dialog, [value, callback, data]() {
        QByteArray text = value->toUtf8(); if (callback) callback(1, text.data(), data);
    });
    QObject::connect(dialog, &QDialog::finished, dialog, [dialog, completion, callback, data](int result) {
        if (!completion->claim()) return;
        QByteArray text = dialog->textValue().toUtf8();
        if (callback) QTimer::singleShot(0, qApp, [callback, data, result, text]() mutable { callback(result != QDialog::Accepted, text.data(), data); });
    });
    dialog->open();
}
void qt_fe_prompt_integer(const char *prompt, int initial, void (*callback)(int, int, void *), void *data)
{
    auto *dialog = new QInputDialog(toolsParent); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("ZoiteChat")); dialog->setLabelText(QString::fromUtf8(prompt ? prompt : ""));
    dialog->setInputMode(QInputDialog::IntInput); dialog->setIntRange(INT_MIN, INT_MAX); dialog->setIntValue(initial);
    const auto value = std::make_shared<int>(initial);
    QObject::connect(dialog, &QInputDialog::intValueChanged, dialog, [value](int number) { *value = number; });
    const auto completion = promptCompletion(dialog, [value, callback, data]() { if (callback) callback(1, *value, data); });
    QObject::connect(dialog, &QDialog::finished, dialog, [dialog, completion, callback, data](int result) {
        if (!completion->claim()) return;
        const int value = dialog->intValue();
        if (callback) QTimer::singleShot(0, qApp, [callback, data, result, value]() { callback(result != QDialog::Accepted, value, data); });
    });
    dialog->open();
}
void qt_fe_prompt_file(const char *title, const char *initial, int flags, void (*callback)(void *, char *), void *data)
{
    auto *dialog = new QFileDialog(toolsParent, QString::fromUtf8(title ? title : "Choose File"),
                                   QString::fromUtf8(initial ? initial : ""));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (flags & 8) {
        dialog->setFileMode(QFileDialog::Directory);
        dialog->setOption(QFileDialog::ShowDirsOnly);
    } else if (flags & 1) {
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setOption(QFileDialog::DontConfirmOverwrite, flags & 32);
    } else {
        dialog->setFileMode(flags & 2 ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
    }
    const auto completion = promptCompletion(dialog, [callback, data]() { if (callback) callback(data, nullptr); });
    QObject::connect(dialog, &QDialog::finished, dialog, [dialog, completion, callback, data](int result) {
        if (!completion->claim() || !callback) return;
        const QStringList files = result == QDialog::Accepted ? dialog->selectedFiles() : QStringList();
        // Leave QFileDialog::done() before core callbacks can destroy the parent.
        QTimer::singleShot(0, qApp, [files, callback, data]() {
            for (const QString &file : files) {
                QByteArray path = file.toUtf8(); callback(data, path.data());
            }
            callback(data, nullptr); // Core file callbacks release state at this sentinel.
        });
    });
    dialog->open();
}
}

void qt_fe_prompt_string(const char *prompt, const char *initial, void (*callback)(int, char *, void *), void *data)
{ promptString(prompt, initial, callback, data, false); }
void qt_fe_prompt_password(const char *prompt, void (*callback)(int, char *, void *), void *data)
{ promptString(prompt, "", callback, data, true); }
