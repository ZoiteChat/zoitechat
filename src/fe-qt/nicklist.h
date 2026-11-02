/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_NICKLIST_H
#define ZOITECHAT_QT_NICKLIST_H
#include <QListWidget>
#include <QMap>
#include <QSet>
#include <functional>
#include <memory>

class QMenu;
struct NickDetails {
    QString host, realName, account, server;
    bool away = false;
    bool me = false;
    int rank = 1000;
};

struct NickListState {
    QString prefixes;
    QStringList insertionOrder;
    std::function<int(const QString &, const QString &)> compare;
    QMap<QString, QString> users;
    QSet<QString> selected;
    QMap<QString, NickDetails> details;
};
enum class NickAction { Operator, Deoperator, Voice, Devoice, Kick, Ban, KickBan, Invite, Copy, Insert, Details, Friend };

class NickList final : public QListWidget {
public:
    explicit NickList(QWidget *parent = nullptr);
    void setState(const std::shared_ptr<NickListState> &state);
    void refresh();
    void selectNick(const QString &nick, bool scroll);
    void setFilterText(const QString &text) { filter_ = text; refresh(); }
    void setDisplayOptions(bool icons, bool hosts) { if (icons_ != icons || hosts_ != hosts) { icons_ = icons; hosts_ = hosts; refresh(); } }
    void setSortMode(int mode) { mode = qBound(0, mode, 4); if (sortMode_ != mode) { sortMode_ = mode; refresh(); } }
    void setRankSorting(bool enabled) { setSortMode(enabled ? 0 : 1); }
    std::function<void(const QString &)> activate;
    int visibleCount() const;
    // false opens a private conversation, true requests WHOIS information.
    std::function<void(const QString &, bool)> action;
    std::function<void(NickAction, const QStringList &)> userAction;
    std::function<void(QMenu *, const QStringList &, const std::shared_ptr<NickListState> &)> decoratePopup;
    void setNicknameColors(std::function<QColor(const QString &)> colors) { nicknameColors_ = std::move(colors); refresh(); }
    void setAwayColor(const QColor &color) { awayColor_ = color; refresh(); }
    void setModerationEnabled(bool enabled) { moderationEnabled_ = enabled; }
private:
    std::shared_ptr<NickListState> state_;
    bool moderationEnabled_ = false;
    int sortMode_ = 1;
    bool icons_ = false, hosts_ = false;
    QString filter_;
    QColor awayColor_;
    std::function<QColor(const QString &)> nicknameColors_;
};
#endif
