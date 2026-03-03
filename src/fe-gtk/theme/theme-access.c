#include "theme-access.h"

#include "theme-runtime.h"


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
	theme_runtime_get_widget_style_values (out_values);
}

void
theme_get_xtext_colors (XTextColor *palette, size_t palette_len)
{
	theme_runtime_get_xtext_colors (palette, palette_len);
}
