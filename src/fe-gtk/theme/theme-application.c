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

#include "theme-application.h"

#include "../../common/fe.h"
#include "../../common/zoitechatc.h"
#include "theme-css.h"
#include "theme-runtime.h"
#include "theme-gtk3.h"
#include "../maingui.h"

#ifdef G_OS_WIN32
#include <gtk/gtk.h>

static void
theme_application_apply_windows_theme (gboolean dark)
{
	GtkSettings *settings = gtk_settings_get_default ();
	static GtkCssProvider *win_theme_provider = NULL;
	static gboolean win_theme_provider_installed = FALSE;
	GdkScreen *screen;
	gboolean prefer_dark = dark;
	char *css;

	if (theme_gtk3_is_active ())
	{
		if (prefs.hex_gui_gtk3_variant == THEME_GTK3_VARIANT_PREFER_DARK)
			prefer_dark = TRUE;
		else if (prefs.hex_gui_gtk3_variant == THEME_GTK3_VARIANT_PREFER_LIGHT)
			prefer_dark = FALSE;
	}

	if (settings && g_object_class_find_property (G_OBJECT_GET_CLASS (settings),
	                                              "gtk-application-prefer-dark-theme"))
		g_object_set (settings, "gtk-application-prefer-dark-theme", prefer_dark, NULL);

	screen = gdk_screen_get_default ();
	if (!screen)
		return;

	if (theme_gtk3_is_active ())
	{
		if (win_theme_provider_installed && win_theme_provider)
		{
			gtk_style_context_remove_provider_for_screen (screen,
				GTK_STYLE_PROVIDER (win_theme_provider));
			win_theme_provider_installed = FALSE;
		}
		return;
	}

	if (!win_theme_provider)
		win_theme_provider = gtk_css_provider_new ();

	css = theme_css_build_toplevel_classes ();
	gtk_css_provider_load_from_data (win_theme_provider, css, -1, NULL);
	g_free (css);

	if (!win_theme_provider_installed)
	{
		gtk_style_context_add_provider_for_screen (screen,
			GTK_STYLE_PROVIDER (win_theme_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
		win_theme_provider_installed = TRUE;
	}
}
#endif

gboolean
theme_application_apply_mode (unsigned int mode, gboolean *palette_changed)
{
	gboolean dark;

	theme_runtime_load ();
	dark = theme_runtime_apply_mode (mode, palette_changed);

#ifdef G_OS_WIN32
	theme_application_apply_windows_theme (dark);
#endif

	theme_application_reload_input_style ();

	return dark;
}

void
theme_application_reload_input_style (void)
{
	input_style = theme_application_update_input_style (input_style);
}

InputStyle *
theme_application_update_input_style (InputStyle *style)
{
	char buf[256];

	if (!style)
		style = g_new0 (InputStyle, 1);

	if (style->font_desc)
		pango_font_description_free (style->font_desc);
	style->font_desc = pango_font_description_from_string (prefs.hex_text_font);

	if (pango_font_description_get_size (style->font_desc) == 0)
	{
		g_snprintf (buf, sizeof (buf), _("Failed to open font:\n\n%s"), prefs.hex_text_font);
		fe_message (buf, FE_MSG_ERROR);
		pango_font_description_free (style->font_desc);
		style->font_desc = pango_font_description_from_string ("sans 11");
	}

	theme_css_reload_input_style (FALSE, style->font_desc);

	return style;
}
