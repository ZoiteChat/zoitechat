/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "characterchart.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <cstdio>
int main(int argc, char **argv)
{
    QApplication app(argc, argv); QWidget parent; QLineEdit input; input.setText(QStringLiteral("aOLDz")); input.setSelection(1, 3);
    bool ok = true, active = true; int inserted = 0;
    auto check = [&](bool value, const char *message) { if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    showCharacterChart(&parent, [&](const QString &text) { if (!active) return false; input.insert(text); ++inserted; return true; });
    auto *chart = parent.findChild<QDialog *>(QStringLiteral("characterChart"));
    auto *code = chart->findChild<QLineEdit *>(QStringLiteral("characterCode")); auto *find = chart->findChild<QPushButton *>(QStringLiteral("findCharacter"));
    auto *insert = chart->findChild<QPushButton *>(QStringLiteral("insertCharacter")); auto *grid = chart->findChild<QTableWidget *>(QStringLiteral("characterGrid"));
    check(grid->rowCount() == 16 && grid->columnCount() == 16 && !insert->isEnabled(), "bounded character grid starts without an accidental selection");
    code->setText(QStringLiteral("U+1F600")); QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); QApplication::sendEvent(code, &enter);
    check(inserted == 0 && insert->isEnabled() && chart->findChild<QLabel *>(QStringLiteral("selectedCharacter"))->text().contains(QStringLiteral("U+1F600")), "Enter performs supplementary-plane lookup without inserting");
    insert->click(); check(input.text() == QString::fromUtf8("a😀z") && input.cursorPosition() == 3, "supplementary character replaces selection and advances UTF-16 cursor");
    chart->findChild<QPushButton *>(QStringLiteral("copyCharacter"))->click(); check(QApplication::clipboard()->text() == QString::fromUtf8("😀"), "copy preserves full Unicode character");
    code->setText(QStringLiteral("0041")); QApplication::sendEvent(code, &enter); check(inserted == 1 && grid->currentItem()->text() == QStringLiteral("A"), "lookup never submits a previously focused insert button");
    active = false; insert->click(); check(inserted == 1 && chart->findChild<QLabel *>(QStringLiteral("characterStatus"))->text().contains(QStringLiteral("no active")), "unavailable conversation produces local status without insertion");
    for (const auto &invalid : {QStringLiteral("D800"), QStringLiteral("110000"), QStringLiteral("000A"), QStringLiteral("2028")}) {
        code->setText(invalid); find->click(); check(chart->findChild<QLabel *>(QStringLiteral("characterStatus"))->text().contains(QStringLiteral("printable")), "nonprintable/invalid code points rejected");
    }
    chart->findChild<QSpinBox *>(QStringLiteral("characterPage"))->setValue(0xD8);
    bool enabled = false;
    for (int row = 0; row < 16; ++row) for (int col = 0; col < 16; ++col) enabled |= bool(grid->item(row, col)->flags() & Qt::ItemIsEnabled);
    check(!enabled, "surrogate page exposes no insertable cells");
    chart->findChild<QComboBox *>(QStringLiteral("characterRange"))->setCurrentIndex(2);
    check(!insert->isEnabled(), "range changes clear selection");
    return ok ? 0 : 1;
}
