/* ZoiteChat
 * Copyright (C) 1998-2010 Peter Zelezny.
 * Copyright (C) 2009-2013 Berke Viktor.
 * Copyright (C) 2026 deepend-tildeclub.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

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
gboolean theme_runtime_save (void);
gboolean theme_runtime_save_prepare (char **temp_path);
gboolean theme_runtime_save_finalize (const char *temp_path);
void theme_runtime_save_discard (const char *temp_path);
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
