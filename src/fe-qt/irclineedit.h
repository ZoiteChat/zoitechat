/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_IRCLINEEDIT_H
#define ZOITECHAT_QT_IRCLINEEDIT_H
#include <QLineEdit>
#include <QTextLayout>
class QTimer;
// QLineEdit retains editing, undo and the original IRC controls. This layer
// provides formatted geometry for painting, pointer selection and IME queries.
class IrcLineEdit : public QLineEdit {
public:
    explicit IrcLineEdit(QWidget *parent = nullptr);
    void setIrcRendering(bool enabled);
    bool ircRendering() const { return rendering_; }
    int cursorPositionAt(const QPoint &point);
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    bool ircRenderingActive() const;
    bool inputPreeditActive() const { return preedit_; }
    void prepareLayout(QTextLayout &layout) const;
    QPointF layoutOrigin(QTextLayout &layout) const;
    QRect cursorRect() const;
    void paintEvent(QPaintEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
private:
    QRect textArea() const;
    void restartCursor();
    void extendSelection(const QPoint &point);
    bool rendering_ = false, preedit_ = false, cursorOn_ = true, selecting_ = false, selectingWord_ = false;
    int mouseAnchor_ = 0, wordEnd_ = 0;
    QPoint selectionPoint_;
    mutable qreal offset_ = 0;
    QTimer *cursorTimer_ = nullptr, *selectionTimer_ = nullptr;
};
#endif
