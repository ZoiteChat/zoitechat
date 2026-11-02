/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_PANEDRAG_H
#define ZOITECHAT_QT_PANEDRAG_H
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QRubberBand>
#include <QUuid>
#include <functional>

// One controller per main window. The private payload keeps drags scoped to
// their originating window; ordinary nick, text and file drops pass through.
class PaneDrag final : public QObject {
public:
    enum Pane { Switcher, Users };
    explicit PaneDrag(QWidget *area) : QObject(area), area_(area), token_(QUuid::createUuid().toByteArray()) {
        area->setAcceptDrops(true);
        indicator_ = new QRubberBand(QRubberBand::Rectangle, area);
        indicator_->setAttribute(Qt::WA_TransparentForMouseEvents);
        qApp->installEventFilter(this);
    }
    void source(QWidget *widget, Pane pane) {
        sources_.append({widget, pane});
        widget->setToolTip(QStringLiteral("Ctrl+Shift-drag to move this pane"));
        widget->setWhatsThis(QStringLiteral("Hold Ctrl+Shift and drag to move this pane. Drop at a side, or at the top/bottom for conversation tabs."));
    }
    QMimeData *mimeFor(Pane pane) const {
        auto *mime = new QMimeData;
        mime->setData(mimeType(), token_ + (pane == Users ? ":users" : ":switcher"));
        return mime;
    }
    std::function<bool(Pane, int)> move;
    std::function<bool()> horizontalTabs;
protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        auto *widget = qobject_cast<QWidget *>(object);
        if (!area_ || !widget || (widget != area_ && !area_->isAncestorOf(widget))) return false;
        const auto type = event->type();
        if (type == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && mouse->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
                for (const auto &source : sources_) if (source.first == widget) {
                    pressed_ = widget; pane_ = source.second; start_ = mouse->position().toPoint();
                    return true;
                }
            }
        } else if (type == QEvent::MouseButtonRelease && pressed_) {
            pressed_ = nullptr;
            return true;
        } else if (type == QEvent::MouseMove && pressed_ == widget) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (!(mouse->buttons() & Qt::LeftButton)) { pressed_ = nullptr; return false; }
            if ((mouse->position().toPoint() - start_).manhattanLength() < QApplication::startDragDistance()) return true;
            pressed_ = nullptr;
            QDrag drag(widget);
            drag.setMimeData(mimeFor(pane_));
            drag.setPixmap(widget->grab().scaled(240, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            drag.exec(Qt::MoveAction);
            indicator_->hide();
            return true;
        } else if (type == QEvent::DragEnter || type == QEvent::DragMove || type == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            const QByteArray payload = drop->mimeData()->data(mimeType());
            Pane pane;
            if (payload == token_ + ":users") pane = Users;
            else if (payload == token_ + ":switcher") pane = Switcher;
            else return false;
            if (!(drop->possibleActions() & Qt::MoveAction)) { drop->ignore(); return true; }
            const QPoint point = widget->mapTo(area_, drop->position().toPoint());
            if (!area_->rect().contains(point)) { indicator_->hide(); drop->ignore(); return true; }
            const bool center = point.x() > area_->width() / 3 && point.x() < 2 * area_->width() / 3;
            int position;
            if (pane == Switcher && horizontalTabs && horizontalTabs() && center && (point.y() < 48 || point.y() >= area_->height() - 48)) {
                position = point.y() < 48 ? 5 : 6;
                indicator_->setGeometry(0, position == 5 ? 0 : area_->height() - 32, area_->width(), 32);
            } else {
                const bool left = point.x() < area_->width() / 2, upper = point.y() < area_->height() / 2;
                position = (left ? 1 : 3) + (upper ? 0 : 1);
                const int width = qMin(200, area_->width() / 3), height = area_->height() / 2;
                indicator_->setGeometry(left ? 0 : area_->width() - width, upper ? 0 : height, width, height);
            }
            if (type == QEvent::Drop) {
                indicator_->hide();
                if (!move || !move(pane, position)) { drop->ignore(); return true; }
            } else { indicator_->show(); indicator_->raise(); }
            drop->setDropAction(Qt::MoveAction); drop->accept(); return true;
        } else if (type == QEvent::DragLeave) indicator_->hide();
        return false;
    }
private:
    static const char *mimeType() { return "application/x-zoitechat-qt-pane"; }
    QPointer<QWidget> area_, pressed_;
    QRubberBand *indicator_ = nullptr;
    QList<QPair<QPointer<QWidget>, Pane>> sources_;
    QByteArray token_;
    QPoint start_;
    Pane pane_ = Switcher;
};
#endif
