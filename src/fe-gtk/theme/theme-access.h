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
void theme_get_widget_style_values_for_widget (GtkWidget *widget, ThemeWidgetStyleValues *out_values);
void theme_get_xtext_colors (XTextColor *palette, size_t palette_len);
void theme_get_xtext_colors_for_widget (GtkWidget *widget, XTextColor *palette, size_t palette_len);

#endif
