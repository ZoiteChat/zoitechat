/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "menumarkup.h"
#include <QAction>
#include <QApplication>
#include <QFont>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>
#include <cmath>
#include <pango/pangocairo.h>

namespace {
const char markupProperty[] = "zoitechatMenuMarkup";
struct Parsed {
    PangoAttrList *attributes = nullptr;
    char *text = nullptr;
    bool valid;
    Parsed(const QString &markup, gunichar accelerator) {
        const auto bytes = markup.toUtf8();
        valid = pango_parse_markup(bytes.constData(), bytes.size(), accelerator, &attributes, &text, nullptr, nullptr);
    }
    ~Parsed() { if (attributes) pango_attr_list_unref(attributes); g_free(text); }
};
gboolean mnemonicAttribute(PangoAttribute *attribute, gpointer) {
    return attribute->klass->type == PANGO_ATTR_UNDERLINE &&
           reinterpret_cast<PangoAttrInt *>(attribute)->value == PANGO_UNDERLINE_LOW;
}
QImage image(const QString &markup, const QFont &font, const QColor &color, qreal dpi, qreal ratio, bool mnemonics) {
    Parsed parsed(markup, '_');
    if (!parsed.valid) return {};
    if (!mnemonics) {
        PangoAttrList *removed = pango_attr_list_filter(parsed.attributes, mnemonicAttribute, nullptr);
        if (removed) pango_attr_list_unref(removed);
    }
    auto *context = pango_font_map_create_context(pango_cairo_font_map_get_default());
    pango_cairo_context_set_resolution(context, dpi);
    auto *description = pango_font_description_new();
    pango_font_description_set_family(description, font.family().toUtf8().constData());
    if (font.pointSizeF() > 0) pango_font_description_set_size(description, int(font.pointSizeF() * PANGO_SCALE));
    else pango_font_description_set_absolute_size(description, qMax(1, font.pixelSize()) * PANGO_SCALE);
    pango_font_description_set_weight(description, PangoWeight(font.weight()));
    pango_font_description_set_style(description, font.italic() ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    pango_context_set_font_description(context, description);
    pango_font_description_free(description);
    auto *layout = pango_layout_new(context);
    pango_layout_set_text(layout, parsed.text, -1);
    pango_layout_set_attributes(layout, parsed.attributes);
    PangoRectangle ink, logical;
    pango_layout_get_pixel_extents(layout, &ink, &logical);
    const QRect bounds = QRect(ink.x, ink.y, ink.width, ink.height).united(QRect(logical.x, logical.y, logical.width, logical.height));
    const qreal scaledWidth = qMax(qreal(1), std::ceil(bounds.width() * ratio));
    const qreal scaledHeight = qMax(qreal(1), std::ceil(bounds.height() * ratio));
    // A menu label should not allocate an unbounded raster for pathological input.
    QImage result;
    if (scaledWidth <= 16384 && scaledHeight <= 4096 && scaledWidth * scaledHeight <= 16 * 1024 * 1024) {
        const int width = int(scaledWidth), height = int(scaledHeight);
        result = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent); result.setDevicePixelRatio(ratio);
        auto *surface = cairo_image_surface_create_for_data(result.bits(), CAIRO_FORMAT_ARGB32, width, height, result.bytesPerLine());
        auto *cr = cairo_create(surface);
        cairo_scale(cr, ratio, ratio);
        cairo_set_source_rgba(cr, color.redF(), color.greenF(), color.blueF(), color.alphaF());
        cairo_move_to(cr, -bounds.x(), -bounds.y());
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        cairo_destroy(cr); cairo_surface_destroy(surface);
    }
    g_object_unref(layout); g_object_unref(context);
    return result;
}
QAction *matchingAction(const QWidget *widget, const QStyleOptionMenuItem *option, bool geometry) {
    if (!widget) return nullptr;
    if (geometry) {
        if (auto *menu = qobject_cast<const QMenu *>(widget)) return menu->actionAt(option->rect.center());
        if (auto *bar = qobject_cast<const QMenuBar *>(widget)) return bar->actionAt(option->rect.center());
    }
    const QString label = option->text.section(QLatin1Char('\t'), 0, 0);
    for (auto *action : widget->actions())
        if (action->text() == label && !action->property(markupProperty).toString().isEmpty()) return action;
    return nullptr;
}
class MarkupStyle final : public QProxyStyle {
public:
    explicit MarkupStyle(QWidget *widget) : QProxyStyle(QStyleFactory::create(widget->style()->objectName())) { setParent(widget); }
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contents, const QWidget *widget) const override {
        auto result = QProxyStyle::sizeFromContents(type, option, contents, widget);
        if (type != CT_MenuItem && type != CT_MenuBarItem) return result;
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        auto *action = item ? matchingAction(widget, item, false) : nullptr;
        if (!action) return result;
        const auto raster = image(action->property(markupProperty).toString(), item->font, Qt::black, widget->logicalDpiY(), 1, false);
        if (raster.isNull()) return result;
        const QString plain = item->text.section(QLatin1Char('\t'), 0, 0).remove(QLatin1Char('&'));
        result.rwidth() += qMax(0, raster.width() - item->fontMetrics.horizontalAdvance(plain));
        result.rheight() += qMax(0, raster.height() - item->fontMetrics.height());
        return result;
    }
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override {
        if (element != CE_MenuItem && element != CE_MenuBarItem) { QProxyStyle::drawControl(element, option, painter, widget); return; }
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        auto *action = item ? matchingAction(widget, item, true) : nullptr;
        const QString markup = action ? action->property(markupProperty).toString() : QString();
        if (markup.isEmpty()) { QProxyStyle::drawControl(element, option, painter, widget); return; }
        const auto group = item->state & State_Enabled ? QPalette::Active : QPalette::Disabled;
        const auto role = item->state & State_Selected ? QPalette::HighlightedText : QPalette::Text;
        const auto raster = image(markup, item->font, item->palette.color(group, role), widget->logicalDpiY(),
                                  painter->device()->devicePixelRatioF(), styleHint(SH_UnderlineShortcut, item, widget));
        if (raster.isNull()) { QProxyStyle::drawControl(element, option, painter, widget); return; }
        QStyleOptionMenuItem background(*item);
        const QString shortcut = item->text.section(QLatin1Char('\t'), 1);
        background.text = shortcut.isEmpty() ? QString() : QLatin1Char('\t') + shortcut;
        QProxyStyle::drawControl(element, &background, painter, widget);
        QRect area = item->rect;
        if (element == CE_MenuItem) {
            const int column = qMax(item->maxIconWidth, item->menuHasCheckableItems ? pixelMetric(PM_SmallIconSize, item, widget) : 0);
            area.adjust(column + 12, 0, -(shortcut.isEmpty() ? 12 : item->fontMetrics.horizontalAdvance(shortcut) + 24), 0);
            area = visualRect(item->direction, item->rect, area);
        }
        const QSizeF size(raster.width() / raster.devicePixelRatio(), raster.height() / raster.devicePixelRatio());
        const qreal x = element == CE_MenuBarItem ? area.center().x() - size.width() / 2 :
                        item->direction == Qt::RightToLeft ? area.right() - size.width() : area.left();
        painter->save(); painter->setClipRect(area);
        painter->drawImage(QPointF(x, area.center().y() - size.height() / 2), raster); painter->restore();
    }
};
}
QString plainMenuMarkup(const QString &markup, bool *valid) {
    Parsed parsed(markup, 0);
    if (valid) *valid = parsed.valid;
    return parsed.valid ? QString::fromUtf8(parsed.text) : markup;
}
QImage renderMenuMarkup(const QString &markup, const QFont &font, const QColor &color, qreal dpi, qreal ratio, bool mnemonics) {
    if (dpi <= 0 || ratio <= 0 || !std::isfinite(dpi) || !std::isfinite(ratio)) return {};
    return image(markup, font, color, dpi, ratio, mnemonics);
}
void setMenuMarkup(QAction *action, const QString &markup, QWidget *container) {
    bool valid = false;
    plainMenuMarkup(markup, &valid);
    if (!valid || markup.isEmpty()) return;
    action->setProperty(markupProperty, markup);
    if (!container->property("zoitechatMarkupStyle").toBool()) {
        container->setProperty("zoitechatMarkupStyle", true);
        container->setStyle(new MarkupStyle(container));
        if (auto *bar = qobject_cast<QMenuBar *>(container)) bar->setNativeMenuBar(false);
    }
}
