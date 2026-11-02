/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "addons.h"
#include "addons-bridge.h"
#include <QDialog>
#include <QFileDialog>
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
struct Addon { void *id; QString name, version, description, file; bool script, builtin; };
class AddonsDialog final : public QDialog {
public:
    AddonsDialog(QWidget *parent, std::function<AddonContext()> context) : QDialog(parent), context_(std::move(context))
    {
        setObjectName(QStringLiteral("addonsDialog")); setWindowTitle(QStringLiteral("Plugins and Scripts")); resize(900, 470); setAttribute(Qt::WA_DeleteOnClose);
        auto *layout = new QVBoxLayout(this);
        auto *filter = new QLineEdit(this); filter->setPlaceholderText(QStringLiteral("Filter addons")); layout->addWidget(filter);
        model_ = new QStandardItemModel(this); model_->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Version"), QStringLiteral("Type"), QStringLiteral("Filename"), QStringLiteral("Description")});
        proxy_ = new QSortFilterProxyModel(this); proxy_->setSourceModel(model_); proxy_->setFilterKeyColumn(-1); proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("addonRows")); table_->setModel(proxy_); table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSortingEnabled(true); table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_, 1);
        status_ = new QLabel(this); status_->setObjectName(QStringLiteral("addonStatus")); status_->setWordWrap(true); status_->setTextFormat(Qt::PlainText); layout->addWidget(status_);
        auto *buttons = new QHBoxLayout;
        auto button = [&](const QString &name, const QString &id) { auto *b = new QPushButton(name, this); b->setObjectName(id); buttons->addWidget(b); return b; };
        load_ = button(QStringLiteral("Load…"), QStringLiteral("loadAddon")); unload_ = button(QStringLiteral("Unload"), QStringLiteral("unloadAddon")); reload_ = button(QStringLiteral("Reload"), QStringLiteral("reloadAddon"));
        auto *refresh = button(QStringLiteral("Refresh"), QStringLiteral("refreshAddons")); auto *close = button(QStringLiteral("Close"), QStringLiteral("closeAddons")); layout->addLayout(buttons);
        connect(filter, &QLineEdit::textChanged, proxy_, &QSortFilterProxyModel::setFilterFixedString);
        connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { enable(); });
        connect(refresh, &QPushButton::clicked, this, [this]() { refreshRows(); });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        connect(load_, &QPushButton::clicked, this, [this]() {
            const auto context = context_(); if (!valid(context)) return;
            QPointer<AddonsDialog> guard(this);
            const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Load Plugin or Script"), {}, QStringLiteral("Plugins and scripts (*.so *.dylib *.dll *.py *.pl *.lua *.tcl *.js);;All files (*)"));
            if (guard && !file.isEmpty()) run(context, 0, {nullptr, {}, {}, {}, file, false, false});
        });
        connect(unload_, &QPushButton::clicked, this, [this]() { selectedAction(1); });
        connect(reload_, &QPushButton::clicked, this, [this]() { selectedAction(2); });
        auto *timer = new QTimer(this); connect(timer, &QTimer::timeout, this, [this]() { refreshRows(); }); timer->start(2000);
        if (!qt_core_addons_enabled()) status_->setText(QStringLiteral("Loadable plugin support is disabled in this build."));
        else status_->setText(QStringLiteral("Scripts require their matching language plugin. GTK-specific plugin windows are not supported by the Qt frontend."));
        refreshRows();
    }
    void setContext(std::function<AddonContext()> context) { context_ = std::move(context); enable(); }
    void schedule() { if (pending_) return; pending_ = true; QTimer::singleShot(0, this, [this]() { pending_ = false; refreshRows(); }); }
private:
    bool valid(const AddonContext &context) const { return context.session && context.alive && context.alive(); }
    int selected() const { const auto index = proxy_->mapToSource(table_->currentIndex()); return index.isValid() ? index.row() : -1; }
    void enable() {
        const bool enabled = !busy_ && qt_core_addons_enabled() && valid(context_()); load_->setEnabled(enabled);
        const int row = selected(); const bool can = enabled && row >= 0 && row < rows_.size() && !rows_[row].builtin;
        unload_->setEnabled(can); reload_->setEnabled(can);
    }
    void refreshRows()
    {
        void *identity = selected() >= 0 && selected() < rows_.size() ? rows_[selected()].id : nullptr;
        QList<Addon> rows;
        qt_core_addons_read([](const QtAddon *addon, void *data) {
            static_cast<QList<Addon> *>(data)->append({addon->identity, QString::fromUtf8(addon->name), QString::fromUtf8(addon->version), QString::fromUtf8(addon->description), QString::fromUtf8(addon->filename), bool(addon->script), bool(addon->builtin)});
        }, &rows);
        table_->setUpdatesEnabled(false); model_->removeRows(0, model_->rowCount()); rows_ = rows;
        for (int i = 0; i < rows.size(); ++i) {
            const auto &row = rows[i];
            model_->appendRow({new QStandardItem(row.name), new QStandardItem(row.version), new QStandardItem(row.builtin ? QStringLiteral("Built-in") : row.script ? QStringLiteral("Script") : QStringLiteral("Plugin")), new QStandardItem(row.file), new QStandardItem(row.description)});
            if (row.id == identity) { const auto index = proxy_->mapFromSource(model_->index(i, 0)); table_->setCurrentIndex(index); table_->selectRow(index.row()); }
        }
        table_->setUpdatesEnabled(true); enable();
    }
    void selectedAction(int action) { const int row = selected(); if (row >= 0 && row < rows_.size() && !rows_[row].builtin) run(context_(), action, rows_[row]); }
    void run(const AddonContext &context, int action, Addon row)
    {
        if (busy_) return;
        if (!valid(context)) { status_->setText(QStringLiteral("The original conversation has closed.")); return; }
        busy_ = true; enable();
        QPointer<AddonsDialog> guard(this); char error[1024] = {};
        const int result = qt_core_addon_action(context.session, action, row.id, row.name.toUtf8().constData(), row.file.toUtf8().constData(), error, sizeof(error));
        if (context.restore) context.restore();
        if (!guard) return;
        busy_ = false;
        status_->setText(result ? QStringLiteral("Request completed. Script-loader diagnostics appear in the conversation used for the request.") : QString::fromUtf8(error)); refreshRows();
    }
    bool busy_ = false, pending_ = false;
    std::function<AddonContext()> context_; QList<Addon> rows_; QStandardItemModel *model_; QSortFilterProxyModel *proxy_; QTableView *table_;
    QLabel *status_; QPushButton *load_, *unload_, *reload_;
};
QPointer<AddonsDialog> dialog;
}
void showAddons(QWidget *parent, std::function<AddonContext()> context, bool chooseFile)
{ if (!dialog) dialog = new AddonsDialog(parent, std::move(context)); else dialog->setContext(std::move(context)); dialog->show(); dialog->raise(); dialog->activateWindow();
  if (chooseFile) { auto *load = dialog->findChild<QPushButton *>(QStringLiteral("loadAddon")); if (load) load->click(); }
}
extern "C" void qt_fe_addons_changed(void) { if (dialog) dialog->schedule(); }
