#ifndef ZOITECHAT_THEME_RUNTIME_H
#define ZOITECHAT_THEME_RUNTIME_H

#include <stddef.h>

#include <gtk/gtk.h>

#include "theme-palette.h"

void theme_runtime_load (void);
void theme_runtime_save (void);
gboolean theme_runtime_apply_mode (unsigned int mode, gboolean *palette_changed);
gboolean theme_runtime_apply_dark_mode (gboolean enable);
void theme_runtime_user_set_color (ThemeSemanticToken token, const GdkRGBA *col);
void theme_runtime_dark_set_color (ThemeSemanticToken token, const GdkRGBA *col);
gboolean theme_runtime_get_color (ThemeSemanticToken token, GdkRGBA *out_rgba);
void theme_runtime_get_widget_style_values (ThemeWidgetStyleValues *out_values);
void theme_runtime_get_xtext_colors (XTextColor *palette, size_t palette_len);
gboolean theme_runtime_is_dark_active (void);

#endif
