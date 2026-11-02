/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nicklist.h"
#include "rankicon.h"
#include <QMenu>
#include <QPointer>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextDocument>
#include <algorithm>

NickList::NickList(QWidget *parent) : QListWidget(parent)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setToolTip(QStringLiteral("Double-click a nickname to run the configured user-list command; right-click for actions."));
    connect(this, &QListWidget::itemSelectionChanged, this, [this]() {
        if (!state_) return;
        for (int row = 0; row < count(); ++row) {
            auto *entry = item(row);
            if (entry->isHidden()) continue;
            const QString nick = entry->data(Qt::UserRole).toString();
            if (entry->isSelected()) state_->selected.insert(nick);
            else state_->selected.remove(nick);
        }
    });
    connect(this, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        const QString nick = item->data(Qt::UserRole).toString();
        if (!state_ || !state_->users.contains(nick)) return;
        if (activate) activate(nick);
        else if (action) action(nick, false);
    });
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        auto *item = point.x() < 0 ? currentItem() : itemAt(point);
        if (!item || !state_) return;
        const QString nick = item->data(Qt::UserRole).toString();
        const auto state = state_;
        const QPointer<NickList> guard(this);
        QMenu menu;
        auto *query = menu.addAction(QStringLiteral("Open Private Chat"));
        QMap<QAction *, NickAction> choices;
        QStringList targets{nick};
        if (state->selected.contains(nick)) {
            targets.clear();
            for (int row = 0; row < count(); ++row) {
                auto *entry = this->item(row);
                const QString target = entry->data(Qt::UserRole).toString();
                if (!entry->isHidden() && state->selected.contains(target)) targets.append(target);
            }
        }
        auto add = [&](const QString &label, const QString &id, NickAction kind, bool moderation) {
            auto *item = menu.addAction(label); item->setObjectName(id);
            item->setEnabled(bool(userAction) && (!moderation || moderationEnabled_)); choices.insert(item, kind);
        };
        add(QStringLiteral("Copy Nicknames"), QStringLiteral("copyNicks"), NickAction::Copy, false);
        add(QStringLiteral("Insert Nicknames"), QStringLiteral("insertNicks"), NickAction::Insert, false);
        add(QStringLiteral("User Details…"), QStringLiteral("userDetails"), NickAction::Details, false);
        add(QStringLiteral("Add to Friends List…"), QStringLiteral("addNickFriend"), NickAction::Friend, false);
        menu.addSeparator();
        add(QStringLiteral("Give Operator"), QStringLiteral("giveOp"), NickAction::Operator, true);
        add(QStringLiteral("Remove Operator"), QStringLiteral("removeOp"), NickAction::Deoperator, true);
        add(QStringLiteral("Give Voice"), QStringLiteral("giveVoice"), NickAction::Voice, true);
        add(QStringLiteral("Remove Voice"), QStringLiteral("removeVoice"), NickAction::Devoice, true);
        add(QStringLiteral("Kick…"), QStringLiteral("kickUsers"), NickAction::Kick, true);
        add(QStringLiteral("Ban"), QStringLiteral("banUsers"), NickAction::Ban, true);
        add(QStringLiteral("Kick and Ban…"), QStringLiteral("kickBanUsers"), NickAction::KickBan, true);
        add(QStringLiteral("Invite to Channel…"), QStringLiteral("inviteUsers"), NickAction::Invite, true);
        menu.addSeparator();
        if (decoratePopup) decoratePopup(&menu, targets, state);
        if (!guard) return;
        auto *whois = menu.addAction(QStringLiteral("WHOIS"));
        auto *chosen = menu.exec(point.x() < 0 ? mapToGlobal(rect().center())
                                               : viewport()->mapToGlobal(point));
        if (!guard) return;
        // IRC events can remove a user/session while the menu is open.
        if (action && state_ == state && state->users.contains(nick) &&
            (chosen == query || chosen == whois)) action(nick, chosen == whois);
        else if (chosen && choices.contains(chosen) && userAction && state_ == state) {
            QStringList live;
            for (const QString &target : targets) if (state->users.contains(target)) live.append(target);
            live.sort(Qt::CaseInsensitive);
            if (!live.isEmpty()) userAction(choices.value(chosen), live);
        }
    });
}

void NickList::setState(const std::shared_ptr<NickListState> &state)
{
    state_ = state;
    refresh();
}

void NickList::refresh()
{
    const QSignalBlocker blocker(this);
    const int scroll = verticalScrollBar()->value();
    const QString current = currentItem() ? currentItem()->data(Qt::UserRole).toString() : QString();
    clear();
    if (!state_) return;
    QStringList names = state_->users.keys();
    if (sortMode_ == 4) {
        QStringList ordered; QSet<QString> seen;
        for (const auto &nick : state_->insertionOrder) if (state_->users.contains(nick) && !seen.contains(nick)) { ordered.append(nick); seen.insert(nick); }
        for (const auto &nick : names) if (!seen.contains(nick)) ordered.append(nick);
        names = ordered;
    } else std::sort(names.begin(), names.end(), [this](const QString &a, const QString &b) {
        const int ar = state_->details.value(a).rank, br = state_->details.value(b).rank;
        int compared = 0;
        if ((sortMode_ == 0 || sortMode_ == 2) && ar != br) compared = ar < br ? -1 : 1;
        else compared = state_->compare ? state_->compare(a, b) : QString::compare(a, b, Qt::CaseInsensitive);
        if (!compared) compared = QString::compare(a, b);
        return sortMode_ == 2 || sortMode_ == 3 ? compared > 0 : compared < 0;
    });
    for (const QString &nick : names) {
        const auto details = state_->details.value(nick);
        const QString display = state_->users.value(nick);
        const QString prefix = display.endsWith(nick) ? display.left(display.size() - nick.size()).left(1) : QString();
        QString text = icons_ ? nick : display;
        if (hosts_ && !details.host.isEmpty()) text += QStringLiteral("  ") + details.host;
        auto *item = new QListWidgetItem(text, this);
        if (icons_) item->setIcon(rankIcon(prefix, state_->prefixes));
        item->setData(Qt::UserRole, nick);
        QFont font = item->font(); font.setItalic(details.away); font.setBold(details.me); item->setFont(font);
        if (details.away) item->setForeground(awayColor_.isValid() ? awayColor_ : palette().color(QPalette::Disabled, QPalette::Text));
        else if (nicknameColors_) item->setForeground(nicknameColors_(nick));
        const QString tooltip = QStringLiteral("%1\n%2\nHost: %3\nReal name: %4\nAccount: %5\nServer: %6")
            .arg(nick, details.away ? QStringLiteral("Away") : QStringLiteral("Here"), details.host, details.realName,
                 details.account, details.server);
        item->setToolTip(Qt::convertFromPlainText(tooltip));
        if (nick == current) setCurrentItem(item, QItemSelectionModel::NoUpdate);
        item->setSelected(state_->selected.contains(nick));
        const QString searchable = nick + QLatin1Char(' ') + details.host + QLatin1Char(' ') + details.realName + QLatin1Char(' ') + details.account;
        item->setHidden(!searchable.contains(filter_, Qt::CaseInsensitive));
    }
    verticalScrollBar()->setValue(scroll);
}

int NickList::visibleCount() const
{
    int visible = 0;
    for (int row = 0; row < count(); ++row) if (!item(row)->isHidden()) ++visible;
    return visible;
}

void NickList::selectNick(const QString &nick, bool scroll)
{
    if (!state_ || !state_->users.contains(nick)) return;
    state_->selected.insert(nick);
    for (int row = 0; row < count(); ++row) {
        auto *item = this->item(row);
        if (item->data(Qt::UserRole).toString() == nick) {
            item->setSelected(true);
            if (scroll) scrollToItem(item);
            return;
        }
    }
}
