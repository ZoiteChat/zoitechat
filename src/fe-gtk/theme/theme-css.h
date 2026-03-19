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
