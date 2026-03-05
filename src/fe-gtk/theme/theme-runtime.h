#ifndef ZOITECHAT_THEME_RUNTIME_H
#define ZOITECHAT_THEME_RUNTIME_H

#include <stddef.h>

#include <gtk/gtk.h>

#include "theme-palette.h"

typedef struct
{
	gboolean enabled;
	GdkRGBA text_foreground;
	GdkRGBA text_background;
	GdkRGBA selection_foreground;
	GdkRGBA selection_background;
	GdkRGBA accent;
} ThemeGtkPaletteMap;

void theme_runtime_load (void);
void theme_runtime_save (void);
gboolean theme_runtime_apply_mode (unsigned int mode, gboolean *palette_changed);
gboolean theme_runtime_apply_dark_mode (gboolean enable);
void theme_runtime_user_set_color (ThemeSemanticToken token, const GdkRGBA *col);
void theme_runtime_dark_set_color (ThemeSemanticToken token, const GdkRGBA *col);
void theme_runtime_reset_mode_colors (gboolean dark_mode);
gboolean theme_runtime_get_color (ThemeSemanticToken token, GdkRGBA *out_rgba);
gboolean theme_runtime_mode_has_user_colors (gboolean dark_mode);
void theme_runtime_get_widget_style_values (ThemeWidgetStyleValues *out_values);
void theme_runtime_get_widget_style_values_mapped (const ThemeGtkPaletteMap *gtk_map, ThemeWidgetStyleValues *out_values);
void theme_runtime_get_xtext_colors (XTextColor *palette, size_t palette_len);
void theme_runtime_get_xtext_colors_mapped (const ThemeGtkPaletteMap *gtk_map, XTextColor *palette, size_t palette_len);
gboolean theme_runtime_is_dark_active (void);

#endif
