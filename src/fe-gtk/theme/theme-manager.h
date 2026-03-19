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

#ifndef ZOITECHAT_THEME_MANAGER_H
#define ZOITECHAT_THEME_MANAGER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "theme-palette.h"

typedef struct _GtkWidget GtkWidget;
struct zoitechatprefs;

typedef enum
{
	THEME_CHANGED_REASON_NONE = 0,
	THEME_CHANGED_REASON_PALETTE = 1 << 0,
	THEME_CHANGED_REASON_WIDGET_STYLE = 1 << 1,
	THEME_CHANGED_REASON_MODE = 1 << 2,
	THEME_CHANGED_REASON_THEME_PACK = 1 << 3,
	THEME_CHANGED_REASON_PIXMAP = 1 << 4,
	THEME_CHANGED_REASON_USERLIST = 1 << 5,
	THEME_CHANGED_REASON_LAYOUT = 1 << 6,
	THEME_CHANGED_REASON_IDENTD = 1 << 7
} ThemeChangedReason;

typedef struct
{
	ThemeChangedReason reasons;
} ThemeChangedEvent;

typedef struct
{
	const PangoFontDescription *font_desc;
	gboolean apply_background;
	gboolean apply_foreground;
} ThemePaletteBehavior;

typedef void (*ThemeChangedCallback) (const ThemeChangedEvent *event, gpointer userdata);
typedef guint (*ThemeManagerIdleAddFunc) (GSourceFunc function, gpointer data);

void theme_manager_init (void);
gboolean theme_manager_apply_mode (unsigned int mode, gboolean *palette_changed);
void theme_manager_set_mode (unsigned int mode, gboolean *palette_changed);
void theme_manager_set_token_color (unsigned int mode, ThemeSemanticToken token, const GdkRGBA *color, gboolean *palette_changed);
void theme_manager_reset_mode_colors (unsigned int mode, gboolean *palette_changed);
void theme_manager_commit_preferences (unsigned int old_mode, gboolean *color_change);
gboolean theme_manager_save_preferences (void);
gboolean theme_changed_event_has_reason (const ThemeChangedEvent *event, ThemeChangedReason reason);
void theme_manager_apply_and_dispatch (unsigned int mode, ThemeChangedReason reasons, gboolean *palette_changed);
void theme_manager_dispatch_changed (ThemeChangedReason reasons);
guint theme_listener_register (const char *component_id, ThemeChangedCallback callback, gpointer userdata);
void theme_listener_unregister (guint listener_id);
void theme_manager_handle_theme_applied (void);
void theme_manager_apply_to_window (GtkWidget *window);
void theme_manager_attach_window (GtkWidget *window);
void theme_manager_detach_window (GtkWidget *window);
void theme_manager_apply_palette_widget (GtkWidget *widget, const GdkRGBA *bg, const GdkRGBA *fg,
			       const PangoFontDescription *font_desc);
void theme_manager_apply_entry_palette (GtkWidget *widget, const PangoFontDescription *font_desc);
ThemePaletteBehavior theme_manager_get_userlist_palette_behavior (const PangoFontDescription *font_desc);
ThemePaletteBehavior theme_manager_get_channel_tree_palette_behavior (const PangoFontDescription *font_desc);
void theme_manager_apply_userlist_palette (GtkWidget *widget, const PangoFontDescription *font_desc,
				       gboolean prefer_background, gboolean prefer_foreground);
void theme_manager_apply_userlist_style (GtkWidget *widget, ThemePaletteBehavior behavior);
void theme_manager_apply_channel_tree_style (GtkWidget *widget, ThemePaletteBehavior behavior);
void theme_manager_apply_input_style (gboolean enabled, const PangoFontDescription *font_desc);
void theme_manager_reload_input_style (void);
void theme_manager_refresh_auto_mode (void);
ThemeChangedEvent theme_manager_on_preferences_changed (const struct zoitechatprefs *old_prefs,
						 const struct zoitechatprefs *new_prefs,
						 unsigned int old_mode,
						 gboolean *color_change);
void theme_manager_dispatch_setup_apply (const ThemeChangedEvent *event);
void theme_manager_set_idle_add_func (ThemeManagerIdleAddFunc idle_add_func);

#endif
