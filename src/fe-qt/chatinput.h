/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_CHATINPUT_H
#define ZOITECHAT_QT_CHATINPUT_H
#include "irclineedit.h"
#include <QStringList>
#include <functional>
#include <memory>
class SpellChecker;
class QTimer;

// State belongs to a conversation, so switching channels preserves history.
struct InputHistory {
    QStringList lines;
    QString draft;
    int position = 0;
    void record(const QString &line);
};

class ChatInput final : public IrcLineEdit {
public:
    explicit ChatInput(QWidget *parent = nullptr);
    void setSpellChecker(std::shared_ptr<SpellChecker> checker, bool enabled, const QColor &color);
    void refreshSpelling();
    QList<QPair<int, int>> spellingRanges() const;
    void setConversation(InputHistory *history, const QStringList &nicks);
    void setNicks(const QStringList &nicks) { if (nicks_ != nicks) { nicks_ = nicks; resetCompletion(); refreshSpelling(); } }
    void pasteText(const QString &text);
    void historyStep(bool previous);
    void setDefaultShortcutsEnabled(bool enabled) { defaultShortcuts_ = enabled; }
    void insertFormatting(QChar control);
    void completeNick(bool previous = false);
    void setCompletionOptions(bool enabled, const QString &suffix);
    void checkReplacement() { if (!replacementChecked_) replaceWord(); replacementChecked_ = true; }
    void setCommandPrefix(const QString &prefix) { if (commandPrefix_ != prefix) { commandPrefix_ = prefix; resetCompletion(); refreshSpelling(); } }
    void setCompletionThreshold(int amount, int order = 0);
    std::function<bool(const QString &, const QString &)> completionEquivalent;
    std::function<QStringList()> orderedNicks;
    std::function<void(const QStringList &)> completionMatches;
    std::function<QStringList(bool)> completionWords; // true: commands; false: channel names
    void setAutomaticReplacement(bool enabled) { automaticReplacement_ = enabled; }
    std::function<void(const QString &)> multilinePaste;
    std::function<QString(const QString &)> wordReplacement;
protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    struct SpellingRange { int start, length; QString word; };
    std::shared_ptr<SpellChecker> spelling_;
    QList<SpellingRange> spellingRanges_;
    QTimer *spellingTimer_ = nullptr;
    QColor spellingColor_;
    bool spellingEnabled_ = false;
    quint64 conversationGeneration_ = 0;
    void resetCompletion();
    bool equivalent(const QString &left, const QString &right) const { return completionEquivalent ? completionEquivalent(left, right) : left.compare(right, Qt::CaseInsensitive) == 0; }
    void replaceWord();
    bool defaultShortcuts_ = true;
    bool completionEnabled_ = true, automaticReplacement_ = true, replacementChecked_ = false;
    int completionThreshold_ = 1000, completionOrder_ = 0;
    QString commandPrefix_ = QStringLiteral("/");
    QString completionSuffix_ = QStringLiteral(":");
    InputHistory *history_ = nullptr;
    QStringList nicks_, matches_;
    QString completionText_;
    int completionStart_ = 0, completionLength_ = 0, match_ = -1;
};
#endif
