/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "textevents.h"
#include "textevents-bridge.h"
#include "ircformat.h"
#include <QDialog>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextBrowser>
#include <QVBoxLayout>
namespace {
struct Event { int id; QString name, text, defaults; QStringList help; };
class EventEditor final : public QDialog {
public:
    explicit EventEditor(QWidget *parent) : QDialog(parent)
    {
        setObjectName(QStringLiteral("textEventsEditor")); setWindowTitle(QStringLiteral("Text Events"));
        setAttribute(Qt::WA_DeleteOnClose); resize(900, 650);
        auto *layout = new QVBoxLayout(this);
        auto *filter = new QLineEdit(this); filter->setPlaceholderText(QStringLiteral("Filter event names"));
        filter->setObjectName(QStringLiteral("eventFilter")); layout->addWidget(filter);
        model_ = new QStandardItemModel(this); model_->setHorizontalHeaderLabels({QStringLiteral("Event"), QStringLiteral("Format")});
        proxy_ = new QSortFilterProxyModel(this); proxy_->setSourceModel(model_); proxy_->setFilterKeyColumn(0); proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("eventRows")); table_->setModel(proxy_);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(table_, 1);
        format_ = new QLineEdit(this); format_->setObjectName(QStringLiteral("eventFormat")); layout->addWidget(format_);
        auto *syntax = new QLabel(QStringLiteral("$1…$9: event arguments • $t: indentation • $a036: literal $ • %B/%C/%O: IRC formatting. Empty formats suppress an event. Maximum 1,000 UTF-8 bytes."), this);
        syntax->setWordWrap(true); syntax->setTextFormat(Qt::PlainText); layout->addWidget(syntax);
        help_ = new QLabel(this); help_->setObjectName(QStringLiteral("eventHelp")); help_->setTextFormat(Qt::PlainText); help_->setWordWrap(true); layout->addWidget(help_);
        preview_ = new QTextBrowser(this); preview_->setObjectName(QStringLiteral("eventPreview")); preview_->setOpenLinks(false); preview_->setPalette(ircDisplayPalette(preview_->palette())); preview_->setMaximumHeight(110); layout->addWidget(preview_);
        error_ = new QLabel(this); error_->setObjectName(QStringLiteral("eventError")); error_->setTextFormat(Qt::PlainText); error_->setWordWrap(true); layout->addWidget(error_);
        auto *actions = new QHBoxLayout;
        auto button = [&](const QString &label, const QString &id) { auto *b = new QPushButton(label, this); b->setObjectName(id); actions->addWidget(b); return b; };
        auto *preview = button(QStringLiteral("Preview"), QStringLiteral("previewEvent"));
        auto *defaults = button(QStringLiteral("Default for Selected"), QStringLiteral("defaultEvent"));
        auto *all = button(QStringLiteral("Defaults for All"), QStringLiteral("defaultAllEvents"));
        auto *import = button(QStringLiteral("Import…"), QStringLiteral("importEvents"));
        auto *exportButton = button(QStringLiteral("Export…"), QStringLiteral("exportEvents"));
        auto *reload = button(QStringLiteral("Reload"), QStringLiteral("reloadEvents")); layout->addLayout(actions);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this); layout->addWidget(buttons);
        connect(filter, &QLineEdit::textChanged, proxy_, &QSortFilterProxyModel::setFilterFixedString);
        connect(table_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { select(); });
        connect(format_, &QLineEdit::textChanged, this, [this](const QString &text) {
            if (selected_ < 0) return;
            rows_[selected_].text = text; model_->item(selected_, 1)->setText(text); preview_->clear(); error_->clear();
        });
        connect(preview, &QPushButton::clicked, this, [this]() { showPreview(); });
        connect(defaults, &QPushButton::clicked, this, [this]() { if (selected_ >= 0) format_->setText(rows_[selected_].defaults); });
        connect(all, &QPushButton::clicked, this, [this]() { for (auto &row : rows_) row.text = row.defaults; render(); });
        connect(reload, &QPushButton::clicked, this, [this]() { load(); });
        connect(import, &QPushButton::clicked, this, [this]() { importFile(); });
        connect(exportButton, &QPushButton::clicked, this, [this]() { exportFile(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            QList<QByteArray> bytes; for (const auto &row : rows_) bytes.append(row.text.toUtf8());
            QList<const char *> texts; for (const auto &text : bytes) texts.append(text.constData());
            char error[1024] = {};
            if (!qt_core_text_events_save(texts.constData(), texts.size(), revision_.constData(), error, sizeof(error))) { error_->setText(QString::fromUtf8(error)); return; }
            accept();
        });
        load();
    }
private:
    void load()
    {
        rows_.clear(); char revision[65] = {};
        if (!qt_core_text_events([](const QtTextEvent *event, void *data) {
            Event row{event->id, QString::fromUtf8(event->name), QString::fromUtf8(event->text), QString::fromUtf8(event->defaults), {}};
            for (int i = 0; i < event->arguments && i < 9; ++i) row.help.append(QStringLiteral("$%1: %2").arg(i + 1).arg(QString::fromUtf8(event->help[i])));
            static_cast<QList<Event> *>(data)->append(row);
        }, &rows_, revision, sizeof(revision))) { error_->setText(QStringLiteral("Could not load text events.")); return; }
        revision_ = revision; error_->clear(); render();
    }
    void render()
    {
        selected_ = -1; model_->removeRows(0, model_->rowCount());
        for (const auto &row : rows_) model_->appendRow({new QStandardItem(row.name), new QStandardItem(row.text)});
        if (proxy_->rowCount()) { table_->setCurrentIndex(proxy_->index(0, 0)); table_->selectRow(0); }
        else select();
    }
    void select()
    {
        const auto index = proxy_->mapToSource(table_->currentIndex()); selected_ = index.isValid() ? index.row() : -1;
        QSignalBlocker blocker(format_); format_->setEnabled(selected_ >= 0);
        format_->setText(selected_ >= 0 ? rows_[selected_].text : QString());
        help_->setText(selected_ >= 0 ? rows_[selected_].help.join(QLatin1Char('\n')) : QString());
        preview_->clear(); error_->clear();
    }
    void showPreview()
    {
        if (selected_ < 0) return;
        preview_->setPalette(ircDisplayPalette(QApplication::palette(preview_)));
        char output[8192] = {}, error[1024] = {}; const auto &row = rows_[selected_];
        if (!qt_core_text_event_preview(row.id, row.text.toUtf8().constData(), output, sizeof(output), error, sizeof(error))) { error_->setText(QString::fromUtf8(error)); return; }
        error_->clear(); preview_->clear(); appendIrcMessage(preview_->document(), QString::fromUtf8(output), 0, preview_->palette(), false);
    }
    void importFile()
    {
        QPointer<EventEditor> guard(this);
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Text Events"), {}, QStringLiteral("Text events (*.conf);;All files (*)"));
        if (!guard || path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) { error_->setText(QStringLiteral("Could not read file, or file exceeds 4 MiB.")); return; }
        const QByteArray data = file.readAll();
        if (data.contains('\0') || QString::fromUtf8(data).toUtf8() != data) { error_->setText(QStringLiteral("The file must contain valid UTF-8 text without NUL bytes.")); return; }
        QString name; QMap<QString, QString> imported;
        for (const auto &lineBytes : data.split('\n')) {
            QByteArray line = lineBytes; if (line.endsWith('\r')) line.chop(1);
            if (line.startsWith("event_name=")) name = QString::fromUtf8(line.mid(11));
            else if (line.startsWith("event_text=") && !name.isEmpty()) { imported[name] = QString::fromUtf8(line.mid(11)); name.clear(); }
        }
        int matched = 0;
        for (auto &row : rows_) if (imported.contains(row.name)) { row.text = imported.value(row.name); ++matched; }
        render(); error_->setText(QStringLiteral("Imported %1 matching events into the draft. Save to apply.").arg(matched));
    }
    void exportFile()
    {
        QByteArray data; for (const auto &row : rows_) data += "event_name=" + row.name.toUtf8() + "\nevent_text=" + row.text.toUtf8() + "\n\n";
        QPointer<EventEditor> guard(this);
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Text Events"), QStringLiteral("pevents.conf"));
        if (!guard || path.isEmpty()) return;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) error_->setText(file.errorString());
        else error_->setText(QStringLiteral("Draft exported."));
    }
    QList<Event> rows_; QByteArray revision_; int selected_ = -1;
    QStandardItemModel *model_; QSortFilterProxyModel *proxy_; QTableView *table_;
    QLineEdit *format_; QLabel *help_, *error_; QTextBrowser *preview_;
};
QPointer<EventEditor> editor;
}
void showTextEvents(QWidget *parent)
{
    if (!editor) editor = new EventEditor(parent);
    editor->show(); editor->raise(); editor->activateWindow();
}

namespace {
class SoundEditor final : public QDialog {
public:
    explicit SoundEditor(QWidget *parent) : QDialog(parent)
    {
        setObjectName(QStringLiteral("soundSettings")); setWindowTitle(QStringLiteral("Event Sounds"));
        setAttribute(Qt::WA_DeleteOnClose); resize(850, 500);
        auto *layout = new QVBoxLayout(this);
        auto *filter = new QLineEdit(this); filter->setObjectName(QStringLiteral("soundFilter")); filter->setPlaceholderText(QStringLiteral("Filter event names")); layout->addWidget(filter);
        auto *help = new QLabel(QStringLiteral("Assign a sound file to an event, or leave it empty for no custom sound. Relative paths use the sounds directory inside the ZoiteChat configuration directory. Playback uses the core's platform sound backend."), this);
        help->setWordWrap(true); help->setTextFormat(Qt::PlainText); layout->addWidget(help);
        model_ = new QStandardItemModel(this); model_->setHorizontalHeaderLabels({QStringLiteral("Event"), QStringLiteral("Sound file")});
        proxy_ = new QSortFilterProxyModel(this); proxy_->setSourceModel(model_); proxy_->setFilterKeyColumn(0); proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("soundRows")); table_->setModel(proxy_);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_, 1);
        auto *row = new QHBoxLayout;
        auto button = [&](const QString &label, const QString &id) { auto *b = new QPushButton(label, this); b->setObjectName(id); row->addWidget(b); return b; };
        auto *browse = button(QStringLiteral("Browse…"), QStringLiteral("browseSound"));
        auto *play = button(QStringLiteral("Play Selected"), QStringLiteral("playSound"));
        auto *clear = button(QStringLiteral("Clear Selected"), QStringLiteral("clearSound"));
        auto *reload = button(QStringLiteral("Reload"), QStringLiteral("reloadSounds")); layout->addLayout(row);
        error_ = new QLabel(this); error_->setObjectName(QStringLiteral("soundError")); error_->setTextFormat(Qt::PlainText); error_->setWordWrap(true); layout->addWidget(error_);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this); layout->addWidget(buttons);
        connect(filter, &QLineEdit::textChanged, proxy_, &QSortFilterProxyModel::setFilterFixedString);
        connect(browse, &QPushButton::clicked, this, [this]() {
            const auto index = selected(); if (!index.isValid()) return;
            const QPersistentModelIndex target(index); QPointer<SoundEditor> guard(this);
            const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Choose Event Sound"), index.data().toString(), QStringLiteral("Sound files (*.wav *.ogg *.flac *.mp3);;All files (*)"));
            if (guard && target.isValid() && !path.isEmpty()) model_->setData(target, path);
        });
        connect(play, &QPushButton::clicked, this, [this]() {
            const auto index = selected(); if (!index.isValid()) return;
            char error[1024] = {};
            if (!qt_core_sound_preview(index.data().toString().toUtf8().constData(), error, sizeof(error))) error_->setText(QString::fromUtf8(error));
            else error_->setText(QStringLiteral("Playback requested. Audio availability depends on the core's sound backend."));
        });
        connect(clear, &QPushButton::clicked, this, [this]() { const auto index = selected(); if (index.isValid()) model_->setData(index, QString()); });
        connect(reload, &QPushButton::clicked, this, [this]() { load(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            table_->setFocus();
            QList<QByteArray> bytes; for (int i = 0; i < model_->rowCount(); ++i) bytes.append(model_->item(i, 1)->text().toUtf8());
            QList<const char *> files; for (const auto &file : bytes) files.append(file.constData());
            char error[1024] = {};
            if (!qt_core_sounds_save(files.constData(), files.size(), revision_.constData(), error, sizeof(error))) { error_->setText(QString::fromUtf8(error)); return; }
            accept();
        });
        load();
    }
private:
    QModelIndex selected() const {
        const auto index = proxy_->mapToSource(table_->currentIndex()); return index.isValid() ? model_->index(index.row(), 1) : QModelIndex();
    }
    void load()
    {
        model_->removeRows(0, model_->rowCount()); char revision[65] = {};
        if (!qt_core_sounds_read([](int, const char *name, const char *file, void *data) {
            auto *model = static_cast<QStandardItemModel *>(data); auto *label = new QStandardItem(QString::fromUtf8(name)); label->setEditable(false);
            model->appendRow({label, new QStandardItem(QString::fromUtf8(file))});
        }, model_, revision, sizeof(revision))) { error_->setText(QStringLiteral("Could not load sound assignments.")); return; }
        revision_ = revision; error_->clear();
        if (proxy_->rowCount()) { table_->setCurrentIndex(proxy_->index(0, 0)); table_->selectRow(0); }
    }
    QStandardItemModel *model_; QSortFilterProxyModel *proxy_; QTableView *table_;
    QLabel *error_; QByteArray revision_;
};
QPointer<SoundEditor> soundEditor;
}
void showSoundSettings(QWidget *parent)
{
    if (!soundEditor) soundEditor = new SoundEditor(parent);
    soundEditor->show(); soundEditor->raise(); soundEditor->activateWindow();
}
