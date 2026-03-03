#ifndef ZOITECHAT_THEME_CSS_H
#define ZOITECHAT_THEME_CSS_H

#include "../fe-gtk.h"

/**
 * theme_css_apply_app_provider/theme_css_remove_app_provider:
 * Use for CSS providers that should apply to the entire application screen.
 *
 * theme_css_apply_widget_provider:
 * Use for widget-local CSS providers attached to a specific widget context.
 */
void theme_css_apply_app_provider (GtkStyleProvider *provider);
void theme_css_remove_app_provider (GtkStyleProvider *provider);
void theme_css_apply_widget_provider (GtkWidget *widget, GtkStyleProvider *provider);
void theme_css_reload_input_style (gboolean enabled, const PangoFontDescription *font_desc);
void theme_css_apply_palette_widget (GtkWidget *widget, const GdkRGBA *bg, const GdkRGBA *fg,
                                     const PangoFontDescription *font_desc);
char *theme_css_build_toplevel_classes (void);

#endif
