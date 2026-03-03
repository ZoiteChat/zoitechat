#ifndef ZOITECHAT_THEME_ACCESS_H
#define ZOITECHAT_THEME_ACCESS_H

#include <stddef.h>

#include <gtk/gtk.h>

#include "theme-palette.h"
#include "../xtext-color.h"

gboolean theme_get_color (ThemeSemanticToken token, GdkRGBA *out_rgba);
gboolean theme_get_mirc_color (unsigned int mirc_index, GdkRGBA *out_rgba);
gboolean theme_get_color_rgb16 (ThemeSemanticToken token, guint16 *red, guint16 *green, guint16 *blue);
gboolean theme_get_mirc_color_rgb16 (unsigned int mirc_index, guint16 *red, guint16 *green, guint16 *blue);
G_DEPRECATED_FOR (theme_get_color)
gboolean theme_get_legacy_color (int legacy_idx, GdkRGBA *out_rgba);
void theme_get_widget_style_values (ThemeWidgetStyleValues *out_values);
void theme_get_xtext_colors (XTextColor *palette, size_t palette_len);

#endif
