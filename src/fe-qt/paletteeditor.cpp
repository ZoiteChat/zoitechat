/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "paletteeditor.h"
#include "ircformat.h"
#include <QApplication>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QSignalBlocker>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
namespace {
class PaletteEditor final : public QDialog {
public:
    PaletteEditor(const QString &path, QWidget *parent, std::function<void()> applied)
        : QDialog(parent), path_(path), applied_(std::move(applied))
    {
        setObjectName(QStringLiteral("paletteEditor")); setWindowTitle(QStringLiteral("Chat Colors"));
        setAttribute(Qt::WA_DeleteOnClose); resize(620, 650);
        auto *layout = new QVBoxLayout(this);
        auto *help = new QLabel(QStringLiteral("Edit #RRGGBB values or choose a color. Changes stay in this draft until Save. The native colors.conf file is updated without changing unrelated entries. Existing rendered text retains its explicit colors."), this);
        help->setTextFormat(Qt::PlainText); help->setWordWrap(true); layout->addWidget(help);
        model_ = new QStandardItemModel(this); model_->setHorizontalHeaderLabels({QStringLiteral("Color"), QStringLiteral("RGB")});
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("paletteRows")); table_->setModel(model_);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_, 1);
        auto *row = new QHBoxLayout;
        auto button = [&](const QString &label, const QString &id) { auto *b = new QPushButton(label, this); b->setObjectName(id); row->addWidget(b); return b; };
        auto *choose = button(QStringLiteral("Choose…"), QStringLiteral("choosePaletteColor"));
        auto *reset = button(QStringLiteral("Default for Selected"), QStringLiteral("defaultPaletteColor"));
        auto *all = button(QStringLiteral("Defaults for All"), QStringLiteral("defaultAllPaletteColors"));
        auto *reload = button(QStringLiteral("Reload"), QStringLiteral("reloadPalette")); layout->addLayout(row);
        error_ = new QLabel(this); error_->setObjectName(QStringLiteral("paletteError")); error_->setWordWrap(true); error_->setTextFormat(Qt::PlainText); layout->addWidget(error_);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this); layout->addWidget(buttons);
        connect(choose, &QPushButton::clicked, this, [this]() {
            const auto current = table_->currentIndex(); if (!current.isValid()) return;
            const QPersistentModelIndex index(model_->index(current.row(), 1)); QPointer<PaletteEditor> guard(this);
            const QColor color = QColorDialog::getColor(QColor(index.data().toString()), this, QStringLiteral("Choose Chat Color"));
            if (guard && index.isValid() && color.isValid()) model_->setData(index, color.name(QColor::HexRgb));
        });
        connect(reset, &QPushButton::clicked, this, [this]() { const int row = table_->currentIndex().row(); if (row >= 0) setDefault(row); });
        connect(all, &QPushButton::clicked, this, [this]() { for (int i = 0; i < 42; ++i) setDefault(i); });
        connect(reload, &QPushButton::clicked, this, [this]() { load(); });
        connect(model_, &QStandardItemModel::itemChanged, this, [this](QStandardItem *item) {
            if (item->column() != 1) return;
            const QSignalBlocker blocker(model_); const QColor color(item->text());
            item->setBackground(color.isValid() ? QBrush(color) : QBrush());
            item->setForeground(color.isValid() && color.lightness() < 128 ? Qt::white : Qt::black);
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() { save(); });
        load();
    }
private:
    static int key(int row) { return row < 32 ? row : row + 224; }
    void setDefault(int row) { if (row < model_->rowCount()) model_->setData(model_->index(row, 1), defaultIrcPaletteColor(row, QApplication::palette()).name(QColor::HexRgb)); }
    bool read(QByteArray &data, bool &exists)
    {
        QFile file(path_); exists = file.exists();
        if (!exists) { data.clear(); return true; }
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { error_->setText(QStringLiteral("Could not read the palette, or file exceeds 1 MiB.")); return false; }
        data = file.readAll();
        if (file.error() != QFileDevice::NoError) { error_->setText(file.errorString()); return false; }
        return true;
    }
    void load()
    {
        QByteArray data; bool exists;
        if (!read(data, exists)) { loaded_ = false; return; }
        original_ = data; existed_ = exists; loaded_ = true; originalColors_.clear(); model_->removeRows(0, model_->rowCount());
        const auto colors = parseIrcPaletteConfig(data); nativeColors_ = colors;
        const QStringList special{QStringLiteral("Selection text"), QStringLiteral("Selection background"), QStringLiteral("Chat text"), QStringLiteral("Chat background"),
            QStringLiteral("Marker"), QStringLiteral("New data"), QStringLiteral("Mention"), QStringLiteral("New message"), QStringLiteral("Away user"), QStringLiteral("Spell checker")};
        for (int i = 0; i < 42; ++i) {
            const QColor color = colors[i].isValid() ? colors[i] : defaultIrcPaletteColor(i, QApplication::palette()); originalColors_.append(color);
            auto *label = new QStandardItem(i < 32 ? QStringLiteral("IRC %1").arg(i) : special[i - 32]); label->setEditable(false);
            auto *value = new QStandardItem(color.name(QColor::HexRgb)); model_->appendRow({label, value});
            value->setBackground(color); value->setForeground(color.lightness() < 128 ? Qt::white : Qt::black);
        }
        table_->setCurrentIndex(model_->index(0, 1)); table_->selectRow(0); error_->clear();
    }
    void save()
    {
        if (!loaded_) { error_->setText(QStringLiteral("Reload the palette before saving.")); return; }
        QByteArray current; bool exists;
        if (!read(current, exists)) return;
        if (exists != existed_ || current != original_) { error_->setText(QStringLiteral("Palette changed externally. Reload before saving.")); return; }
        QMap<int, QColor> changes;
        const QRegularExpression hex(QStringLiteral("^#[0-9a-fA-F]{6}$"));
        for (int i = 0; i < 42; ++i) {
            const QString value = model_->item(i, 1)->text();
            if (!hex.match(value).hasMatch()) { error_->setText(QStringLiteral("Use #RRGGBB for every color.")); return; }
            const QColor color(value); if (color != originalColors_[i] || !nativeColors_[i].isValid()) changes.insert(key(i), color);
        }
        if (changes.isEmpty()) { finish(); return; }
        const QRegularExpression assignment(QStringLiteral("^\\s*color_(\\d+)\\s*="));
        QByteArray output;
        // Keep untouched lines byte-for-byte, including 16-bit channel precision.
        const auto lines = original_.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            const auto match = assignment.match(QString::fromUtf8(lines[i])); bool valid = false;
            const int colorKey = match.hasMatch() ? match.captured(1).toInt(&valid) : -1;
            if (valid && changes.contains(colorKey)) continue;
            output += lines[i]; if (i + 1 < lines.size()) output += '\n';
        }
        if (!output.isEmpty() && !output.endsWith('\n')) output += '\n';
        for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
            const QColor color = it.value();
            output += QStringLiteral("color_%1 = %2 %3 %4\n").arg(it.key()).arg(color.red() * 257, 4, 16, QLatin1Char('0')).arg(color.green() * 257, 4, 16, QLatin1Char('0')).arg(color.blue() * 257, 4, 16, QLatin1Char('0')).toUtf8();
        }
        QSaveFile file(path_);
        if (!file.open(QIODevice::WriteOnly)) { error_->setText(file.errorString()); return; }
        if (!existed_) file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        if (file.write(output) != output.size() || !file.commit()) { error_->setText(file.errorString()); return; }
        finish();
    }
    void finish()
    {
        const QPointer<PaletteEditor> guard(this);
        const auto applied = applied_;
        if (applied) applied();
        if (guard) accept();
    }
    QString path_; std::function<void()> applied_; QByteArray original_; bool existed_ = false, loaded_ = false;
    QVector<QColor> originalColors_, nativeColors_; QStandardItemModel *model_; QTableView *table_; QLabel *error_;
};
QPointer<PaletteEditor> editor;
}
void showPaletteEditor(const QString &path, QWidget *parent, std::function<void()> applied)
{
    if (!editor) editor = new PaletteEditor(path, parent, std::move(applied));
    editor->show(); editor->raise(); editor->activateWindow();
}
