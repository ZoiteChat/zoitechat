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

#include "theme-css.h"

#include "theme-runtime.h"
#include "theme-gtk3.h"
#include "theme-access.h"
#include "../gtkutil.h"
#include <string.h>

static const char *theme_css_selector_input = "#zoitechat-inputbox";
static const char *theme_css_selector_input_text = "#zoitechat-inputbox text";
static const char *theme_css_selector_palette_class = "zoitechat-palette";
static const char *theme_css_selector_dark_class = "zoitechat-dark";
static const char *theme_css_selector_light_class = "zoitechat-light";
static const char *theme_css_palette_provider_key = "zoitechat-palette-provider";
static const guint theme_css_provider_priority = GTK_STYLE_PROVIDER_PRIORITY_USER;

typedef struct
{
	char *theme_name;
	char *font;
	gboolean enabled;
	gboolean dark;
	gboolean colors_set;
	guint16 fg_red;
	guint16 fg_green;
	guint16 fg_blue;
	guint16 bg_red;
	guint16 bg_green;
	guint16 bg_blue;
	guint16 sel_fg_red;
	guint16 sel_fg_green;
	guint16 sel_fg_blue;
	guint16 sel_bg_red;
	guint16 sel_bg_green;
	guint16 sel_bg_blue;
} ThemeCssInputFingerprint;

static GtkCssProvider *theme_css_input_provider;
static ThemeCssInputFingerprint theme_css_input_fp;

void
theme_css_apply_app_provider (GtkStyleProvider *provider)
{
	GdkScreen *screen;

	if (!provider)
		return;

	screen = gdk_screen_get_default ();
	if (!screen)
		return;

	gtk_style_context_add_provider_for_screen (screen, provider, theme_css_provider_priority);
}

void
theme_css_remove_app_provider (GtkStyleProvider *provider)
{
	GdkScreen *screen;

	if (!provider)
		return;

	screen = gdk_screen_get_default ();
	if (!screen)
		return;

	gtk_style_context_remove_provider_for_screen (screen, provider);
}

void
theme_css_apply_widget_provider (GtkWidget *widget, GtkStyleProvider *provider)
{
	GtkStyleContext *context;

	if (!widget || !provider || !GTK_IS_WIDGET (widget))
		return;

	context = gtk_widget_get_style_context (widget);
	if (!context)
		return;

	gtk_style_context_add_provider (context, provider, theme_css_provider_priority);
}

static gboolean
theme_css_input_fingerprint_matches (const ThemeCssInputFingerprint *next)
{
	if (theme_css_input_fp.enabled != next->enabled)
		return FALSE;
	if (theme_css_input_fp.dark != next->dark)
		return FALSE;
	if (theme_css_input_fp.colors_set != next->colors_set)
		return FALSE;
	if (theme_css_input_fp.fg_red != next->fg_red || theme_css_input_fp.fg_green != next->fg_green
		|| theme_css_input_fp.fg_blue != next->fg_blue)
		return FALSE;
	if (theme_css_input_fp.bg_red != next->bg_red || theme_css_input_fp.bg_green != next->bg_green
		|| theme_css_input_fp.bg_blue != next->bg_blue)
		return FALSE;
	if (theme_css_input_fp.sel_fg_red != next->sel_fg_red || theme_css_input_fp.sel_fg_green != next->sel_fg_green
		|| theme_css_input_fp.sel_fg_blue != next->sel_fg_blue)
		return FALSE;
	if (theme_css_input_fp.sel_bg_red != next->sel_bg_red || theme_css_input_fp.sel_bg_green != next->sel_bg_green
		|| theme_css_input_fp.sel_bg_blue != next->sel_bg_blue)
		return FALSE;
	if (g_strcmp0 (theme_css_input_fp.theme_name, next->theme_name) != 0)
		return FALSE;
	if (g_strcmp0 (theme_css_input_fp.font, next->font) != 0)
		return FALSE;
	return TRUE;
}

static void
theme_css_input_fingerprint_replace (ThemeCssInputFingerprint *next)
{
	g_free (theme_css_input_fp.theme_name);
	g_free (theme_css_input_fp.font);
	theme_css_input_fp = *next;
	next->theme_name = NULL;
	next->font = NULL;
}

static void
theme_css_input_fingerprint_clear (void)
{
	g_free (theme_css_input_fp.theme_name);
	g_free (theme_css_input_fp.font);
	memset (&theme_css_input_fp, 0, sizeof (theme_css_input_fp));
}

static char *
theme_css_build_input (const char *theme_name, guint16 fg_red, guint16 fg_green, guint16 fg_blue,
					guint16 bg_red, guint16 bg_green, guint16 bg_blue,
					guint16 sel_fg_red, guint16 sel_fg_green, guint16 sel_fg_blue,
					guint16 sel_bg_red, guint16 sel_bg_green, guint16 sel_bg_blue)
{
	GString *css = g_string_new ("");

	if (g_str_has_prefix (theme_name, "Adwaita") || g_str_has_prefix (theme_name, "Yaru"))
	{
		g_string_append_printf (css, "%s { background-image: none; }", theme_css_selector_input);
	}

	g_string_append_printf (css,
		"%s {"
		"background-color: #%02x%02x%02x;"
		"color: #%02x%02x%02x;"
		"caret-color: #%02x%02x%02x;"
		"}"
		"%s {"
		"color: #%02x%02x%02x;"
		"caret-color: #%02x%02x%02x;"
		"}"
		"%s selection, %s text selection, %s:focus selection, %s:focus text selection, %s *:selected, %s *:selected:focus {"
		"background-color: #%02x%02x%02x;"
		"color: #%02x%02x%02x;"
		"text-shadow: none;"
		"}",
		theme_css_selector_input,
		(bg_red >> 8), (bg_green >> 8), (bg_blue >> 8),
		(fg_red >> 8), (fg_green >> 8), (fg_blue >> 8),
		(fg_red >> 8), (fg_green >> 8), (fg_blue >> 8),
		theme_css_selector_input_text,
		(fg_red >> 8), (fg_green >> 8), (fg_blue >> 8),
		(fg_red >> 8), (fg_green >> 8), (fg_blue >> 8),
		theme_css_selector_input,
		theme_css_selector_input,
		theme_css_selector_input,
		theme_css_selector_input,
		theme_css_selector_input,
		theme_css_selector_input,
		(sel_bg_red >> 8), (sel_bg_green >> 8), (sel_bg_blue >> 8),
		(sel_fg_red >> 8), (sel_fg_green >> 8), (sel_fg_blue >> 8));

	return g_string_free (css, FALSE);
}

void
theme_css_reload_input_style (gboolean enabled, const PangoFontDescription *font_desc)
{
	ThemeCssInputFingerprint next = {0};

	next.enabled = enabled;
	next.dark = theme_runtime_is_dark_active ();
	next.theme_name = NULL;
	next.font = font_desc ? pango_font_description_to_string (font_desc) : NULL;

	if (enabled)
	{
		GtkSettings *settings = gtk_settings_get_default ();
		char *theme_name = NULL;
		char *css;

		if (settings)
			g_object_get (settings, "gtk-theme-name", &theme_name, NULL);

		next.theme_name = g_strdup (theme_name);
		{
			ThemeWidgetStyleValues style_values;

			theme_get_widget_style_values_for_widget (NULL, &style_values);
			theme_palette_color_get_rgb16 (&style_values.foreground,
								 &next.fg_red, &next.fg_green, &next.fg_blue);
			theme_palette_color_get_rgb16 (&style_values.background,
								 &next.bg_red, &next.bg_green, &next.bg_blue);
			theme_palette_color_get_rgb16 (&style_values.selection_foreground,
							 &next.sel_fg_red, &next.sel_fg_green, &next.sel_fg_blue);
			theme_palette_color_get_rgb16 (&style_values.selection_background,
							 &next.sel_bg_red, &next.sel_bg_green, &next.sel_bg_blue);
			next.colors_set = TRUE;
		}

		if (theme_css_input_fingerprint_matches (&next))
		{
			g_free (theme_name);
			g_free (next.theme_name);
			g_free (next.font);
			return;
		}

		if (!theme_css_input_provider)
			theme_css_input_provider = gtk_css_provider_new ();

		css = theme_css_build_input (theme_name ? theme_name : "",
			next.fg_red, next.fg_green, next.fg_blue,
			next.bg_red, next.bg_green, next.bg_blue,
			next.sel_fg_red, next.sel_fg_green, next.sel_fg_blue,
			next.sel_bg_red, next.sel_bg_green, next.sel_bg_blue);
		gtk_css_provider_load_from_data (theme_css_input_provider, css, -1, NULL);
		g_free (css);
		theme_css_apply_app_provider (GTK_STYLE_PROVIDER (theme_css_input_provider));

		g_free (theme_name);
		theme_css_input_fingerprint_replace (&next);
		return;
	}

	if (theme_css_input_provider)
		theme_css_remove_app_provider (GTK_STYLE_PROVIDER (theme_css_input_provider));
	g_clear_object (&theme_css_input_provider);
	theme_css_input_fingerprint_clear ();
	g_free (next.theme_name);
	g_free (next.font);
}

void
theme_css_apply_palette_widget (GtkWidget *widget, const GdkRGBA *bg, const GdkRGBA *fg,
						const PangoFontDescription *font_desc)
{
	GtkCssProvider *provider;
	gboolean new_provider = FALSE;
	GString *css;
	gchar *bg_color = NULL;
	gchar *fg_color = NULL;
	gchar *sel_bg_color = NULL;
	gchar *sel_fg_color = NULL;

	if (!widget || !GTK_IS_WIDGET (widget))
		return;

	provider = g_object_get_data (G_OBJECT (widget), theme_css_palette_provider_key);

	if (!bg && !fg && !font_desc)
	{
		gtk_style_context_remove_class (gtk_widget_get_style_context (widget), theme_css_selector_palette_class);
		if (provider)
		{
			gtk_style_context_remove_provider (gtk_widget_get_style_context (widget), GTK_STYLE_PROVIDER (provider));
			g_object_set_data (G_OBJECT (widget), theme_css_palette_provider_key, NULL);
		}
		return;
	}

	if (!provider)
	{
		provider = gtk_css_provider_new ();
		g_object_set_data_full (G_OBJECT (widget), theme_css_palette_provider_key,
						provider, g_object_unref);
		new_provider = TRUE;
	}

	css = g_string_new (".");
	g_string_append (css, theme_css_selector_palette_class);
	g_string_append (css, " {");
	if (bg)
	{
		bg_color = gdk_rgba_to_string (bg);
		g_string_append_printf (css, " background-color: %s;", bg_color);
	}
	if (fg)
	{
		fg_color = gdk_rgba_to_string (fg);
		g_string_append_printf (css, " color: %s;", fg_color);
	}
	{
		GdkRGBA selection_bg;
		GdkRGBA selection_fg;
		if (theme_runtime_get_color (THEME_TOKEN_SELECTION_BACKGROUND, &selection_bg))
			sel_bg_color = gdk_rgba_to_string (&selection_bg);
		if (theme_runtime_get_color (THEME_TOKEN_SELECTION_FOREGROUND, &selection_fg))
			sel_fg_color = gdk_rgba_to_string (&selection_fg);
	}
	gtkutil_append_font_css (css, font_desc);
	g_string_append (css, " }");
	g_string_append_printf (css, ".%s, .%s *, .%s treeview, .%s treeview.view, .%s treeview.view text, .%s treeview.view cell, .%s treeview.view row, .%s list, .%s list row, .%s text {", theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class);
	if (bg)
		g_string_append_printf (css, " background-color: %s;", bg_color);
	if (fg)
		g_string_append_printf (css, " color: %s;", fg_color);
	g_string_append (css, " }");
	g_string_append_printf (css, ".%s *:selected, .%s *:selected:focus, .%s *:selected:hover, .%s treeview.view:selected, .%s treeview.view:selected:focus, .%s treeview.view:selected:hover, .%s row:selected, .%s row:selected:focus, .%s row:selected:hover, .%s selection, .%s text selection, .%s entry selection, .%s entry text selection, .%s:focus selection, .%s:focus text selection {", theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class, theme_css_selector_palette_class);
	if (sel_bg_color)
		g_string_append_printf (css, " background-color: %s;", sel_bg_color);
	else if (bg)
		g_string_append (css, " background-color: @theme_selected_bg_color;");
	if (sel_fg_color)
		g_string_append_printf (css, " color: %s;", sel_fg_color);
	else if (fg)
		g_string_append (css, " color: @theme_selected_fg_color;");
	g_string_append (css, " }");

	gtk_css_provider_load_from_data (provider, css->str, -1, NULL);
	if (new_provider)
		theme_css_apply_widget_provider (widget, GTK_STYLE_PROVIDER (provider));
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), theme_css_selector_palette_class);

	g_string_free (css, TRUE);
	g_free (bg_color);
	g_free (fg_color);
	g_free (sel_bg_color);
	g_free (sel_fg_color);
}

char *
theme_css_build_toplevel_classes (void)
{
	return g_strdup_printf (
		"window.%s, window.%s:backdrop, .%s {"
		"background-color: #202020;"
		"color: #f0f0f0;"
		"border-color: #202020;"
		"}"
		"window.%s menubar, window.%s menubar:backdrop, window.%s menubar box, window.%s menubar box:backdrop, window.%s menuitem, window.%s menuitem:backdrop {"
		"background-color: @theme_bg_color;"
		"background-image: none;"
		"color: @theme_fg_color;"
		"border-color: @theme_bg_color;"
		"}"
		"window.%s menuitem:hover, window.%s menuitem:selected {"
		"background-color: @theme_selected_bg_color;"
		"background-image: none;"
		"color: @theme_selected_fg_color;"
		"border-color: @theme_selected_bg_color;"
		"}"
		"window.%s, window.%s:backdrop, .%s {"
		"background-color: #f6f6f6;"
		"color: #101010;"
		"border-color: #f6f6f6;"
		"}"
		"window.%s menubar, window.%s menubar:backdrop, window.%s menubar box, window.%s menubar box:backdrop, window.%s menuitem, window.%s menuitem:backdrop {"
		"background-color: @theme_bg_color;"
		"background-image: none;"
		"color: @theme_fg_color;"
		"border-color: @theme_bg_color;"
		"}"
		"window.%s menuitem:hover, window.%s menuitem:selected {"
		"background-color: @theme_selected_bg_color;"
		"background-image: none;"
		"color: @theme_selected_fg_color;"
		"border-color: @theme_selected_bg_color;"
		"}",
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_dark_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class,
		theme_css_selector_light_class);
}
