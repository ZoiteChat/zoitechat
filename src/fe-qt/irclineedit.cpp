/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "irclineedit.h"
#include "ircformat.h"
#include <QApplication>
#include <QClipboard>
#include <QFocusEvent>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QTextBoundaryFinder>
#include <QTimer>
IrcLineEdit::IrcLineEdit(QWidget *parent) : QLineEdit(parent)
{
    cursorTimer_ = new QTimer(this);
    selectionTimer_ = new QTimer(this); selectionTimer_->setInterval(30);
    connect(selectionTimer_, &QTimer::timeout, this, [this]() { if (selecting_ && hasFocus()) extendSelection(selectionPoint_); else selectionTimer_->stop(); });
    connect(cursorTimer_, &QTimer::timeout, this, [this]() { cursorOn_ = !cursorOn_; update(); });
    connect(this, &QLineEdit::cursorPositionChanged, this, [this]() { restartCursor(); });
    connect(this, &QLineEdit::selectionChanged, this, [this]() { restartCursor(); });
    connect(this, &QLineEdit::textChanged, this, [this]() { restartCursor(); if (rendering_) updateGeometry(); });
}
void IrcLineEdit::restartCursor()
{
    cursorOn_ = true;
    if (rendering_ && hasFocus() && QApplication::cursorFlashTime() > 0) cursorTimer_->start(qMax(1, QApplication::cursorFlashTime() / 2));
    else cursorTimer_->stop();
    update();
    if (rendering_ && hasFocus()) QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle | Qt::ImAnchorRectangle);
}
void IrcLineEdit::setIrcRendering(bool enabled)
{
    if (rendering_ == enabled) return;
    rendering_ = enabled; offset_ = 0; selecting_ = false; selectionTimer_->stop();
    restartCursor(); updateGeometry();
}
bool IrcLineEdit::ircRenderingActive() const { return rendering_ && !preedit_ && echoMode() == QLineEdit::Normal && !text().isEmpty(); }
QRect IrcLineEdit::textArea() const
{
    QStyleOptionFrame frame; initStyleOption(&frame);
    return style()->subElementRect(QStyle::SE_LineEditContents, &frame, this).marginsRemoved(textMargins()).adjusted(2, 0, -2, 0);
}
void IrcLineEdit::prepareLayout(QTextLayout &layout) const
{
    layout.setText(displayText()); layout.setFont(font());
    QTextOption option; option.setWrapMode(QTextOption::NoWrap); option.setFlags(QTextOption::IncludeTrailingSpaces);
    option.setTextDirection(displayText().isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight);
    option.setAlignment(QStyle::visualAlignment(layoutDirection(), alignment())); layout.setTextOption(option);
    QList<QTextLayout::FormatRange> formats; int at = 0;
    for (const auto &run : parseIrcInput(text(), palette())) {
        formats.append({at, int(run.text.size()), run.format}); at += run.text.size();
    }
    layout.setFormats(formats);
    layout.beginLayout(); auto line = layout.createLine();
    if (line.isValid()) {
        line.setLineWidth(10000000);
        line.setLineWidth(qMax(qreal(qMax(1, textArea().width())), line.naturalTextWidth()));
    }
    layout.endLayout();
}
QPointF IrcLineEdit::layoutOrigin(QTextLayout &layout) const
{
    const QRect area = textArea();
    if (!layout.lineCount()) return area.topLeft();
    auto line = layout.lineAt(0);
    const qreal caret = line.cursorToX(cursorPosition()), width = qMax(1, area.width());
    if (caret < offset_) offset_ = caret;
    else if (caret > offset_ + width - 1) offset_ = caret - width + 1;
    offset_ = qBound(qreal(0), offset_, qMax(qreal(0), line.naturalTextWidth() - width + 1));
    return QPointF(area.left() - offset_, area.center().y() - line.height() / 2);
}
QRect IrcLineEdit::cursorRect() const
{
    if (!ircRenderingActive()) return QLineEdit::cursorRect();
    QTextLayout layout; prepareLayout(layout); const auto origin = layoutOrigin(layout);
    if (!layout.lineCount()) return QLineEdit::cursorRect();
    const auto line = layout.lineAt(0);
    return QRect(qRound(origin.x() + line.cursorToX(cursorPosition())), qRound(origin.y()), 1, qCeil(line.height()));
}
int IrcLineEdit::cursorPositionAt(const QPoint &point)
{
    if (!ircRenderingActive()) return QLineEdit::cursorPositionAt(point);
    QTextLayout layout; prepareLayout(layout); const auto origin = layoutOrigin(layout);
    return layout.lineCount() ? layout.lineAt(0).xToCursor(point.x() - origin.x()) : 0;
}
QVariant IrcLineEdit::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (ircRenderingActive() && (query == Qt::ImCursorRectangle || query == Qt::ImAnchorRectangle)) {
        if (query == Qt::ImCursorRectangle || !hasSelectedText()) return cursorRect();
        const int anchor = cursorPosition() == selectionStart() ? selectionEnd() : selectionStart();
        QTextLayout layout; prepareLayout(layout); const auto origin = layoutOrigin(layout);
        if (layout.lineCount()) return QRectF(origin.x() + layout.lineAt(0).cursorToX(anchor), origin.y(), 1, layout.lineAt(0).height());
    }
    return QLineEdit::inputMethodQuery(query);
}
void IrcLineEdit::paintEvent(QPaintEvent *event)
{
    if (!ircRenderingActive()) { QLineEdit::paintEvent(event); return; }
    QStyleOptionFrame frame; initStyleOption(&frame);
    QPainter painter(this); style()->drawPrimitive(QStyle::PE_PanelLineEdit, &frame, &painter, this);
    painter.setClipRect(textArea()); painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::Text));
    QTextLayout layout; prepareLayout(layout); const auto origin = layoutOrigin(layout);
    QList<QTextLayout::FormatRange> selection;
    if (hasSelectedText()) {
        QTextCharFormat format; format.setForeground(palette().brush(QPalette::HighlightedText)); format.setBackground(palette().brush(QPalette::Highlight));
        selection.append({selectionStart(), selectionLength(), format});
    }
    layout.draw(&painter, origin, selection);
    if (hasFocus() && !isReadOnly() && cursorOn_) {
        if (hasSelectedText()) painter.setPen(palette().color(QPalette::HighlightedText));
        else for (const auto &range : layout.formats())
            if (cursorPosition() >= range.start && cursorPosition() < range.start + range.length && range.format.foreground().style() != Qt::NoBrush) painter.setPen(range.format.foreground().color());
        layout.drawCursor(&painter, origin, cursorPosition(), 1);
    }
}
void IrcLineEdit::inputMethodEvent(QInputMethodEvent *event)
{
    // Native GTK likewise suspends IRC attributes while IME preedit is active.
    preedit_ = !event->preeditString().isEmpty();
    QLineEdit::inputMethodEvent(event); restartCursor();
}
void IrcLineEdit::focusInEvent(QFocusEvent *event) { QLineEdit::focusInEvent(event); restartCursor(); }
void IrcLineEdit::focusOutEvent(QFocusEvent *event) { selecting_ = false; selectionTimer_->stop(); QLineEdit::focusOutEvent(event); restartCursor(); }
void IrcLineEdit::mousePressEvent(QMouseEvent *event)
{
    if (!ircRenderingActive()) { QLineEdit::mousePressEvent(event); return; }
    if (event->button() == Qt::LeftButton) {
        const int oldAnchor = hasSelectedText() && cursorPosition() == selectionStart() ? selectionEnd() : hasSelectedText() ? selectionStart() : cursorPosition();
        const int position = cursorPositionAt(event->position().toPoint());
        mouseAnchor_ = event->modifiers() & Qt::ShiftModifier ? oldAnchor : position;
        setFocus(Qt::MouseFocusReason); setSelection(mouseAnchor_, position - mouseAnchor_); selecting_ = true; selectingWord_ = false; selectionPoint_ = event->position().toPoint(); event->accept();
    } else { QLineEdit::mousePressEvent(event); }
}
void IrcLineEdit::extendSelection(const QPoint &point)
{
    int position = cursorPositionAt(point), anchor = mouseAnchor_;
    if (selectingWord_) {
        QTextBoundaryFinder words(QTextBoundaryFinder::Word, text()); words.setPosition(position);
        if (position < mouseAnchor_) {
            anchor = wordEnd_;
            if (!(words.boundaryReasons() & QTextBoundaryFinder::StartOfItem)) position = qMax(0, words.toPreviousBoundary());
        } else if (position > wordEnd_) {
            if (!(words.boundaryReasons() & QTextBoundaryFinder::EndOfItem)) { const int end = words.toNextBoundary(); position = end < 0 ? text().size() : end; }
        } else position = wordEnd_;
    }
    setSelection(anchor, position - anchor);
}
void IrcLineEdit::mouseMoveEvent(QMouseEvent *event)
{
    if (ircRenderingActive() && selecting_ && (event->buttons() & Qt::LeftButton)) {
        selectionPoint_ = event->position().toPoint(); extendSelection(selectionPoint_);
        if (selectionPoint_.x() < textArea().left() || selectionPoint_.x() > textArea().right()) selectionTimer_->start(); else selectionTimer_->stop();
        event->accept();
    } else QLineEdit::mouseMoveEvent(event);
}
void IrcLineEdit::mouseReleaseEvent(QMouseEvent *event)
{
    if (ircRenderingActive() && event->button() == Qt::MiddleButton && QApplication::clipboard()->supportsSelection()) {
        if (!isReadOnly()) { setCursorPosition(cursorPositionAt(event->position().toPoint())); insert(QApplication::clipboard()->text(QClipboard::Selection)); }
        event->accept(); return;
    }
    if (ircRenderingActive() && selecting_ && event->button() == Qt::LeftButton) {
        extendSelection(event->position().toPoint()); selecting_ = false; selectionTimer_->stop();
        if (hasSelectedText() && QApplication::clipboard()->supportsSelection()) QApplication::clipboard()->setText(selectedText(), QClipboard::Selection);
        event->accept();
    } else QLineEdit::mouseReleaseEvent(event);
}
void IrcLineEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!ircRenderingActive() || event->button() != Qt::LeftButton) { QLineEdit::mouseDoubleClickEvent(event); return; }
    const int position = cursorPositionAt(event->position().toPoint());
    QTextBoundaryFinder words(QTextBoundaryFinder::Word, text()); words.setPosition(position);
    int start = position;
    if (!(words.boundaryReasons() & QTextBoundaryFinder::StartOfItem)) start = words.toPreviousBoundary();
    words.setPosition(position); int end = words.toNextBoundary();
    if (start < 0) start = 0;
    if (end < 0) end = text().size();
    mouseAnchor_ = start; wordEnd_ = end; selecting_ = true; selectingWord_ = true; selectionPoint_ = event->position().toPoint(); setSelection(start, end - start); event->accept();
}
QSize IrcLineEdit::sizeHint() const
{
    auto size = QLineEdit::sizeHint();
    if (ircRenderingActive()) { QTextLayout layout; prepareLayout(layout); if (layout.lineCount()) size.setHeight(qMax(size.height(), qCeil(layout.lineAt(0).height()) + 8)); }
    return size;
}
QSize IrcLineEdit::minimumSizeHint() const { auto size = QLineEdit::minimumSizeHint(); size.setHeight(qMax(size.height(), sizeHint().height())); return size; }
