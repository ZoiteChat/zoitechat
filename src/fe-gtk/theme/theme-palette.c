#include <string.h>

#include "theme-palette.h"

static const ThemePaletteTokenDef theme_palette_token_defs[] = {
	{ THEME_TOKEN_MIRC_0, 0, "mirc_0" },
	{ THEME_TOKEN_MIRC_1, 1, "mirc_1" },
	{ THEME_TOKEN_MIRC_2, 2, "mirc_2" },
	{ THEME_TOKEN_MIRC_3, 3, "mirc_3" },
	{ THEME_TOKEN_MIRC_4, 4, "mirc_4" },
	{ THEME_TOKEN_MIRC_5, 5, "mirc_5" },
	{ THEME_TOKEN_MIRC_6, 6, "mirc_6" },
	{ THEME_TOKEN_MIRC_7, 7, "mirc_7" },
	{ THEME_TOKEN_MIRC_8, 8, "mirc_8" },
	{ THEME_TOKEN_MIRC_9, 9, "mirc_9" },
	{ THEME_TOKEN_MIRC_10, 10, "mirc_10" },
	{ THEME_TOKEN_MIRC_11, 11, "mirc_11" },
	{ THEME_TOKEN_MIRC_12, 12, "mirc_12" },
	{ THEME_TOKEN_MIRC_13, 13, "mirc_13" },
	{ THEME_TOKEN_MIRC_14, 14, "mirc_14" },
	{ THEME_TOKEN_MIRC_15, 15, "mirc_15" },
	{ THEME_TOKEN_MIRC_16, 16, "mirc_16" },
	{ THEME_TOKEN_MIRC_17, 17, "mirc_17" },
	{ THEME_TOKEN_MIRC_18, 18, "mirc_18" },
	{ THEME_TOKEN_MIRC_19, 19, "mirc_19" },
	{ THEME_TOKEN_MIRC_20, 20, "mirc_20" },
	{ THEME_TOKEN_MIRC_21, 21, "mirc_21" },
	{ THEME_TOKEN_MIRC_22, 22, "mirc_22" },
	{ THEME_TOKEN_MIRC_23, 23, "mirc_23" },
	{ THEME_TOKEN_MIRC_24, 24, "mirc_24" },
	{ THEME_TOKEN_MIRC_25, 25, "mirc_25" },
	{ THEME_TOKEN_MIRC_26, 26, "mirc_26" },
	{ THEME_TOKEN_MIRC_27, 27, "mirc_27" },
	{ THEME_TOKEN_MIRC_28, 28, "mirc_28" },
	{ THEME_TOKEN_MIRC_29, 29, "mirc_29" },
	{ THEME_TOKEN_MIRC_30, 30, "mirc_30" },
	{ THEME_TOKEN_MIRC_31, 31, "mirc_31" },
	{ THEME_TOKEN_SELECTION_FOREGROUND, 32, "selection_foreground" },
	{ THEME_TOKEN_SELECTION_BACKGROUND, 33, "selection_background" },
	{ THEME_TOKEN_TEXT_FOREGROUND, 34, "text_foreground" },
	{ THEME_TOKEN_TEXT_BACKGROUND, 35, "text_background" },
	{ THEME_TOKEN_MARKER, 36, "marker" },
	{ THEME_TOKEN_TAB_NEW_DATA, 37, "tab_new_data" },
	{ THEME_TOKEN_TAB_HIGHLIGHT, 38, "tab_highlight" },
	{ THEME_TOKEN_TAB_NEW_MESSAGE, 39, "tab_new_message" },
	{ THEME_TOKEN_TAB_AWAY, 40, "tab_away" },
	{ THEME_TOKEN_SPELL, 41, "spell" },
};

static const ThemePaletteTokenDef *
theme_palette_lookup_token_def (ThemeSemanticToken token)
{
	size_t i;

	for (i = 0; i < G_N_ELEMENTS (theme_palette_token_defs); i++)
	{
		if (theme_palette_token_defs[i].token == token)
			return &theme_palette_token_defs[i];
	}

	return NULL;
}

static const ThemePaletteTokenDef *
theme_palette_lookup_legacy_def (int legacy_idx)
{
	size_t i;

	for (i = 0; i < G_N_ELEMENTS (theme_palette_token_defs); i++)
	{
		if (theme_palette_token_defs[i].legacy_index == legacy_idx)
			return &theme_palette_token_defs[i];
	}

	return NULL;
}

size_t
theme_palette_token_count (void)
{
	return THEME_TOKEN_COUNT;
}

size_t
theme_palette_token_def_count (void)
{
	return G_N_ELEMENTS (theme_palette_token_defs);
}

const ThemePaletteTokenDef *
theme_palette_token_def_at (size_t index)
{
	if (index >= G_N_ELEMENTS (theme_palette_token_defs))
		return NULL;

	return &theme_palette_token_defs[index];
}

const ThemePaletteTokenDef *
theme_palette_token_def_for_token (ThemeSemanticToken token)
{
	if (token < 0 || token >= THEME_TOKEN_COUNT)
		return NULL;

	return theme_palette_lookup_token_def (token);
}

const char *
theme_palette_token_name (ThemeSemanticToken token)
{
	const ThemePaletteTokenDef *def = theme_palette_token_def_for_token (token);

	if (def == NULL)
		return NULL;

	return def->name;
}

gboolean
theme_palette_token_to_legacy_index (ThemeSemanticToken token, int *legacy_idx)
{
	const ThemePaletteTokenDef *def;

	if (legacy_idx == NULL)
		return FALSE;

	def = theme_palette_token_def_for_token (token);
	if (def == NULL)
		return FALSE;

	*legacy_idx = def->legacy_index;
	return TRUE;
}

gboolean
theme_palette_legacy_index_to_token (int legacy_idx, ThemeSemanticToken *token)
{
	const ThemePaletteTokenDef *def;

	if (token == NULL)
		return FALSE;

	def = theme_palette_lookup_legacy_def (legacy_idx);
	if (def == NULL)
		return FALSE;

	*token = def->token;
	return TRUE;
}

gboolean
theme_palette_set_color (ThemePalette *palette, ThemeSemanticToken token, const GdkRGBA *color)
{
	if (palette == NULL || color == NULL)
		return FALSE;
	if (token < 0 || token >= THEME_TOKEN_COUNT)
		return FALSE;
	if (theme_palette_token_def_for_token (token) == NULL)
		return FALSE;

	palette->colors[token] = *color;
	return TRUE;
}

gboolean
theme_palette_get_color (const ThemePalette *palette, ThemeSemanticToken token, GdkRGBA *color)
{
	if (palette == NULL || color == NULL)
		return FALSE;
	if (token < 0 || token >= THEME_TOKEN_COUNT)
		return FALSE;
	if (theme_palette_token_def_for_token (token) == NULL)
		return FALSE;

	*color = palette->colors[token];
	return TRUE;
}

void
theme_palette_from_legacy_colors (ThemePalette *palette, const GdkRGBA *legacy_colors, size_t legacy_len)
{
	size_t i;

	g_return_if_fail (palette != NULL);
	g_return_if_fail (legacy_colors != NULL);

	for (i = 0; i < G_N_ELEMENTS (theme_palette_token_defs); i++)
	{
		int legacy_idx = theme_palette_token_defs[i].legacy_index;
		ThemeSemanticToken token = theme_palette_token_defs[i].token;

		g_return_if_fail (legacy_idx >= 0);
		g_return_if_fail ((size_t) legacy_idx < legacy_len);
		palette->colors[token] = legacy_colors[legacy_idx];
	}
}

void
theme_palette_to_legacy_colors (const ThemePalette *palette, GdkRGBA *legacy_colors, size_t legacy_len)
{
	size_t i;

	g_return_if_fail (palette != NULL);
	g_return_if_fail (legacy_colors != NULL);

	for (i = 0; i < G_N_ELEMENTS (theme_palette_token_defs); i++)
	{
		int legacy_idx = theme_palette_token_defs[i].legacy_index;
		ThemeSemanticToken token = theme_palette_token_defs[i].token;

		g_return_if_fail (legacy_idx >= 0);
		g_return_if_fail ((size_t) legacy_idx < legacy_len);
		legacy_colors[legacy_idx] = palette->colors[token];
	}
}

void
theme_palette_to_xtext_colors (const ThemePalette *palette, XTextColor *xtext_colors, size_t xtext_len)
{
	size_t i;

	g_return_if_fail (palette != NULL);
	g_return_if_fail (xtext_colors != NULL);

	for (i = 0; i < G_N_ELEMENTS (theme_palette_token_defs); i++)
	{
		int legacy_idx = theme_palette_token_defs[i].legacy_index;
		ThemeSemanticToken token = theme_palette_token_defs[i].token;

		if ((size_t) legacy_idx >= xtext_len)
			continue;
		xtext_colors[legacy_idx].red = palette->colors[token].red;
		xtext_colors[legacy_idx].green = palette->colors[token].green;
		xtext_colors[legacy_idx].blue = palette->colors[token].blue;
		xtext_colors[legacy_idx].alpha = palette->colors[token].alpha;
	}
}

void
theme_palette_to_widget_style_values (const ThemePalette *palette, ThemeWidgetStyleValues *style_values)
{
	g_return_if_fail (palette != NULL);
	g_return_if_fail (style_values != NULL);

	style_values->foreground = palette->colors[THEME_TOKEN_TEXT_FOREGROUND];
	style_values->background = palette->colors[THEME_TOKEN_TEXT_BACKGROUND];
	style_values->selection_foreground = palette->colors[THEME_TOKEN_SELECTION_FOREGROUND];
	style_values->selection_background = palette->colors[THEME_TOKEN_SELECTION_BACKGROUND];
	g_snprintf (style_values->foreground_css, sizeof (style_values->foreground_css),
	            "rgba(%u,%u,%u,%.3f)",
	            (guint) CLAMP (style_values->foreground.red * 255.0 + 0.5, 0.0, 255.0),
	            (guint) CLAMP (style_values->foreground.green * 255.0 + 0.5, 0.0, 255.0),
	            (guint) CLAMP (style_values->foreground.blue * 255.0 + 0.5, 0.0, 255.0),
	            CLAMP (style_values->foreground.alpha, 0.0, 1.0));
	g_snprintf (style_values->background_css, sizeof (style_values->background_css),
	            "rgba(%u,%u,%u,%.3f)",
	            (guint) CLAMP (style_values->background.red * 255.0 + 0.5, 0.0, 255.0),
	            (guint) CLAMP (style_values->background.green * 255.0 + 0.5, 0.0, 255.0),
	            (guint) CLAMP (style_values->background.blue * 255.0 + 0.5, 0.0, 255.0),
	            CLAMP (style_values->background.alpha, 0.0, 1.0));
	g_snprintf (style_values->selection_foreground_css, sizeof (style_values->selection_foreground_css),
	            "rgba(%u,%u,%u,%.3f)",
	            (guint) CLAMP (style_values->selection_foreground.red * 255.0 + 0.5, 0.0, 255.0),
	            (guint) CLAMP (style_values->selection_foreground.green * 255.0 + 0.5, 0.0, 255.0),
	            (guint) CLAMP (style_values->selection_foreground.blue * 255.0 + 0.5, 0.0, 255.0),
	            CLAMP (style_values->selection_foreground.alpha, 0.0, 1.0));
	g_snprintf (style_values->selection_background_css, sizeof (style_values->selection_background_css),
	            "rgba(%u,%u,%u,%.3f)",
	            (guint) CLAMP (style_values->selection_background.red * 255.0 + 0.5, 0.0, 255.0),
	            (guint) CLAMP (style_values->selection_background.green * 255.0 + 0.5, 0.0, 255.0),
	            (guint) CLAMP (style_values->selection_background.blue * 255.0 + 0.5, 0.0, 255.0),
	            CLAMP (style_values->selection_background.alpha, 0.0, 1.0));
}

void
theme_palette_color_get_rgb16 (const GdkRGBA *color, guint16 *red, guint16 *green, guint16 *blue)
{
	g_return_if_fail (color != NULL);
	g_return_if_fail (red != NULL);
	g_return_if_fail (green != NULL);
	g_return_if_fail (blue != NULL);

	*red = (guint16) CLAMP (color->red * 65535.0 + 0.5, 0.0, 65535.0);
	*green = (guint16) CLAMP (color->green * 65535.0 + 0.5, 0.0, 65535.0);
	*blue = (guint16) CLAMP (color->blue * 65535.0 + 0.5, 0.0, 65535.0);
}
