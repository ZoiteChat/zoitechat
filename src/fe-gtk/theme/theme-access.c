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

#include "theme-access.h"

#include "theme-runtime.h"



enum
{
	THEME_XTEXT_MIRC_COLS = 99,
	THEME_XTEXT_MARK_FG_INDEX = 99,
	THEME_XTEXT_MARK_BG_INDEX = 100,
	THEME_XTEXT_FG_INDEX = 101,
	THEME_XTEXT_BG_INDEX = 102,
	THEME_XTEXT_MARKER_INDEX = 103,
	THEME_XTEXT_MARKER_LEGACY_INDEX = 36
};

static const guint8 theme_default_99_mirc_colors[THEME_XTEXT_MIRC_COLS][3] = {
	{ 0xff, 0xff, 0xff }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x7f }, { 0x00, 0x93, 0x00 }, { 0xff, 0x00, 0x00 }, { 0x7f, 0x00, 0x00 }, { 0x9c, 0x00, 0x9c }, { 0xfc, 0x7f, 0x00 },
	{ 0xff, 0xff, 0x00 }, { 0x00, 0xfc, 0x00 }, { 0x00, 0x93, 0x93 }, { 0x00, 0xff, 0xff }, { 0x00, 0x00, 0xfc }, { 0xff, 0x00, 0xff }, { 0x7f, 0x7f, 0x7f }, { 0xd2, 0xd2, 0xd2 },
	{ 0x47, 0x00, 0x00 }, { 0x47, 0x21, 0x00 }, { 0x47, 0x47, 0x00 }, { 0x32, 0x47, 0x00 }, { 0x00, 0x47, 0x00 }, { 0x00, 0x47, 0x2c }, { 0x00, 0x47, 0x47 }, { 0x00, 0x2f, 0x47 },
	{ 0x00, 0x00, 0x47 }, { 0x2e, 0x00, 0x47 }, { 0x47, 0x00, 0x47 }, { 0x47, 0x00, 0x2a }, { 0x74, 0x00, 0x00 }, { 0x74, 0x3a, 0x00 }, { 0x74, 0x74, 0x00 }, { 0x51, 0x74, 0x00 },
	{ 0x00, 0x74, 0x00 }, { 0x00, 0x74, 0x49 }, { 0x00, 0x74, 0x74 }, { 0x00, 0x4d, 0x74 }, { 0x00, 0x00, 0x74 }, { 0x4b, 0x00, 0x74 }, { 0x74, 0x00, 0x74 }, { 0x74, 0x00, 0x45 },
	{ 0xb5, 0x00, 0x00 }, { 0xb5, 0x63, 0x00 }, { 0xb5, 0xb5, 0x00 }, { 0x7d, 0xb5, 0x00 }, { 0x00, 0xb5, 0x00 }, { 0x00, 0xb5, 0x71 }, { 0x00, 0xb5, 0xb5 }, { 0x00, 0x75, 0xb5 },
	{ 0x00, 0x00, 0xb5 }, { 0x75, 0x00, 0xb5 }, { 0xb5, 0x00, 0xb5 }, { 0xb5, 0x00, 0x6b }, { 0xff, 0x00, 0x00 }, { 0xff, 0x8c, 0x00 }, { 0xff, 0xff, 0x00 }, { 0xb2, 0xff, 0x00 },
	{ 0x00, 0xff, 0x00 }, { 0x00, 0xff, 0xa0 }, { 0x00, 0xff, 0xff }, { 0x00, 0xa9, 0xff }, { 0x00, 0x00, 0xff }, { 0xa5, 0x00, 0xff }, { 0xff, 0x00, 0xff }, { 0xff, 0x00, 0x98 },
	{ 0xff, 0x59, 0x59 }, { 0xff, 0xb4, 0x59 }, { 0xff, 0xff, 0x71 }, { 0xcf, 0xff, 0x60 }, { 0x6f, 0xff, 0x6f }, { 0x65, 0xff, 0xc9 }, { 0x6d, 0xff, 0xff }, { 0x59, 0xcd, 0xff },
	{ 0x59, 0x59, 0xff }, { 0xc4, 0x59, 0xff }, { 0xff, 0x66, 0xff }, { 0xff, 0x59, 0xbc }, { 0xff, 0x9c, 0x9c }, { 0xff, 0xd3, 0x9c }, { 0xff, 0xff, 0x9c }, { 0xe2, 0xff, 0x9c },
	{ 0x9c, 0xff, 0x9c }, { 0x9c, 0xff, 0xdb }, { 0x9c, 0xff, 0xff }, { 0x9c, 0xe2, 0xff }, { 0x9c, 0x9c, 0xff }, { 0xdc, 0x9c, 0xff }, { 0xff, 0x9c, 0xff }, { 0xff, 0x94, 0xd3 },
	{ 0x00, 0x00, 0x00 }, { 0x13, 0x13, 0x13 }, { 0x28, 0x28, 0x28 }, { 0x36, 0x36, 0x36 }, { 0x4d, 0x4d, 0x4d }, { 0x65, 0x65, 0x65 }, { 0x81, 0x81, 0x81 }, { 0x9f, 0x9f, 0x9f },
	{ 0xbc, 0xbc, 0xbc }, { 0xe2, 0xe2, 0xe2 }, { 0xff, 0xff, 0xff }
};

static void
theme_access_apply_default_99_palette (XTextColor *palette, size_t palette_len)
{
	size_t i;

	if (palette_len == 0)
		return;
	for (i = 32; i < THEME_XTEXT_MIRC_COLS && i < palette_len; i++)
	{
		palette[i].red = theme_default_99_mirc_colors[i][0] / 255.0;
		palette[i].green = theme_default_99_mirc_colors[i][1] / 255.0;
		palette[i].blue = theme_default_99_mirc_colors[i][2] / 255.0;
		palette[i].alpha = 1.0;
	}
}

static gboolean
theme_token_to_rgb16 (ThemeSemanticToken token, guint16 *red, guint16 *green, guint16 *blue)
{
	GdkRGBA color = { 0 };

	g_return_val_if_fail (red != NULL, FALSE);
	g_return_val_if_fail (green != NULL, FALSE);
	g_return_val_if_fail (blue != NULL, FALSE);
	if (!theme_runtime_get_color (token, &color))
		return FALSE;
	theme_palette_color_get_rgb16 (&color, red, green, blue);
	return TRUE;
}

static GtkStateFlags
theme_access_state_with_base (GtkStyleContext *context, GtkStateFlags state)
{
	GtkStateFlags base_state;

	base_state = gtk_style_context_get_state (context);
	base_state &= (GTK_STATE_FLAG_DIR_LTR | GTK_STATE_FLAG_DIR_RTL | GTK_STATE_FLAG_BACKDROP | GTK_STATE_FLAG_FOCUSED);
	return base_state | state;
}

static void
theme_access_context_get_color (GtkStyleContext *context, GtkStateFlags state, GdkRGBA *out_color)
{
	gtk_style_context_save (context);
	gtk_style_context_set_state (context, theme_access_state_with_base (context, state));
	gtk_style_context_get_color (context, gtk_style_context_get_state (context), out_color);
	gtk_style_context_restore (context);
}

static void
theme_access_context_get_background_color (GtkStyleContext *context, GtkStateFlags state, GdkRGBA *out_color)
{
	gtk_style_context_save (context);
	gtk_style_context_set_state (context, theme_access_state_with_base (context, state));
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_style_context_get_background_color (context, gtk_style_context_get_state (context), out_color);
	G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_style_context_restore (context);
}

static gboolean
theme_access_lookup_named_color (GtkStyleContext *context, const char *const *names, GdkRGBA *out_color)
{
	size_t i;

	for (i = 0; names[i] != NULL; i++)
	{
		if (gtk_style_context_lookup_color (context, names[i], out_color))
			return TRUE;
	}

	return FALSE;
}

static void
theme_access_resolve_map_background (GtkStyleContext *context, GtkStateFlags state,
				     const char *const *named_fallbacks, GdkRGBA *out_color)
{
	GdkRGBA named;

	theme_access_context_get_background_color (context, state, out_color);
	if (out_color->alpha > 0.0)
		return;

	/* Unstyled CSS nodes (e.g. the custom chat text widget) report a fully
	 * transparent background; fall back to the theme's named colors so the
	 * mapped palette still follows the active GTK3 theme. */
	if (theme_access_lookup_named_color (context, named_fallbacks, &named))
		*out_color = named;
}

static gboolean
theme_access_get_gtk_palette_map (GtkWidget *widget, ThemeGtkPaletteMap *out_map)
{
	static const char *const base_fallbacks[] = { "theme_base_color", "theme_bg_color", NULL };
	static const char *const selected_bg_fallbacks[] = { "theme_selected_bg_color", NULL };
	static const char *const selected_fg_fallbacks[] = { "theme_selected_fg_color", NULL };
	GtkStyleContext *context;
	GdkRGBA named;
	GdkRGBA accent;

	g_return_val_if_fail (out_map != NULL, FALSE);
	if (widget == NULL || !GTK_IS_WIDGET (widget))
		return FALSE;

	context = gtk_widget_get_style_context (widget);
	if (context == NULL)
		return FALSE;

	theme_access_context_get_color (context, GTK_STATE_FLAG_NORMAL, &out_map->text_foreground);
	theme_access_resolve_map_background (context, GTK_STATE_FLAG_NORMAL, base_fallbacks,
					     &out_map->text_background);
	theme_access_context_get_color (context, GTK_STATE_FLAG_SELECTED, &out_map->selection_foreground);
	theme_access_resolve_map_background (context, GTK_STATE_FLAG_SELECTED, selected_bg_fallbacks,
					     &out_map->selection_background);
	if (gdk_rgba_equal (&out_map->selection_foreground, &out_map->text_foreground) &&
	    theme_access_lookup_named_color (context, selected_fg_fallbacks, &named))
		out_map->selection_foreground = named;
	theme_access_context_get_color (context, GTK_STATE_FLAG_LINK, &accent);
	if (accent.alpha <= 0.0)
		accent = out_map->selection_background;
	out_map->accent = accent;
	out_map->enabled = TRUE;
	return TRUE;
}

gboolean
theme_get_color (ThemeSemanticToken token, GdkRGBA *out_rgba)
{
	return theme_runtime_get_color (token, out_rgba);
}

gboolean
theme_get_mirc_color (unsigned int mirc_index, GdkRGBA *out_rgba)
{
	ThemeSemanticToken token = (ThemeSemanticToken) (THEME_TOKEN_MIRC_0 + (int) mirc_index);

	if (mirc_index >= THEME_XTEXT_MIRC_COLS)
		return FALSE;
	if (mirc_index >= 32)
	{
		out_rgba->red = theme_default_99_mirc_colors[mirc_index][0] / 255.0;
		out_rgba->green = theme_default_99_mirc_colors[mirc_index][1] / 255.0;
		out_rgba->blue = theme_default_99_mirc_colors[mirc_index][2] / 255.0;
		out_rgba->alpha = 1.0;
		return TRUE;
	}
	return theme_runtime_get_color (token, out_rgba);
}

gboolean
theme_get_color_rgb16 (ThemeSemanticToken token, guint16 *red, guint16 *green, guint16 *blue)
{
	return theme_token_to_rgb16 (token, red, green, blue);
}

gboolean
theme_get_mirc_color_rgb16 (unsigned int mirc_index, guint16 *red, guint16 *green, guint16 *blue)
{
	ThemeSemanticToken token = (ThemeSemanticToken) (THEME_TOKEN_MIRC_0 + (int) mirc_index);

	if (mirc_index >= THEME_XTEXT_MIRC_COLS)
		return FALSE;
	if (mirc_index >= 32)
	{
		*red = (guint16) (theme_default_99_mirc_colors[mirc_index][0] * 257);
		*green = (guint16) (theme_default_99_mirc_colors[mirc_index][1] * 257);
		*blue = (guint16) (theme_default_99_mirc_colors[mirc_index][2] * 257);
		return TRUE;
	}
	return theme_token_to_rgb16 (token, red, green, blue);
}

gboolean
theme_get_legacy_color (int legacy_idx, GdkRGBA *out_rgba)
{
	ThemeSemanticToken token;

	g_return_val_if_fail (out_rgba != NULL, FALSE);
	if (!theme_palette_legacy_index_to_token (legacy_idx, &token))
		return FALSE;
	return theme_runtime_get_color (token, out_rgba);
}

void
theme_get_widget_style_values (ThemeWidgetStyleValues *out_values)
{
	theme_get_widget_style_values_for_widget (NULL, out_values);
}

void
theme_get_widget_style_values_for_widget (GtkWidget *widget, ThemeWidgetStyleValues *out_values)
{
	ThemeGtkPaletteMap gtk_map = { 0 };

	if (theme_access_get_gtk_palette_map (widget, &gtk_map))
	{
		theme_runtime_get_widget_style_values_mapped (&gtk_map, out_values);
		return;
	}
	theme_runtime_get_widget_style_values (out_values);
}

void
theme_get_xtext_colors (XTextColor *palette, size_t palette_len)
{
	theme_get_xtext_colors_for_widget (NULL, palette, palette_len);
}

void
theme_get_xtext_colors_for_widget (GtkWidget *widget, XTextColor *palette, size_t palette_len)
{
	ThemeGtkPaletteMap gtk_map = { 0 };
	ThemeWidgetStyleValues style_values;
	GdkRGBA marker_color;
	gboolean have_marker = FALSE;

	if (!palette)
		return;

	if (theme_access_get_gtk_palette_map (widget, &gtk_map))
	{
		theme_runtime_get_widget_style_values_mapped (&gtk_map, &style_values);
		theme_runtime_get_xtext_colors_mapped (&gtk_map, palette, palette_len);
	}
	else
	{
		theme_runtime_get_widget_style_values (&style_values);
		theme_runtime_get_xtext_colors (palette, palette_len);
	}
	if (palette_len > THEME_XTEXT_MARKER_LEGACY_INDEX)
	{
		/* The marker token lives at its legacy slot before the extended
		 * mIRC palette overwrites it; keep the (possibly theme-mapped)
		 * value for the dedicated marker index below. */
		marker_color.red = palette[THEME_XTEXT_MARKER_LEGACY_INDEX].red;
		marker_color.green = palette[THEME_XTEXT_MARKER_LEGACY_INDEX].green;
		marker_color.blue = palette[THEME_XTEXT_MARKER_LEGACY_INDEX].blue;
		marker_color.alpha = palette[THEME_XTEXT_MARKER_LEGACY_INDEX].alpha;
		have_marker = TRUE;
	}
	if (palette_len >= THEME_XTEXT_MIRC_COLS)
		theme_access_apply_default_99_palette (palette, palette_len);
	if (palette_len > THEME_XTEXT_MARK_FG_INDEX)
	{
		palette[THEME_XTEXT_MARK_FG_INDEX].red = style_values.selection_foreground.red;
		palette[THEME_XTEXT_MARK_FG_INDEX].green = style_values.selection_foreground.green;
		palette[THEME_XTEXT_MARK_FG_INDEX].blue = style_values.selection_foreground.blue;
		palette[THEME_XTEXT_MARK_FG_INDEX].alpha = style_values.selection_foreground.alpha;
	}
	if (palette_len > THEME_XTEXT_MARK_BG_INDEX)
	{
		palette[THEME_XTEXT_MARK_BG_INDEX].red = style_values.selection_background.red;
		palette[THEME_XTEXT_MARK_BG_INDEX].green = style_values.selection_background.green;
		palette[THEME_XTEXT_MARK_BG_INDEX].blue = style_values.selection_background.blue;
		palette[THEME_XTEXT_MARK_BG_INDEX].alpha = style_values.selection_background.alpha;
	}
	if (palette_len > THEME_XTEXT_MARKER_INDEX)
	{
		if (!have_marker && !theme_runtime_get_color (THEME_TOKEN_MARKER, &marker_color))
			marker_color = style_values.selection_background;
		palette[THEME_XTEXT_MARKER_INDEX].red = marker_color.red;
		palette[THEME_XTEXT_MARKER_INDEX].green = marker_color.green;
		palette[THEME_XTEXT_MARKER_INDEX].blue = marker_color.blue;
		palette[THEME_XTEXT_MARKER_INDEX].alpha = marker_color.alpha;
	}
	if (palette_len > THEME_XTEXT_FG_INDEX)
	{
		palette[THEME_XTEXT_FG_INDEX].red = style_values.foreground.red;
		palette[THEME_XTEXT_FG_INDEX].green = style_values.foreground.green;
		palette[THEME_XTEXT_FG_INDEX].blue = style_values.foreground.blue;
		palette[THEME_XTEXT_FG_INDEX].alpha = style_values.foreground.alpha;
	}
	if (palette_len > THEME_XTEXT_BG_INDEX)
	{
		palette[THEME_XTEXT_BG_INDEX].red = style_values.background.red;
		palette[THEME_XTEXT_BG_INDEX].green = style_values.background.green;
		palette[THEME_XTEXT_BG_INDEX].blue = style_values.background.blue;
		palette[THEME_XTEXT_BG_INDEX].alpha = style_values.background.alpha;
	}
}
