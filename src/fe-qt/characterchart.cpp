/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "characterchart.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
namespace {
struct Range { const char *name; char32_t first, last; };
const Range ranges[] = {{"Latin", 0x20, 0xFF}, {"Latin Extended", 0x100, 0x24F}, {"Greek", 0x370, 0x3FF},
    {"Cyrillic", 0x400, 0x52F}, {"Punctuation", 0x2000, 0x206F}, {"Arrows", 0x2190, 0x21FF},
    {"Mathematics", 0x2200, 0x22FF}, {"Symbols", 0x2600, 0x26FF}, {"Emoji and pictographs", 0x1F300, 0x1FAFF}, {"All Unicode", 0, 0x10FFFF}};
QString codeLabel(char32_t code) { return QStringLiteral("U+%1").arg(quint32(code), 4, 16, QLatin1Char('0')).toUpper(); }
QString character(char32_t code)
{
    if (code > 0x10FFFF || code == 0x2028 || code == 0x2029 || (code >= 0xD800 && code <= 0xDFFF) || !QChar::isPrint(code)) return {};
    return QString::fromUcs4(&code, 1);
}
class CharacterChart final : public QDialog {
public:
    CharacterChart(QWidget *parent, std::function<bool(const QString &)> insert) : QDialog(parent), insert_(std::move(insert))
    {
        setObjectName(QStringLiteral("characterChart")); setWindowTitle(QStringLiteral("Character Chart")); resize(740, 670); setAttribute(Qt::WA_DeleteOnClose);
        auto *layout = new QVBoxLayout(this); auto *navigation = new QHBoxLayout;
        ranges_ = new QComboBox(this); ranges_->setObjectName(QStringLiteral("characterRange"));
        for (const auto &range : ranges) ranges_->addItem(QString::fromLatin1(range.name));
        page_ = new QSpinBox(this); page_->setObjectName(QStringLiteral("characterPage")); page_->setPrefix(QStringLiteral("Page ")); navigation->addWidget(ranges_, 1); navigation->addWidget(page_); layout->addLayout(navigation);
        auto *lookup = new QHBoxLayout;
        code_ = new QLineEdit(this); code_->setObjectName(QStringLiteral("characterCode")); code_->setPlaceholderText(QStringLiteral("Hex code point, for example U+1F600"));
        auto *find = new QPushButton(QStringLiteral("Find Code Point"), this); find->setObjectName(QStringLiteral("findCharacter")); lookup->addWidget(code_, 1); lookup->addWidget(find); layout->addLayout(lookup);
        table_ = new QTableWidget(16, 16, this); table_->setObjectName(QStringLiteral("characterGrid")); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->horizontalHeader()->hide(); table_->verticalHeader()->hide();
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); table_->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        QFont font = table_->font(); font.setPointSize(18); table_->setFont(font); layout->addWidget(table_, 1);
        selected_ = new QLabel(this); selected_->setObjectName(QStringLiteral("selectedCharacter")); selected_->setTextFormat(Qt::PlainText); selected_->setWordWrap(true); layout->addWidget(selected_);
        status_ = new QLabel(this); status_->setObjectName(QStringLiteral("characterStatus")); status_->setTextFormat(Qt::PlainText); status_->setWordWrap(true); layout->addWidget(status_);
        auto *help = new QLabel(QStringLiteral("Select a printable character. Insert adds it at the current conversation's input cursor; double-click also inserts. Missing glyphs depend on installed fonts. Control, surrogate and unassigned code points are excluded."), this);
        help->setWordWrap(true); help->setTextFormat(Qt::PlainText); layout->addWidget(help);
        auto *buttons = new QHBoxLayout;
        insertButton_ = new QPushButton(QStringLiteral("Insert"), this); insertButton_->setObjectName(QStringLiteral("insertCharacter"));
        copyButton_ = new QPushButton(QStringLiteral("Copy"), this); copyButton_->setObjectName(QStringLiteral("copyCharacter")); auto *close = new QPushButton(QStringLiteral("Close"), this);
        buttons->addWidget(insertButton_); buttons->addWidget(copyButton_); buttons->addStretch(); buttons->addWidget(close); layout->addLayout(buttons);
        connect(ranges_, &QComboBox::currentIndexChanged, this, [this]() { const QSignalBlocker blocker(page_); page_->setRange(0, int((range().last - range().first) / 256)); page_->setValue(0); render(); });
        connect(page_, &QSpinBox::valueChanged, this, [this]() { render(); });
        connect(table_, &QTableWidget::currentCellChanged, this, [this]() { updateSelection(); });
        connect(table_, &QTableWidget::cellDoubleClicked, this, [this]() { insertSelected(); });
        connect(find, &QPushButton::clicked, this, [this]() { findCode(); }); connect(code_, &QLineEdit::returnPressed, this, [this]() { findCode(); });
        connect(insertButton_, &QPushButton::clicked, this, [this]() { insertSelected(); });
        connect(copyButton_, &QPushButton::clicked, this, [this]() { const auto text = selection(); if (!text.isEmpty()) QApplication::clipboard()->setText(text); });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        for (auto *button : findChildren<QPushButton *>()) { button->setAutoDefault(false); button->setDefault(false); }
        page_->setRange(0, 0); render();
    }
private:
    const Range &range() const { return ranges[qMax(0, ranges_->currentIndex())]; }
    QString selection() const { auto *item = table_->currentItem(); return item && (item->flags() & Qt::ItemIsEnabled) ? item->text() : QString(); }
    void updateSelection()
    {
        const QString text = selection(); auto *item = table_->currentItem();
        selected_->setText(text.isEmpty() ? QStringLiteral("Select a printable character") : text + QStringLiteral("   ") + codeLabel(char32_t(item->data(Qt::UserRole).toUInt())));
        insertButton_->setEnabled(!text.isEmpty() && bool(insert_)); copyButton_->setEnabled(!text.isEmpty());
    }
    void render()
    {
        table_->clearContents(); const auto &section = range(); const char32_t first = section.first + char32_t(page_->value()) * 256;
        for (int offset = 0; offset < 256; ++offset) {
            const char32_t code = first + char32_t(offset); const QString text = code <= section.last ? character(code) : QString();
            auto *item = new QTableWidgetItem(text); item->setTextAlignment(Qt::AlignCenter); item->setData(Qt::UserRole, quint32(code)); item->setToolTip(codeLabel(code));
            item->setFlags(text.isEmpty() ? Qt::NoItemFlags : Qt::ItemIsEnabled | Qt::ItemIsSelectable); table_->setItem(offset / 16, offset % 16, item);
        }
        table_->setCurrentCell(-1, -1); updateSelection(); status_->clear();
    }
    void findCode()
    {
        QString text = code_->text().trimmed(); if (text.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive) || text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) text.remove(0, 2);
        bool valid = false; const uint value = text.toUInt(&valid, 16);
        if (!valid || value > 0x10FFFF || character(char32_t(value)).isEmpty()) { status_->setText(QStringLiteral("Enter a printable Unicode code point in hexadecimal.")); return; }
        ranges_->setCurrentIndex(int(sizeof(ranges) / sizeof(ranges[0])) - 1); page_->setValue(int(value / 256));
        const int offset = int(value % 256); table_->setCurrentCell(offset / 16, offset % 16); table_->scrollToItem(table_->currentItem()); status_->clear();
    }
    void insertSelected()
    {
        const QString text = selection(); if (text.isEmpty() || !insert_) return;
        const auto callback = insert_; const QPointer<CharacterChart> guard(this); const bool inserted = callback(text);
        if (guard) status_->setText(inserted ? QStringLiteral("Inserted into the current conversation's draft.") : QStringLiteral("There is no active conversation to insert into."));
    }
    std::function<bool(const QString &)> insert_; QComboBox *ranges_; QSpinBox *page_; QLineEdit *code_; QTableWidget *table_;
    QLabel *selected_, *status_; QPushButton *insertButton_, *copyButton_;
};
QPointer<CharacterChart> chart;
}
void showCharacterChart(QWidget *parent, std::function<bool(const QString &)> insert)
{ if (!chart) chart = new CharacterChart(parent, std::move(insert)); chart->show(); chart->raise(); chart->activateWindow(); }
