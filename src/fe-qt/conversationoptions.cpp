/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "conversationoptions.h"
#include "conversationoptions-bridge.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
namespace {
class OptionsDialog final : public QDialog {
public:
    OptionsDialog(void *session, QWidget *parent, std::function<bool()> alive, std::function<void()> applied)
        : QDialog(parent), session_(session), alive_(std::move(alive)), applied_(std::move(applied))
    {
        setObjectName(QStringLiteral("conversationOptions")); setWindowTitle(QStringLiteral("Conversation Options")); setAttribute(Qt::WA_DeleteOnClose);
        auto *layout = new QVBoxLayout(this); source_ = new QLabel(this); source_->setTextFormat(Qt::PlainText); layout->addWidget(source_);
        auto *form = new QFormLayout; layout->addLayout(form);
        const char *labels[] = {"Desktop notifications", "Beep", "Flash taskbar", "Tray activity", "Hide joins and parts", "Log to disk", "Save scrollback to disk", "Strip message colors"};
        for (int i = 0; i < QT_CONVERSATION_OPTION_COUNT; ++i) {
            options_[i] = new QComboBox(this); options_[i]->setObjectName(QStringLiteral("conversationOption%1").arg(i));
            options_[i]->addItem(QStringLiteral("Default"), 2); options_[i]->addItem(QStringLiteral("On"), 1); options_[i]->addItem(QStringLiteral("Off"), 0);
            form->addRow(QString::fromLatin1(labels[i]), options_[i]);
        }
        auto *help = new QLabel(QStringLiteral("Default follows global settings. Changes apply to this conversation. The core saves named-network settings during normal shutdown; unnamed networks keep settings for this session only."), this);
        help->setWordWrap(true); help->setTextFormat(Qt::PlainText); layout->addWidget(help);
        error_ = new QLabel(this); error_->setObjectName(QStringLiteral("conversationOptionsError")); error_->setTextFormat(Qt::PlainText); error_->setWordWrap(true); layout->addWidget(error_);
        auto *reload = new QPushButton(QStringLiteral("Reload"), this); reload->setObjectName(QStringLiteral("reloadConversationOptions")); layout->addWidget(reload);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this); buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Apply")); layout->addWidget(buttons);
        connect(reload, &QPushButton::clicked, this, [this]() { load(); }); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (!loaded_ || !alive_ || !alive_()) { error_->setText(QStringLiteral("The original conversation has closed or changed.")); return; }
            int values[QT_CONVERSATION_OPTION_COUNT]; for (int i = 0; i < QT_CONVERSATION_OPTION_COUNT; ++i) values[i] = options_[i]->currentData().toInt();
            char error[1024] = {}; QPointer<OptionsDialog> guard(this);
            const int result = qt_core_conversation_options_apply(session_, name_.toUtf8().constData(), network_.toUtf8().constData(), expected_, values, error, sizeof(error));
            if (!guard) return;
            if (!result) { error_->setText(QString::fromUtf8(error)); return; }
            const auto applied = applied_; if (applied) applied(); if (guard) accept();
        });
        load();
    }
private:
    void load() {
        QtConversationOptions state{};
        if (!alive_ || !alive_() || !qt_core_conversation_options(session_, &state) || (bound_ && (name_ != QString::fromUtf8(state.name) || network_ != QString::fromUtf8(state.network)))) {
            loaded_ = false; error_->setText(QStringLiteral("The original conversation is no longer available.")); return;
        }
        name_ = QString::fromUtf8(state.name); network_ = QString::fromUtf8(state.network);
        source_->setText(QStringLiteral("%1 · %2").arg(name_, state.persistent ? network_ : QStringLiteral("Unnamed network (session only)")));
        for (int i = 0; i < QT_CONVERSATION_OPTION_COUNT; ++i) { expected_[i] = state.values[i]; options_[i]->setCurrentIndex(options_[i]->findData(expected_[i])); }
        loaded_ = true; bound_ = true; error_->clear();
    }
    void *session_; std::function<bool()> alive_; std::function<void()> applied_; QString name_, network_; int expected_[QT_CONVERSATION_OPTION_COUNT] = {};
    QComboBox *options_[QT_CONVERSATION_OPTION_COUNT]; QLabel *source_, *error_; bool loaded_ = false, bound_ = false;
};
}
void showConversationOptions(void *session, QWidget *parent, std::function<bool()> alive, std::function<void()> applied)
{ auto *dialog = new OptionsDialog(session, parent, std::move(alive), std::move(applied)); dialog->show(); dialog->raise(); dialog->activateWindow(); }
