/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatinput.h"
#include "spellchecker.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionFrame>
#include <QTextLayout>
#include <QTimer>
#include <QToolTip>
#include <QCursor>
#include <QPointer>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QSet>

ChatInput::ChatInput(QWidget *parent) : IrcLineEdit(parent)
{
    spellingTimer_ = new QTimer(this); spellingTimer_->setSingleShot(true); spellingTimer_->setInterval(150);
    connect(spellingTimer_, &QTimer::timeout, this, [this]() { refreshSpelling(); });
    connect(this, &QLineEdit::textChanged, this, [this]() { spellingRanges_.clear(); update(); if (spellingEnabled_) spellingTimer_->start(); });
}
void ChatInput::setSpellChecker(std::shared_ptr<SpellChecker> checker, bool enabled, const QColor &color)
{
    spelling_ = std::move(checker); spellingEnabled_ = enabled; spellingColor_ = color; refreshSpelling();
}
QList<QPair<int, int>> ChatInput::spellingRanges() const
{
    QList<QPair<int, int>> result; for (const auto &range : spellingRanges_) result.append({range.start, range.length}); return result;
}
void ChatInput::refreshSpelling()
{
    spellingRanges_.clear();
    const QString raw = text();
    if (!spellingEnabled_ || !spelling_ || !spelling_->ready() || echoMode() != QLineEdit::Normal ||
        (!commandPrefix_.isEmpty() && raw.startsWith(commandPrefix_) && !raw.startsWith(commandPrefix_ + commandPrefix_))) { update(); return; }
    QString visible; QList<int> positions;
    for (int i = 0; i < raw.size();) {
        const ushort control = raw[i].unicode();
        if (control == 3 || control == 4) {
            ++i; const int digits = control == 3 ? 2 : 6;
            auto number = [&]() { int count = 0; while (i < raw.size() && count < digits && (control == 3 ? raw[i].isDigit() && raw[i].unicode() < 128 : QStringLiteral("0123456789abcdefABCDEF").contains(raw[i]))) { ++i; ++count; } };
            number(); if (i < raw.size() && raw[i] == QLatin1Char(',')) { ++i; number(); }
        } else if (control == 2 || control == 15 || control == 22 || control == 29 || control == 30 || control == 31) ++i;
        else { visible += raw[i]; positions.append(i++); }
    }
    static const QRegularExpression urls(QStringLiteral("(?:[a-zA-Z][a-zA-Z0-9+.-]*://|www\\.|[^\\s@]+@)[^\\s]+"));
    QList<QPair<int, int>> links; auto linkMatches = urls.globalMatch(visible);
    while (linkMatches.hasNext()) { const auto match = linkMatches.next(); links.append({match.capturedStart(), match.capturedEnd()}); }
    QSet<QString> nicknames; for (const QString &nick : nicks_) nicknames.insert(nick.toCaseFolded());
    static const QRegularExpression words(QStringLiteral("\\p{L}[\\p{L}\\p{M}]*(?:['’][\\p{L}\\p{M}]+)*"));
    auto matches = words.globalMatch(visible);
    while (matches.hasNext()) {
        const auto match = matches.next(); const int start = match.capturedStart(), end = match.capturedEnd(); const QString word = match.captured();
        if ((start && visible[start - 1].isLetterOrNumber()) || (end < visible.size() && visible[end].isLetterOrNumber()) || nicknames.contains(word.toCaseFolded())) continue;
        bool link = false; for (const auto &range : links) if (start < range.second && end > range.first) { link = true; break; }
        if (link || positions[end - 1] - positions[start] + 1 != word.size()) continue;
        if (spelling_->misspelled(word)) spellingRanges_.append({positions[start], int(word.size()), word});
    }
    update();
}
void ChatInput::paintEvent(QPaintEvent *event)
{
    IrcLineEdit::paintEvent(event);
    if (inputPreeditActive() || spellingRanges_.isEmpty() || echoMode() != QLineEdit::Normal) return;
    QTextLayout layout(displayText(), font()); QTextOption option; option.setTextDirection(displayText().isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight); layout.setTextOption(option);
    QPointF formattedOrigin;
    if (ircRenderingActive()) { prepareLayout(layout); formattedOrigin = layoutOrigin(layout); }
    else { layout.beginLayout(); auto line = layout.createLine(); if (line.isValid()) { line.setLineWidth(10000000); line.setLineWidth(qMax(qreal(1), line.naturalTextWidth())); } layout.endLayout(); }
    if (!layout.lineCount()) return;
    const auto line = layout.lineAt(0);
    const QRect caret = cursorRect(); const qreal origin = ircRenderingActive() ? formattedOrigin.x() : caret.center().x() - line.cursorToX(cursorPosition());
    QStyleOptionFrame frame; initStyleOption(&frame); QRect content = style()->subElementRect(QStyle::SE_LineEditContents, &frame, this); content = content.marginsRemoved(textMargins());
    QPainter painter(this); painter.setClipRect(content); painter.setPen(QPen(spellingColor_.isValid() ? spellingColor_ : QColor(Qt::red), 1));
    const qreal y = qMin(qreal(content.bottom() - 1), qreal(caret.bottom() - 2));
    for (const auto &range : spellingRanges_) {
        qreal left = origin + line.cursorToX(range.start), right = origin + line.cursorToX(range.start + range.length); if (left > right) std::swap(left, right);
        left = qMax(left, qreal(content.left())); right = qMin(right, qreal(content.right())); if (right <= left) continue;
        QPainterPath wave; wave.moveTo(left, y); bool down = true;
        for (qreal x = left + 2; x < right; x += 2) { wave.lineTo(x, y + (down ? 1 : -1)); down = !down; }
        wave.lineTo(right, y); painter.drawPath(wave);
    }
}

void InputHistory::record(const QString &line)
{
    if (!line.isEmpty() && (lines.isEmpty() || lines.last() != line)) {
        lines.append(line);
        if (lines.size() > 100) lines.removeFirst();
    }
    position = lines.size();
    draft.clear();
}

void ChatInput::resetCompletion()
{
    matches_.clear();
    match_ = -1;
}

void ChatInput::setConversation(InputHistory *history, const QStringList &nicks)
{
    ++conversationGeneration_; history_ = history;
    nicks_ = nicks;
    resetCompletion(); refreshSpelling();
}

bool ChatInput::event(QEvent *event)
{
    // QWidget normally consumes Tab for focus traversal before keyPressEvent.
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (defaultShortcuts_ && completionEnabled_ && ((key->key() == Qt::Key_Tab && (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::ShiftModifier)) || key->key() == Qt::Key_Backtab)) {
            keyPressEvent(key);
            return true;
        }
    }
    return QLineEdit::event(event);
}

void ChatInput::keyPressEvent(QKeyEvent *event)
{
    if (defaultShortcuts_ && !replacementChecked_ && event->modifiers() == Qt::NoModifier && (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) replaceWord();
    replacementChecked_ = false;
    if (event->matches(QKeySequence::Paste)) { pasteText(QApplication::clipboard()->text()); return; }
    if (defaultShortcuts_ && event->modifiers() == Qt::ControlModifier) {
        ushort control = 0;
        switch (event->key()) {
        case Qt::Key_B: control = 0x02; break;
        case Qt::Key_K: control = 0x03; break;
        case Qt::Key_I: control = 0x1d; break;
        case Qt::Key_U: control = 0x1f; break;
        case Qt::Key_R: control = 0x16; break;
        case Qt::Key_O: control = 0x0f; break;
        }
        if (control && !isReadOnly()) {
            insertFormatting(QChar(control)); return;
        }
    }
    if (defaultShortcuts_ && event->modifiers() == Qt::NoModifier && history_ &&
        (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        historyStep(event->key() == Qt::Key_Up);
        return;
    }
    if (defaultShortcuts_ && ((event->key() == Qt::Key_Tab && (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier)) || event->key() == Qt::Key_Backtab)) {
        completeNick(event->key() == Qt::Key_Backtab || event->modifiers() == Qt::ShiftModifier);
        return;
    }
    resetCompletion();
    QLineEdit::keyPressEvent(event);
}

void ChatInput::insertFormatting(QChar control)
{
    if (isReadOnly()) return;
    const QString selected = selectedText(); const int start = hasSelectedText() ? selectionStart() : cursorPosition();
    insert(control + selected + (selected.isEmpty() ? QString() : QString(control)));
    if (!selected.isEmpty()) setSelection(start + 1, selected.size());
    resetCompletion();
}
void ChatInput::historyStep(bool previous)
{
    replacementChecked_ = false;
    if (!history_) return;
    resetCompletion();
    if (previous) {
        if (history_->position == history_->lines.size()) history_->draft = text();
        if (history_->position > 0) --history_->position;
    } else if (history_->position < history_->lines.size()) {
        ++history_->position;
    } else {
        return;
    }
    setText(history_->position == history_->lines.size() ? history_->draft
                                                        : history_->lines[history_->position]);
}
void ChatInput::setCompletionOptions(bool enabled, const QString &suffix)
{
    if (completionEnabled_ == enabled && completionSuffix_ == suffix) return;
    completionEnabled_ = enabled; completionSuffix_ = suffix; resetCompletion();
}
void ChatInput::setCompletionThreshold(int amount, int order)
{
    amount = qBound(0, amount, 1000);
    if (amount != completionThreshold_ || order != completionOrder_) { completionThreshold_ = amount; completionOrder_ = order; resetCompletion(); }
}
void ChatInput::completeNick(bool previous)
{
    if (!completionEnabled_ || isReadOnly()) return;
    if (match_ < 0 || text() != completionText_ || cursorPosition() != completionStart_ + completionLength_) {
        resetCompletion();
        if (hasSelectedText()) return;
        completionStart_ = cursorPosition();
        while (completionStart_ > 0 && !text()[completionStart_ - 1].isSpace())
            --completionStart_;
        completionLength_ = cursorPosition() - completionStart_;
        const QString prefix = text().mid(completionStart_, completionLength_);
        if (prefix.isEmpty()) return;
        const bool command = completionStart_ == 0 && !commandPrefix_.isEmpty() && prefix.startsWith(commandPrefix_);
        const bool channel = QStringLiteral("#&!+").contains(prefix.front());
        QStringList candidates = !command && !channel && orderedNicks ? orderedNicks() : nicks_;
        if (completionWords && (command || channel)) {
            candidates = completionWords(command);
            if (command) for (QString &candidate : candidates) candidate.prepend(commandPrefix_);
        }
        for (const QString &candidate : candidates)
            if (candidate.size() >= prefix.size() && equivalent(candidate.left(prefix.size()), prefix)) matches_.append(candidate);
        if (command || channel || !orderedNicks) matches_.sort(Qt::CaseInsensitive);
        QSet<QString> seen;
        for (int i = 0; i < matches_.size();) {
            const QString folded = matches_[i].toCaseFolded();
            if (seen.contains(folded)) matches_.removeAt(i); else { seen.insert(folded); ++i; }
        }
        if (matches_.size() > 1 && (completionThreshold_ == 0 || matches_.size() > completionThreshold_)) {
            QString common = matches_.first();
            for (const QString &candidate : matches_) {
                int length = 0;
                while (length < common.size() && length < candidate.size()) {
                    const int units = common[length].isHighSurrogate() && length + 1 < common.size() ? 2 : 1;
                    if (length + units > candidate.size() || !equivalent(common.mid(length, units), candidate.mid(length, units))) break;
                    length += units;
                }
                common.truncate(length);
            }
            if (!common.isEmpty() && common.back().isHighSurrogate()) common.chop(1);
            if (common.size() > prefix.size()) {
                QString line = text(); line.replace(completionStart_, completionLength_, common); setText(line);
                setCursorPosition(completionStart_ + common.size());
            }
            const auto callback = completionMatches; const auto matches = matches_;
            resetCompletion(); if (callback) callback(matches); return;
        }
    }
    if (matches_.isEmpty()) return;
    match_ = match_ < 0 && previous ? matches_.size() - 1 : (match_ + (previous ? -1 : 1) + matches_.size()) % matches_.size();
    QString replacement = matches_[match_];
    const bool command = completionStart_ == 0 && !commandPrefix_.isEmpty() && replacement.startsWith(commandPrefix_);
    const bool channel = !replacement.isEmpty() && QStringLiteral("#&!+").contains(replacement.front());
    if (command || channel) replacement += QLatin1Char(' ');
    else if (completionStart_ == 0) replacement += completionSuffix_ + QLatin1Char(' ');
    QString line = text();
    line.replace(completionStart_, completionLength_, replacement);
    setText(line);
    completionLength_ = replacement.size();
    setCursorPosition(completionStart_ + completionLength_);
    completionText_ = line;
}

void ChatInput::replaceWord()
{
    if (!automaticReplacement_ || !wordReplacement || isReadOnly() || hasSelectedText() || (!commandPrefix_.isEmpty() && text().startsWith(commandPrefix_))) return;
    const int end = cursorPosition(); int start = end;
    while (start > 0 && !text()[start - 1].isSpace()) --start;
    if (start == end) return;
    const QString word = text().mid(start, end - start), replacement = wordReplacement(word);
    if (replacement == word) return;
    setSelection(start, end - start); insert(replacement); resetCompletion();
}

void ChatInput::pasteText(const QString &text)
{
    if (isReadOnly()) return;
    QString value = text; value.replace(QStringLiteral("\r\n"), QStringLiteral("\n")); value.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (!value.contains(QLatin1Char('\n'))) { insert(value); resetCompletion(); return; }
    if (!multilinePaste) return; // Multiline data must never silently collapse or execute.
    QString candidate = this->text();
    candidate.replace(hasSelectedText() ? selectionStart() : cursorPosition(), selectedText().size(), value);
    multilinePaste(candidate);
}
void ChatInput::contextMenuEvent(QContextMenuEvent *event)
{
    refreshSpelling();
    QMenu menu; menu.setObjectName(QStringLiteral("inputContextMenu"));
    const QString original = text(); const quint64 generation = conversationGeneration_;
    const int position = cursorPositionAt(event->pos());
    for (const auto &range : spellingRanges_) if (position >= range.start && position <= range.start + range.length) {
        auto current = [this, original, generation]() { return conversationGeneration_ == generation && text() == original; };
        for (const QString &suggestion : spelling_->suggestions(range.word)) {
            auto *action = menu.addAction(QString(suggestion).replace(QLatin1Char('&'), QStringLiteral("&&"))); action->setObjectName(QStringLiteral("spellingSuggestion"));
            action->setEnabled(!isReadOnly()); connect(action, &QAction::triggered, this, [this, current, range, suggestion]() { if (current() && !isReadOnly()) { setSelection(range.start, range.length); insert(suggestion); refreshSpelling(); } });
        }
        auto *ignore = menu.addAction(tr("Ignore Word")); ignore->setObjectName(QStringLiteral("spellingIgnore"));
        connect(ignore, &QAction::triggered, this, [this, current, range]() { if (current()) { spelling_->ignore(range.word); refreshSpelling(); } });
        auto *add = menu.addAction(tr("Add to Dictionary")); add->setObjectName(QStringLiteral("spellingAdd"));
        connect(add, &QAction::triggered, this, [this, current, range]() { if (!current()) return; QString error; if (!spelling_->add(range.word, &error)) QToolTip::showText(QCursor::pos(), error, this); refreshSpelling(); });
        menu.addSeparator(); break;
    }
    auto *undoAction = menu.addAction(tr("Undo"), this, &QLineEdit::undo); undoAction->setEnabled(isUndoAvailable());
    auto *redoAction = menu.addAction(tr("Redo"), this, &QLineEdit::redo); redoAction->setEnabled(isRedoAvailable());
    menu.addSeparator();
    auto *cutAction = menu.addAction(tr("Cut"), this, &QLineEdit::cut); cutAction->setEnabled(!isReadOnly() && hasSelectedText());
    auto *copyAction = menu.addAction(tr("Copy"), this, &QLineEdit::copy); copyAction->setEnabled(hasSelectedText());
    auto *pasteAction = menu.addAction(tr("Paste"), this, [this]() { pasteText(QApplication::clipboard()->text()); });
    pasteAction->setEnabled(!isReadOnly() && !QApplication::clipboard()->text().isEmpty());
    menu.addSeparator(); menu.addAction(tr("Select All"), this, &QLineEdit::selectAll);
    menu.exec(event->globalPos());
}
void ChatInput::dropEvent(QDropEvent *event)
{
    const QString text = event->mimeData()->text();
    if (text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\r'))) {
        setCursorPosition(cursorPositionAt(event->position().toPoint())); pasteText(text); event->acceptProposedAction();
    }
    else if (ircRenderingActive() && !isReadOnly() && event->mimeData()->hasText() && !event->mimeData()->hasUrls()) {
        setCursorPosition(cursorPositionAt(event->position().toPoint())); pasteText(text); event->acceptProposedAction();
    } else QLineEdit::dropEvent(event);
}
void ChatInput::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && QApplication::clipboard()->supportsSelection()) {
        const QString text = QApplication::clipboard()->text(QClipboard::Selection);
        if (text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\r'))) { setCursorPosition(cursorPositionAt(event->position().toPoint())); pasteText(text); return; }
    }
    IrcLineEdit::mouseReleaseEvent(event);
}
