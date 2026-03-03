#ifndef ZOITECHAT_THEME_PALETTE_H
#define ZOITECHAT_THEME_PALETTE_H

#include <stddef.h>

#include <gtk/gtk.h>

#include "../xtext-color.h"

typedef enum
{
	THEME_LEGACY_MIRC_0 = 0,
	THEME_LEGACY_MIRC_1,
	THEME_LEGACY_MIRC_2,
	THEME_LEGACY_MIRC_3,
	THEME_LEGACY_MIRC_4,
	THEME_LEGACY_MIRC_5,
	THEME_LEGACY_MIRC_6,
	THEME_LEGACY_MIRC_7,
	THEME_LEGACY_MIRC_8,
	THEME_LEGACY_MIRC_9,
	THEME_LEGACY_MIRC_10,
	THEME_LEGACY_MIRC_11,
	THEME_LEGACY_MIRC_12,
	THEME_LEGACY_MIRC_13,
	THEME_LEGACY_MIRC_14,
	THEME_LEGACY_MIRC_15,
	THEME_LEGACY_MIRC_16,
	THEME_LEGACY_MIRC_17,
	THEME_LEGACY_MIRC_18,
	THEME_LEGACY_MIRC_19,
	THEME_LEGACY_MIRC_20,
	THEME_LEGACY_MIRC_21,
	THEME_LEGACY_MIRC_22,
	THEME_LEGACY_MIRC_23,
	THEME_LEGACY_MIRC_24,
	THEME_LEGACY_MIRC_25,
	THEME_LEGACY_MIRC_26,
	THEME_LEGACY_MIRC_27,
	THEME_LEGACY_MIRC_28,
	THEME_LEGACY_MIRC_29,
	THEME_LEGACY_MIRC_30,
	THEME_LEGACY_MIRC_31,
	THEME_LEGACY_SELECTION_FOREGROUND,
	THEME_LEGACY_SELECTION_BACKGROUND,
	THEME_LEGACY_TEXT_FOREGROUND,
	THEME_LEGACY_TEXT_BACKGROUND,
	THEME_LEGACY_MARKER,
	THEME_LEGACY_TAB_NEW_DATA,
	THEME_LEGACY_TAB_HIGHLIGHT,
	THEME_LEGACY_TAB_NEW_MESSAGE,
	THEME_LEGACY_TAB_AWAY,
	THEME_LEGACY_SPELL,
	THEME_LEGACY_MAX = THEME_LEGACY_SPELL
} ThemeLegacyColorIndex;

typedef enum
{
	THEME_TOKEN_MIRC_0 = 0,
	THEME_TOKEN_MIRC_1,
	THEME_TOKEN_MIRC_2,
	THEME_TOKEN_MIRC_3,
	THEME_TOKEN_MIRC_4,
	THEME_TOKEN_MIRC_5,
	THEME_TOKEN_MIRC_6,
	THEME_TOKEN_MIRC_7,
	THEME_TOKEN_MIRC_8,
	THEME_TOKEN_MIRC_9,
	THEME_TOKEN_MIRC_10,
	THEME_TOKEN_MIRC_11,
	THEME_TOKEN_MIRC_12,
	THEME_TOKEN_MIRC_13,
	THEME_TOKEN_MIRC_14,
	THEME_TOKEN_MIRC_15,
	THEME_TOKEN_MIRC_16,
	THEME_TOKEN_MIRC_17,
	THEME_TOKEN_MIRC_18,
	THEME_TOKEN_MIRC_19,
	THEME_TOKEN_MIRC_20,
	THEME_TOKEN_MIRC_21,
	THEME_TOKEN_MIRC_22,
	THEME_TOKEN_MIRC_23,
	THEME_TOKEN_MIRC_24,
	THEME_TOKEN_MIRC_25,
	THEME_TOKEN_MIRC_26,
	THEME_TOKEN_MIRC_27,
	THEME_TOKEN_MIRC_28,
	THEME_TOKEN_MIRC_29,
	THEME_TOKEN_MIRC_30,
	THEME_TOKEN_MIRC_31,
	THEME_TOKEN_SELECTION_FOREGROUND,
	THEME_TOKEN_SELECTION_BACKGROUND,
	THEME_TOKEN_TEXT_FOREGROUND,
	THEME_TOKEN_TEXT_BACKGROUND,
	THEME_TOKEN_MARKER,
	THEME_TOKEN_TAB_NEW_DATA,
	THEME_TOKEN_TAB_HIGHLIGHT,
	THEME_TOKEN_TAB_NEW_MESSAGE,
	THEME_TOKEN_TAB_AWAY,
	THEME_TOKEN_SPELL,
	THEME_TOKEN_COUNT
} ThemeSemanticToken;

typedef struct
{
	ThemeSemanticToken token;
	int legacy_index;
	const char *name;
} ThemePaletteTokenDef;

typedef struct
{
	GdkRGBA colors[THEME_TOKEN_COUNT];
} ThemePalette;

typedef struct
{
	GdkRGBA foreground;
	GdkRGBA background;
	GdkRGBA selection_foreground;
	GdkRGBA selection_background;
	char foreground_css[32];
	char background_css[32];
	char selection_foreground_css[32];
	char selection_background_css[32];
} ThemeWidgetStyleValues;

size_t theme_palette_token_count (void);
size_t theme_palette_token_def_count (void);
const ThemePaletteTokenDef *theme_palette_token_def_at (size_t index);
const ThemePaletteTokenDef *theme_palette_token_def_for_token (ThemeSemanticToken token);
const char *theme_palette_token_name (ThemeSemanticToken token);
gboolean theme_palette_token_to_legacy_index (ThemeSemanticToken token, int *legacy_idx);
gboolean theme_palette_legacy_index_to_token (int legacy_idx, ThemeSemanticToken *token);
gboolean theme_palette_set_color (ThemePalette *palette, ThemeSemanticToken token, const GdkRGBA *color);
gboolean theme_palette_get_color (const ThemePalette *palette, ThemeSemanticToken token, GdkRGBA *color);
void theme_palette_from_legacy_colors (ThemePalette *palette, const GdkRGBA *legacy_colors, size_t legacy_len);
void theme_palette_to_legacy_colors (const ThemePalette *palette, GdkRGBA *legacy_colors, size_t legacy_len);
void theme_palette_to_xtext_colors (const ThemePalette *palette, XTextColor *xtext_colors, size_t xtext_len);
void theme_palette_to_widget_style_values (const ThemePalette *palette, ThemeWidgetStyleValues *style_values);
void theme_palette_color_get_rgb16 (const GdkRGBA *color, guint16 *red, guint16 *green, guint16 *blue);

#endif
