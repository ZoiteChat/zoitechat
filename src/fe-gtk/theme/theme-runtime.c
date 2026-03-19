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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "theme-runtime.h"
#include "theme-policy.h"

#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"
#include "../../common/util.h"
#include "../../common/cfgfiles.h"
#include "../../common/typedef.h"

#define PALETTE_COLOR_INIT(r, g, b) { (r) / 65535.0, (g) / 65535.0, (b) / 65535.0, 1.0 }

static const GdkRGBA legacy_light_defaults[THEME_LEGACY_MAX + 1] = {
	PALETTE_COLOR_INIT (0xd3d3, 0xd7d7, 0xcfcf),
	PALETTE_COLOR_INIT (0x2e2e, 0x3434, 0x3636),
	PALETTE_COLOR_INIT (0x3434, 0x6565, 0xa4a4),
	PALETTE_COLOR_INIT (0x4e4e, 0x9a9a, 0x0606),
	PALETTE_COLOR_INIT (0xcccc, 0x0000, 0x0000),
	PALETTE_COLOR_INIT (0x8f8f, 0x3939, 0x0202),
	PALETTE_COLOR_INIT (0x5c5c, 0x3535, 0x6666),
	PALETTE_COLOR_INIT (0xcece, 0x5c5c, 0x0000),
	PALETTE_COLOR_INIT (0xc4c4, 0xa0a0, 0x0000),
	PALETTE_COLOR_INIT (0x7373, 0xd2d2, 0x1616),
	PALETTE_COLOR_INIT (0x1111, 0xa8a8, 0x7979),
	PALETTE_COLOR_INIT (0x5858, 0xa1a1, 0x9d9d),
	PALETTE_COLOR_INIT (0x5757, 0x7979, 0x9e9e),
	PALETTE_COLOR_INIT (0xa0d0, 0x42d4, 0x6562),
	PALETTE_COLOR_INIT (0x5555, 0x5757, 0x5353),
	PALETTE_COLOR_INIT (0x8888, 0x8a8a, 0x8585),
	PALETTE_COLOR_INIT (0xd3d3, 0xd7d7, 0xcfcf),
	PALETTE_COLOR_INIT (0x2e2e, 0x3434, 0x3636),
	PALETTE_COLOR_INIT (0x3434, 0x6565, 0xa4a4),
	PALETTE_COLOR_INIT (0x4e4e, 0x9a9a, 0x0606),
	PALETTE_COLOR_INIT (0xcccc, 0x0000, 0x0000),
	PALETTE_COLOR_INIT (0x8f8f, 0x3939, 0x0202),
	PALETTE_COLOR_INIT (0x5c5c, 0x3535, 0x6666),
	PALETTE_COLOR_INIT (0xcece, 0x5c5c, 0x0000),
	PALETTE_COLOR_INIT (0xc4c4, 0xa0a0, 0x0000),
	PALETTE_COLOR_INIT (0x7373, 0xd2d2, 0x1616),
	PALETTE_COLOR_INIT (0x1111, 0xa8a8, 0x7979),
	PALETTE_COLOR_INIT (0x5858, 0xa1a1, 0x9d9d),
	PALETTE_COLOR_INIT (0x5757, 0x7979, 0x9e9e),
	PALETTE_COLOR_INIT (0xa0d0, 0x42d4, 0x6562),
	PALETTE_COLOR_INIT (0x5555, 0x5757, 0x5353),
	PALETTE_COLOR_INIT (0x8888, 0x8a8a, 0x8585),
	PALETTE_COLOR_INIT (0xd3d3, 0xd7d7, 0xcfcf),
	PALETTE_COLOR_INIT (0x2020, 0x4a4a, 0x8787),
	PALETTE_COLOR_INIT (0x2512, 0x29e8, 0x2b85),
	PALETTE_COLOR_INIT (0xfae0, 0xfae0, 0xf8c4),
	PALETTE_COLOR_INIT (0x8f8f, 0x3939, 0x0202),
	PALETTE_COLOR_INIT (0x3434, 0x6565, 0xa4a4),
	PALETTE_COLOR_INIT (0x4e4e, 0x9a9a, 0x0606),
	PALETTE_COLOR_INIT (0xcece, 0x5c5c, 0x0000),
	PALETTE_COLOR_INIT (0x8888, 0x8a8a, 0x8585),
	PALETTE_COLOR_INIT (0xa4a4, 0x0000, 0x0000),
};

static const GdkRGBA legacy_dark_defaults[THEME_LEGACY_MAX + 1] = {
	PALETTE_COLOR_INIT (0xe5e5, 0xe5e5, 0xe5e5), PALETTE_COLOR_INIT (0x3c3c, 0x3c3c, 0x3c3c),
	PALETTE_COLOR_INIT (0x5656, 0x9c9c, 0xd6d6), PALETTE_COLOR_INIT (0x0d0d, 0xbcbc, 0x7979),
	PALETTE_COLOR_INIT (0xf4f4, 0x4747, 0x4747), PALETTE_COLOR_INIT (0xcece, 0x9191, 0x7878),
	PALETTE_COLOR_INIT (0xc5c5, 0x8686, 0xc0c0), PALETTE_COLOR_INIT (0xd7d7, 0xbaba, 0x7d7d),
	PALETTE_COLOR_INIT (0xdcdc, 0xdcdc, 0xaaaa), PALETTE_COLOR_INIT (0xb5b5, 0xcece, 0xa8a8),
	PALETTE_COLOR_INIT (0x4e4e, 0xc9c9, 0xb0b0), PALETTE_COLOR_INIT (0x9c9c, 0xdcdc, 0xfefe),
	PALETTE_COLOR_INIT (0x3737, 0x9494, 0xffff), PALETTE_COLOR_INIT (0xd6d6, 0x7070, 0xd6d6),
	PALETTE_COLOR_INIT (0x8080, 0x8080, 0x8080), PALETTE_COLOR_INIT (0xc0c0, 0xc0c0, 0xc0c0),
	PALETTE_COLOR_INIT (0xe5e5, 0xe5e5, 0xe5e5), PALETTE_COLOR_INIT (0x3c3c, 0x3c3c, 0x3c3c),
	PALETTE_COLOR_INIT (0x5656, 0x9c9c, 0xd6d6), PALETTE_COLOR_INIT (0x0d0d, 0xbcbc, 0x7979),
	PALETTE_COLOR_INIT (0xf4f4, 0x4747, 0x4747), PALETTE_COLOR_INIT (0xcece, 0x9191, 0x7878),
	PALETTE_COLOR_INIT (0xc5c5, 0x8686, 0xc0c0), PALETTE_COLOR_INIT (0xd7d7, 0xbaba, 0x7d7d),
	PALETTE_COLOR_INIT (0xdcdc, 0xdcdc, 0xaaaa), PALETTE_COLOR_INIT (0xb5b5, 0xcece, 0xa8a8),
	PALETTE_COLOR_INIT (0x4e4e, 0xc9c9, 0xb0b0), PALETTE_COLOR_INIT (0x9c9c, 0xdcdc, 0xfefe),
	PALETTE_COLOR_INIT (0x3737, 0x9494, 0xffff), PALETTE_COLOR_INIT (0xd6d6, 0x7070, 0xd6d6),
	PALETTE_COLOR_INIT (0x8080, 0x8080, 0x8080), PALETTE_COLOR_INIT (0xc0c0, 0xc0c0, 0xc0c0),
	PALETTE_COLOR_INIT (0xffff, 0xffff, 0xffff), PALETTE_COLOR_INIT (0x2626, 0x4f4f, 0x7878),
	PALETTE_COLOR_INIT (0xd4d4, 0xd4d4, 0xd4d4), PALETTE_COLOR_INIT (0x1e1e, 0x1e1e, 0x1e1e),
	PALETTE_COLOR_INIT (0x4040, 0x4040, 0x4040), PALETTE_COLOR_INIT (0x3737, 0x9494, 0xffff),
	PALETTE_COLOR_INIT (0xd7d7, 0xbaba, 0x7d7d), PALETTE_COLOR_INIT (0xf4f4, 0x4747, 0x4747),
	PALETTE_COLOR_INIT (0x8080, 0x8080, 0x8080), PALETTE_COLOR_INIT (0xf4f4, 0x4747, 0x4747),
};

static ThemePalette light_palette;
static ThemePalette dark_palette;
static ThemePalette active_palette;
static gboolean user_colors_valid = FALSE;
static gboolean dark_user_colors_valid = FALSE;
static gboolean dark_mode_active = FALSE;
static gboolean light_custom_tokens[THEME_TOKEN_COUNT];
static gboolean dark_custom_tokens[THEME_TOKEN_COUNT];

#define THEME_PALETTE_MIGRATION_MARKER_KEY "theme.palette.semantic_migrated"
#define THEME_PALETTE_MIGRATION_MARKER_VALUE 1

typedef struct
{
	const char *mode_name;
	const char *legacy_prefix;
	ThemePalette *palette;
	gboolean *mode_valid;
} ThemePalettePersistenceMode;

static void
theme_runtime_resolve_color (const GdkRGBA *mapped, const GdkRGBA *fallback, GdkRGBA *resolved)
{
	gdouble alpha;

	g_return_if_fail (mapped != NULL);
	g_return_if_fail (fallback != NULL);
	g_return_if_fail (resolved != NULL);

	alpha = CLAMP (mapped->alpha, 0.0, 1.0);
	resolved->red = (mapped->red * alpha) + (fallback->red * (1.0 - alpha));
	resolved->green = (mapped->green * alpha) + (fallback->green * (1.0 - alpha));
	resolved->blue = (mapped->blue * alpha) + (fallback->blue * (1.0 - alpha));
	resolved->alpha = 1.0;
}

static void
theme_runtime_apply_gtk_map (ThemePalette *palette, const ThemeGtkPaletteMap *gtk_map, const gboolean *custom_tokens)
{
	GdkRGBA text_foreground;
	GdkRGBA text_background;
	GdkRGBA selection_foreground;
	GdkRGBA selection_background;
	GdkRGBA accent;
	GdkRGBA fallback;

	g_return_if_fail (palette != NULL);
	if (gtk_map == NULL || !gtk_map->enabled || custom_tokens == NULL)
		return;

	g_assert (theme_palette_get_color (palette, THEME_TOKEN_TEXT_FOREGROUND, &fallback));
	theme_runtime_resolve_color (&gtk_map->text_foreground, &fallback, &text_foreground);
	g_assert (theme_palette_get_color (palette, THEME_TOKEN_TEXT_BACKGROUND, &fallback));
	theme_runtime_resolve_color (&gtk_map->text_background, &fallback, &text_background);
	g_assert (theme_palette_get_color (palette, THEME_TOKEN_SELECTION_FOREGROUND, &fallback));
	theme_runtime_resolve_color (&gtk_map->selection_foreground, &fallback, &selection_foreground);
	g_assert (theme_palette_get_color (palette, THEME_TOKEN_SELECTION_BACKGROUND, &fallback));
	theme_runtime_resolve_color (&gtk_map->selection_background, &fallback, &selection_background);
	g_assert (theme_palette_get_color (palette, THEME_TOKEN_MARKER, &fallback));
	theme_runtime_resolve_color (&gtk_map->accent, &fallback, &accent);

	if (!custom_tokens[THEME_TOKEN_TEXT_FOREGROUND])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_TEXT_FOREGROUND, &text_foreground));
	if (!custom_tokens[THEME_TOKEN_TEXT_BACKGROUND])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_TEXT_BACKGROUND, &text_background));
	if (!custom_tokens[THEME_TOKEN_SELECTION_FOREGROUND])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_SELECTION_FOREGROUND, &selection_foreground));
	if (!custom_tokens[THEME_TOKEN_SELECTION_BACKGROUND])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_SELECTION_BACKGROUND, &selection_background));
	if (!custom_tokens[THEME_TOKEN_MARKER])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_MARKER, &accent));
	if (!custom_tokens[THEME_TOKEN_TAB_NEW_DATA])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_TAB_NEW_DATA, &accent));
	if (!custom_tokens[THEME_TOKEN_TAB_HIGHLIGHT])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_TAB_HIGHLIGHT, &accent));
	if (!custom_tokens[THEME_TOKEN_TAB_NEW_MESSAGE])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_TAB_NEW_MESSAGE, &accent));
	if (!custom_tokens[THEME_TOKEN_TAB_AWAY])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_TAB_AWAY, &accent));
	if (!custom_tokens[THEME_TOKEN_SPELL])
		g_assert (theme_palette_set_color (palette, THEME_TOKEN_SPELL, &accent));
}

static const gboolean *
theme_runtime_active_custom_tokens (void)
{
	return light_custom_tokens;
}

static void
palette_color_set_rgb16 (GdkRGBA *color, guint16 red, guint16 green, guint16 blue)
{
	char color_string[16];
	GdkRGBA parsed = { 0 };
	gboolean parsed_ok;

	g_snprintf (color_string, sizeof (color_string), "#%04x%04x%04x", red, green, blue);
	parsed_ok = gdk_rgba_parse (&parsed, color_string);
	if (!parsed_ok)
	{
		parsed.red = red / 65535.0;
		parsed.green = green / 65535.0;
		parsed.blue = blue / 65535.0;
		parsed.alpha = 1.0;
	}
	*color = parsed;
}

static void
palette_init_defaults (void)
{
	theme_palette_from_legacy_colors (&light_palette, legacy_light_defaults, G_N_ELEMENTS (legacy_light_defaults));
	theme_palette_from_legacy_colors (&dark_palette, legacy_dark_defaults, G_N_ELEMENTS (legacy_dark_defaults));
	active_palette = light_palette;
	dark_mode_active = FALSE;
}

static int
palette_legacy_index_to_cfg_key (int legacy_idx)
{
	g_return_val_if_fail (legacy_idx >= 0 && legacy_idx <= THEME_LEGACY_MAX, -1);
	if (legacy_idx < 32)
		return legacy_idx;
	return (legacy_idx - 32) + 256;
}

static gboolean
palette_read_token_color (char *cfg, const char *mode_name, const ThemePaletteTokenDef *def, GdkRGBA *out_color)
{
	char prefname[256];
	guint16 red;
	guint16 green;
	guint16 blue;

	g_return_val_if_fail (cfg != NULL, FALSE);
	g_return_val_if_fail (mode_name != NULL, FALSE);
	g_return_val_if_fail (def != NULL, FALSE);
	g_return_val_if_fail (out_color != NULL, FALSE);

	g_snprintf (prefname, sizeof prefname, "theme.mode.%s.token.%s", mode_name, def->name);
	if (!cfg_get_color (cfg, prefname, &red, &green, &blue))
		return FALSE;

	palette_color_set_rgb16 (out_color, red, green, blue);
	return TRUE;
}

static gboolean
palette_read_legacy_color (char *cfg, const char *legacy_prefix, int legacy_index, GdkRGBA *out_color)
{
	char prefname[256];
	guint16 red;
	guint16 green;
	guint16 blue;
	int legacy_key;

	g_return_val_if_fail (cfg != NULL, FALSE);
	g_return_val_if_fail (legacy_prefix != NULL, FALSE);
	g_return_val_if_fail (out_color != NULL, FALSE);

	legacy_key = palette_legacy_index_to_cfg_key (legacy_index);
	g_return_val_if_fail (legacy_key >= 0, FALSE);

	g_snprintf (prefname, sizeof prefname, "%s_%d", legacy_prefix, legacy_key);
	if (!cfg_get_color (cfg, prefname, &red, &green, &blue))
		return FALSE;

	palette_color_set_rgb16 (out_color, red, green, blue);
	return TRUE;
}

static gboolean
theme_runtime_load_migrated_legacy_color (char *cfg,
					const ThemePalettePersistenceMode *mode,
					const ThemePaletteTokenDef *def,
					GdkRGBA *out_color)
{
	g_return_val_if_fail (cfg != NULL, FALSE);
	g_return_val_if_fail (mode != NULL, FALSE);
	g_return_val_if_fail (def != NULL, FALSE);
	g_return_val_if_fail (out_color != NULL, FALSE);

	return palette_read_legacy_color (cfg, mode->legacy_prefix, def->legacy_index, out_color);
}

static gboolean
palette_write_token_color (int fh, const char *mode_name, const ThemePaletteTokenDef *def, const GdkRGBA *color)
{
	char prefname[256];
	guint16 red;
	guint16 green;
	guint16 blue;

	g_return_val_if_fail (mode_name != NULL, FALSE);
	g_return_val_if_fail (def != NULL, FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	g_snprintf (prefname, sizeof prefname, "theme.mode.%s.token.%s", mode_name, def->name);
	theme_palette_color_get_rgb16 (color, &red, &green, &blue);
	return cfg_put_color (fh, red, green, blue, prefname);
}



gboolean
theme_runtime_get_color (ThemeSemanticToken token, GdkRGBA *out_rgba)
{
	g_return_val_if_fail (out_rgba != NULL, FALSE);
	return theme_palette_get_color (&active_palette, token, out_rgba);
}

gboolean
theme_runtime_mode_has_user_colors (gboolean dark_mode)
{
	(void) dark_mode;
	return user_colors_valid;
}

void
theme_runtime_get_widget_style_values (ThemeWidgetStyleValues *out_values)
{
	g_return_if_fail (out_values != NULL);
	theme_palette_to_widget_style_values (&active_palette, out_values);
}

void
theme_runtime_get_widget_style_values_mapped (const ThemeGtkPaletteMap *gtk_map, ThemeWidgetStyleValues *out_values)
{
	ThemePalette mapped_palette;

	g_return_if_fail (out_values != NULL);
	mapped_palette = active_palette;
	theme_runtime_apply_gtk_map (&mapped_palette, gtk_map, theme_runtime_active_custom_tokens ());
	theme_palette_to_widget_style_values (&mapped_palette, out_values);
}

void
theme_runtime_get_xtext_colors (XTextColor *palette, size_t palette_len)
{
	g_return_if_fail (palette != NULL);
	theme_palette_to_xtext_colors (&active_palette, palette, palette_len);
}

void
theme_runtime_get_xtext_colors_mapped (const ThemeGtkPaletteMap *gtk_map, XTextColor *palette, size_t palette_len)
{
	ThemePalette mapped_palette;

	g_return_if_fail (palette != NULL);
	mapped_palette = active_palette;
	theme_runtime_apply_gtk_map (&mapped_palette, gtk_map, theme_runtime_active_custom_tokens ());
	theme_palette_to_xtext_colors (&mapped_palette, palette, palette_len);
}

void
theme_runtime_user_set_color (ThemeSemanticToken token, const GdkRGBA *col)
{
	if (!col)
		return;
	if (token < 0 || token >= THEME_TOKEN_COUNT)
		return;
	if (!user_colors_valid)
		light_palette = active_palette;

	g_assert (theme_palette_set_color (&light_palette, token, col));
	light_custom_tokens[token] = TRUE;
	user_colors_valid = TRUE;
}

void
theme_runtime_dark_set_color (ThemeSemanticToken token, const GdkRGBA *col)
{
	if (!col)
		return;
	if (token < 0 || token >= THEME_TOKEN_COUNT)
		return;
	if (!dark_user_colors_valid)
		dark_palette = active_palette;

	g_assert (theme_palette_set_color (&dark_palette, token, col));
	dark_custom_tokens[token] = TRUE;
	dark_user_colors_valid = TRUE;
}

void
theme_runtime_reset_mode_colors (gboolean dark_mode)
{
	(void) dark_mode;
	theme_palette_from_legacy_colors (&light_palette, legacy_light_defaults, G_N_ELEMENTS (legacy_light_defaults));
	active_palette = light_palette;
	memset (light_custom_tokens, 0, sizeof light_custom_tokens);
	memset (dark_custom_tokens, 0, sizeof dark_custom_tokens);
	user_colors_valid = TRUE;
	dark_user_colors_valid = FALSE;
	dark_mode_active = FALSE;
}

void
theme_runtime_load (void)
{
	size_t i;
	int fh;
	struct stat st;
	char *cfg;
	ThemePalettePersistenceMode modes[] = {
		{ "light", "color", &light_palette, &user_colors_valid },
		{ "dark", "dark_color", &dark_palette, &dark_user_colors_valid },
	};
	const size_t mode_count = G_N_ELEMENTS (modes);

	palette_init_defaults ();
	memset (light_custom_tokens, 0, sizeof light_custom_tokens);
	memset (dark_custom_tokens, 0, sizeof dark_custom_tokens);

	fh = zoitechat_open_file ("colors.conf", O_RDONLY, 0, 0);
	if (fh != -1)
	{
		fstat (fh, &st);
		cfg = g_malloc0 (st.st_size + 1);
		read (fh, cfg, st.st_size);
		for (i = 0; i < mode_count; i++)
		{
			size_t j;
			gboolean mode_found = FALSE;

			for (j = 0; j < theme_palette_token_def_count (); j++)
			{
				const ThemePaletteTokenDef *def = theme_palette_token_def_at (j);
				GdkRGBA color;
				gboolean found;

				g_assert (def != NULL);
				g_assert (theme_palette_get_color (modes[i].palette, def->token, &color));
				found = palette_read_token_color (cfg, modes[i].mode_name, def, &color);
				if (!found)
					found = theme_runtime_load_migrated_legacy_color (cfg, &modes[i], def, &color);
				if (found)
				{
					gboolean *custom_tokens;
					g_assert (theme_palette_set_color (modes[i].palette, def->token, &color));
					custom_tokens = (modes[i].palette == &dark_palette) ? dark_custom_tokens : light_custom_tokens;
					custom_tokens[def->token] = TRUE;
					mode_found = TRUE;
				}
			}

			*modes[i].mode_valid = mode_found;
		}

		g_free (cfg);
		close (fh);
	}

	active_palette = light_palette;
	dark_mode_active = FALSE;
	user_colors_valid = TRUE;
}

static gboolean
theme_runtime_save_to_fd (int fh)
{
	size_t i;
	size_t j;
	ThemePalettePersistenceMode modes[] = {
		{ "light", "color", &light_palette, &user_colors_valid },
		{ "dark", "dark_color", &dark_palette, &dark_user_colors_valid },
	};
	const size_t mode_count = G_N_ELEMENTS (modes);

	if (dark_mode_active && !user_colors_valid)
		light_palette = active_palette;

	if (!dark_mode_active)
		light_palette = active_palette;

	if (dark_mode_active)
	{
		if (!dark_user_colors_valid)
			dark_palette = active_palette;
		dark_user_colors_valid = TRUE;
	}

	user_colors_valid = TRUE;
	modes[0].palette = &light_palette;
	modes[0].mode_valid = &user_colors_valid;

	if (dark_user_colors_valid)
		modes[1].palette = &dark_palette;
	else if (dark_mode_active)
		modes[1].palette = &active_palette;
	else
		modes[1].palette = &dark_palette;

	if (!cfg_put_int (fh, THEME_PALETTE_MIGRATION_MARKER_VALUE, (char *) THEME_PALETTE_MIGRATION_MARKER_KEY))
		return FALSE;

	for (i = 0; i < mode_count; i++)
	{
		if (!*modes[i].mode_valid)
			continue;

		for (j = 0; j < theme_palette_token_def_count (); j++)
		{
			const ThemePaletteTokenDef *def = theme_palette_token_def_at (j);
			const gboolean *custom_tokens;
			GdkRGBA color;

			g_assert (def != NULL);
			custom_tokens = (modes[i].palette == &dark_palette) ? dark_custom_tokens : light_custom_tokens;
			if (!custom_tokens[def->token])
				continue;
			g_assert (theme_palette_get_color (modes[i].palette, def->token, &color));
			if (!palette_write_token_color (fh, modes[i].mode_name, def, &color))
				return FALSE;
		}
	}

	return TRUE;
}

gboolean
theme_runtime_save_prepare (char **temp_path)
{
	int fh;
	char *temp_name;
	const char *config_dir;

	if (!temp_path)
		return FALSE;

	config_dir = get_xdir ();
	if (!config_dir)
		return FALSE;

	temp_name = g_build_filename (config_dir, "colors.conf.new.XXXXXX", NULL);
	fh = g_mkstemp (temp_name);
	if (fh == -1)
	{
		g_free (temp_name);
		return FALSE;
	}

	if (!theme_runtime_save_to_fd (fh))
	{
		close (fh);
		g_unlink (temp_name);
		g_free (temp_name);
		return FALSE;
	}

	if (close (fh) == -1)
	{
		g_unlink (temp_name);
		g_free (temp_name);
		return FALSE;
	}

	*temp_path = temp_name;
	return TRUE;
}

gboolean
theme_runtime_save_finalize (const char *temp_path)
{
	char *config_path;

	if (!temp_path)
		return FALSE;

	config_path = g_build_filename (get_xdir (), "colors.conf", NULL);
#ifdef WIN32
	g_unlink (config_path);
#endif
	if (g_rename (temp_path, config_path) == -1)
	{
		g_free (config_path);
		return FALSE;
	}
	g_free (config_path);

	return TRUE;
}

void
theme_runtime_save_discard (const char *temp_path)
{
	if (!temp_path)
		return;

	g_unlink (temp_path);
}

gboolean
theme_runtime_save (void)
{
	char *temp_path = NULL;
	gboolean result;

	if (!theme_runtime_save_prepare (&temp_path))
		return FALSE;

	result = theme_runtime_save_finalize (temp_path);
	if (!result)
		theme_runtime_save_discard (temp_path);
	g_free (temp_path);

	return result;
}

static gboolean
palette_color_eq (const GdkRGBA *a, const GdkRGBA *b)
{
	guint16 red_a;
	guint16 green_a;
	guint16 blue_a;
	guint16 red_b;
	guint16 green_b;
	guint16 blue_b;

	theme_palette_color_get_rgb16 (a, &red_a, &green_a, &blue_a);
	theme_palette_color_get_rgb16 (b, &red_b, &green_b, &blue_b);

	return red_a == red_b && green_a == green_b && blue_a == blue_b;
}

gboolean
theme_runtime_apply_dark_mode (gboolean enable)
{
	ThemePalette previous_palette;
	size_t i;
	gboolean changed = FALSE;

	previous_palette = active_palette;

	if (!user_colors_valid)
	{
		light_palette = active_palette;
		user_colors_valid = TRUE;
	}

	if (enable)
		active_palette = dark_palette;
	else
		active_palette = light_palette;

	dark_mode_active = enable ? TRUE : FALSE;

	for (i = 0; i < theme_palette_token_def_count (); i++)
	{
		const ThemePaletteTokenDef *def = theme_palette_token_def_at (i);
		GdkRGBA old_color;
		GdkRGBA new_color;

		g_assert (def != NULL);
		g_assert (theme_palette_get_color (&previous_palette, def->token, &old_color));
		g_assert (theme_palette_get_color (&active_palette, def->token, &new_color));
		if (!palette_color_eq (&old_color, &new_color))
		{
			changed = TRUE;
			break;
		}
	}

	return changed;
}

gboolean
theme_runtime_apply_mode (unsigned int mode, gboolean *palette_changed)
{
	gboolean changed;

	(void) mode;
	changed = theme_runtime_apply_dark_mode (FALSE);

	if (palette_changed)
		*palette_changed = changed;

	return FALSE;
}

gboolean
theme_runtime_is_dark_active (void)
{
	return dark_mode_active;
}
