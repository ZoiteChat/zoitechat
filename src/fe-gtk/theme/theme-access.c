#include "theme-access.h"

#include "theme-runtime.h"
#include "theme-gtk3.h"



enum
{
	THEME_XTEXT_FG_INDEX = 34,
	THEME_XTEXT_BG_INDEX = 35
};

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
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &out_map->text_background);
	gtk_style_context_get_color (context, GTK_STATE_FLAG_SELECTED, &out_map->selection_foreground);
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_SELECTED, &out_map->selection_background);
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

	if (mirc_index >= 32)
		return FALSE;
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

	if (mirc_index >= 32)
		return FALSE;
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

	if (!palette)
		return;

	theme_get_widget_style_values_for_widget (widget, &style_values);
	theme_runtime_get_xtext_colors (palette, palette_len);
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
