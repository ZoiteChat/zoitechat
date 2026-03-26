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
#include "theme-gtk3.h"



enum
{
	THEME_XTEXT_MIRC_COLS = 99,
	THEME_XTEXT_MARK_FG_INDEX = 99,
	THEME_XTEXT_MARK_BG_INDEX = 100,
	THEME_XTEXT_FG_INDEX = 101,
	THEME_XTEXT_BG_INDEX = 102,
	THEME_XTEXT_MARKER_INDEX = 103
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
theme_access_apply_default_99_palette (XTextColor *palette, size_t palette_len, gboolean apply_base)
{
	size_t i;
	size_t start = apply_base ? 0 : 32;

	if (palette_len == 0)
		return;
	for (i = start; i < THEME_XTEXT_MIRC_COLS && i < palette_len; i++)
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

static gboolean
theme_access_get_gtk_palette_map (GtkWidget *widget, ThemeGtkPaletteMap *out_map)
{
	GtkStyleContext *context;
	GdkRGBA accent;

	g_return_val_if_fail (out_map != NULL, FALSE);
	if (!theme_gtk3_is_active () || widget == NULL)
		return FALSE;

	context = gtk_widget_get_style_context (widget);
	if (context == NULL)
		return FALSE;

	gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &out_map->text_foreground);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &out_map->text_background);
	gtk_style_context_get_color (context, GTK_STATE_FLAG_SELECTED, &out_map->selection_foreground);
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_SELECTED, &out_map->selection_background);
	G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_style_context_get_color (context, GTK_STATE_FLAG_LINK, &accent);
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
	gboolean has_user_colors = theme_runtime_mode_has_user_colors (theme_runtime_is_dark_active ());

	if (mirc_index >= THEME_XTEXT_MIRC_COLS)
		return FALSE;
	if (!has_user_colors || mirc_index >= 32)
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
	gboolean has_user_colors = theme_runtime_mode_has_user_colors (theme_runtime_is_dark_active ());

	if (mirc_index >= THEME_XTEXT_MIRC_COLS)
		return FALSE;
	if (!has_user_colors || mirc_index >= 32)
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
	ThemeWidgetStyleValues style_values;
	gboolean has_user_colors;
	GdkRGBA marker_color;

	if (!palette)
		return;

	theme_get_widget_style_values_for_widget (widget, &style_values);
	theme_runtime_get_xtext_colors (palette, palette_len);
	has_user_colors = theme_runtime_mode_has_user_colors (theme_runtime_is_dark_active ());
	theme_access_apply_default_99_palette (palette, palette_len, !has_user_colors);
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
		if (!theme_runtime_get_color (THEME_TOKEN_MARKER, &marker_color))
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
