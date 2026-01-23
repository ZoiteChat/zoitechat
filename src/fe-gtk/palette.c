/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "fe-gtk.h"
#include "palette.h"

#include "../common/zoitechat.h"
#include "../common/zoitechatc.h" /* prefs */
#include "../common/util.h"
#include "../common/cfgfiles.h"
#include "../common/typedef.h"

#if GTK_CHECK_VERSION(3,0,0)
#define PALETTE_COLOR_INIT(r, g, b) { (r) / 65535.0, (g) / 65535.0, (b) / 65535.0, 1.0 }
#else
#define PALETTE_COLOR_INIT(r, g, b) { 0, (r), (g), (b) }
#endif

static void
palette_color_set_rgb16 (PaletteColor *color, guint16 red, guint16 green, guint16 blue)
{
	char color_string[16];

	g_snprintf (color_string, sizeof (color_string), "#%04x%04x%04x", red, green, blue);
#if GTK_CHECK_VERSION(3,0,0)
	GdkRGBA parsed;
	gboolean parsed_ok;

	parsed_ok = gdk_rgba_parse (&parsed, color_string);
	if (!parsed_ok)
	{
		parsed.red = red / 65535.0;
		parsed.green = green / 65535.0;
		parsed.blue = blue / 65535.0;
		parsed.alpha = 1.0;
	}
	*color = parsed;
#else
	if (!gdk_color_parse (color_string, color))
	{
		color->red = red;
		color->green = green;
		color->blue = blue;
		color->pixel = 0;
	}
#endif
}

static XTextColor
palette_color_from_gdk (const PaletteColor *color)
{
	XTextColor result;

#if GTK_CHECK_VERSION(3,0,0)
	result.red = color->red;
	result.green = color->green;
	result.blue = color->blue;
	result.alpha = color->alpha;
#else
	result.red = color->red / 65535.0;
	result.green = color->green / 65535.0;
	result.blue = color->blue / 65535.0;
	result.alpha = 1.0;
#endif

	return result;
}

PaletteColor colors[] = {
	/* colors for xtext */
	PALETTE_COLOR_INIT (0xd3d3, 0xd7d7, 0xcfcf), /* 0 white */
	PALETTE_COLOR_INIT (0x2e2e, 0x3434, 0x3636), /* 1 black */
	PALETTE_COLOR_INIT (0x3434, 0x6565, 0xa4a4), /* 2 blue */
	PALETTE_COLOR_INIT (0x4e4e, 0x9a9a, 0x0606), /* 3 green */
	PALETTE_COLOR_INIT (0xcccc, 0x0000, 0x0000), /* 4 red */
	PALETTE_COLOR_INIT (0x8f8f, 0x3939, 0x0202), /* 5 light red */
	PALETTE_COLOR_INIT (0x5c5c, 0x3535, 0x6666), /* 6 purple */
	PALETTE_COLOR_INIT (0xcece, 0x5c5c, 0x0000), /* 7 orange */
	PALETTE_COLOR_INIT (0xc4c4, 0xa0a0, 0x0000), /* 8 yellow */
	PALETTE_COLOR_INIT (0x7373, 0xd2d2, 0x1616), /* 9 green */
	PALETTE_COLOR_INIT (0x1111, 0xa8a8, 0x7979), /* 10 aqua */
	PALETTE_COLOR_INIT (0x5858, 0xa1a1, 0x9d9d), /* 11 light aqua */
	PALETTE_COLOR_INIT (0x5757, 0x7979, 0x9e9e), /* 12 blue */
	PALETTE_COLOR_INIT (0xa0d0, 0x42d4, 0x6562), /* 13 light purple */
	PALETTE_COLOR_INIT (0x5555, 0x5757, 0x5353), /* 14 grey */
	PALETTE_COLOR_INIT (0x8888, 0x8a8a, 0x8585), /* 15 light grey */

	PALETTE_COLOR_INIT (0xd3d3, 0xd7d7, 0xcfcf), /* 16 white */
	PALETTE_COLOR_INIT (0x2e2e, 0x3434, 0x3636), /* 17 black */
	PALETTE_COLOR_INIT (0x3434, 0x6565, 0xa4a4), /* 18 blue */
	PALETTE_COLOR_INIT (0x4e4e, 0x9a9a, 0x0606), /* 19 green */
	PALETTE_COLOR_INIT (0xcccc, 0x0000, 0x0000), /* 20 red */
	PALETTE_COLOR_INIT (0x8f8f, 0x3939, 0x0202), /* 21 light red */
	PALETTE_COLOR_INIT (0x5c5c, 0x3535, 0x6666), /* 22 purple */
	PALETTE_COLOR_INIT (0xcece, 0x5c5c, 0x0000), /* 23 orange */
	PALETTE_COLOR_INIT (0xc4c4, 0xa0a0, 0x0000), /* 24 yellow */
	PALETTE_COLOR_INIT (0x7373, 0xd2d2, 0x1616), /* 25 green */
	PALETTE_COLOR_INIT (0x1111, 0xa8a8, 0x7979), /* 26 aqua */
	PALETTE_COLOR_INIT (0x5858, 0xa1a1, 0x9d9d), /* 27 light aqua */
	PALETTE_COLOR_INIT (0x5757, 0x7979, 0x9e9e), /* 28 blue */
	PALETTE_COLOR_INIT (0xa0d0, 0x42d4, 0x6562), /* 29 light purple */
	PALETTE_COLOR_INIT (0x5555, 0x5757, 0x5353), /* 30 grey */
	PALETTE_COLOR_INIT (0x8888, 0x8a8a, 0x8585), /* 31 light grey */

	PALETTE_COLOR_INIT (0xd3d3, 0xd7d7, 0xcfcf), /* 32 marktext Fore (white) */
	PALETTE_COLOR_INIT (0x2020, 0x4a4a, 0x8787), /* 33 marktext Back (blue) */
	PALETTE_COLOR_INIT (0x2512, 0x29e8, 0x2b85), /* 34 foreground (black) */
	PALETTE_COLOR_INIT (0xfae0, 0xfae0, 0xf8c4), /* 35 background (white) */
	PALETTE_COLOR_INIT (0x8f8f, 0x3939, 0x0202), /* 36 marker line (red) */

	/* colors for GUI */
	PALETTE_COLOR_INIT (0x3434, 0x6565, 0xa4a4), /* 37 tab New Data (dark red) */
	PALETTE_COLOR_INIT (0x4e4e, 0x9a9a, 0x0606), /* 38 tab Nick Mentioned (blue) */
	PALETTE_COLOR_INIT (0xcece, 0x5c5c, 0x0000), /* 39 tab New Message (red) */
	PALETTE_COLOR_INIT (0x8888, 0x8a8a, 0x8585), /* 40 away user (grey) */
	PALETTE_COLOR_INIT (0xa4a4, 0x0000, 0x0000), /* 41 spell checker color (red) */
};

/* User palette snapshot (what we write to colors.conf) */
static PaletteColor user_colors[MAX_COL + 1];
static gboolean user_colors_valid = FALSE;

/* Dark palette snapshot (saved separately so dark mode can have its own custom palette). */
static PaletteColor dark_user_colors[MAX_COL + 1];
static gboolean dark_user_colors_valid = FALSE;

/* ZoiteChat's curated dark palette (applies when prefs.hex_gui_dark_mode is enabled). */
static const PaletteColor dark_colors[MAX_COL + 1] = {
	/* mIRC colors 0-15 */
	PALETTE_COLOR_INIT (0xe5e5, 0xe5e5, 0xe5e5), /* 0 white */
	PALETTE_COLOR_INIT (0x3c3c, 0x3c3c, 0x3c3c), /* 1 black (dark gray for contrast) */
	PALETTE_COLOR_INIT (0x5656, 0x9c9c, 0xd6d6), /* 2 blue */
	PALETTE_COLOR_INIT (0x0d0d, 0xbcbc, 0x7979), /* 3 green */
	PALETTE_COLOR_INIT (0xf4f4, 0x4747, 0x4747), /* 4 red */
	PALETTE_COLOR_INIT (0xcece, 0x9191, 0x7878), /* 5 light red / brown */
	PALETTE_COLOR_INIT (0xc5c5, 0x8686, 0xc0c0), /* 6 purple */
	PALETTE_COLOR_INIT (0xd7d7, 0xbaba, 0x7d7d), /* 7 orange */
	PALETTE_COLOR_INIT (0xdcdc, 0xdcdc, 0xaaaa), /* 8 yellow */
	PALETTE_COLOR_INIT (0xb5b5, 0xcece, 0xa8a8), /* 9 light green */
	PALETTE_COLOR_INIT (0x4e4e, 0xc9c9, 0xb0b0), /* 10 aqua */
	PALETTE_COLOR_INIT (0x9c9c, 0xdcdc, 0xfefe), /* 11 light aqua */
	PALETTE_COLOR_INIT (0x3737, 0x9494, 0xffff), /* 12 light blue */
	PALETTE_COLOR_INIT (0xd6d6, 0x7070, 0xd6d6), /* 13 pink */
	PALETTE_COLOR_INIT (0x8080, 0x8080, 0x8080), /* 14 gray */
	PALETTE_COLOR_INIT (0xc0c0, 0xc0c0, 0xc0c0), /* 15 light gray */
	/* mIRC colors 16-31 (repeat) */
	PALETTE_COLOR_INIT (0xe5e5, 0xe5e5, 0xe5e5), PALETTE_COLOR_INIT (0x3c3c, 0x3c3c, 0x3c3c),
	PALETTE_COLOR_INIT (0x5656, 0x9c9c, 0xd6d6), PALETTE_COLOR_INIT (0x0d0d, 0xbcbc, 0x7979),
	PALETTE_COLOR_INIT (0xf4f4, 0x4747, 0x4747), PALETTE_COLOR_INIT (0xcece, 0x9191, 0x7878),
	PALETTE_COLOR_INIT (0xc5c5, 0x8686, 0xc0c0), PALETTE_COLOR_INIT (0xd7d7, 0xbaba, 0x7d7d),
	PALETTE_COLOR_INIT (0xdcdc, 0xdcdc, 0xaaaa), PALETTE_COLOR_INIT (0xb5b5, 0xcece, 0xa8a8),
	PALETTE_COLOR_INIT (0x4e4e, 0xc9c9, 0xb0b0), PALETTE_COLOR_INIT (0x9c9c, 0xdcdc, 0xfefe),
	PALETTE_COLOR_INIT (0x3737, 0x9494, 0xffff), PALETTE_COLOR_INIT (0xd6d6, 0x7070, 0xd6d6),
	PALETTE_COLOR_INIT (0x8080, 0x8080, 0x8080), PALETTE_COLOR_INIT (0xc0c0, 0xc0c0, 0xc0c0),

	/* selection colors */
	PALETTE_COLOR_INIT (0xffff, 0xffff, 0xffff), /* 32 COL_MARK_FG */
	PALETTE_COLOR_INIT (0x2626, 0x4f4f, 0x7878), /* 33 COL_MARK_BG */

	/* foreground/background */
	PALETTE_COLOR_INIT (0xd4d4, 0xd4d4, 0xd4d4), /* 34 COL_FG */
	PALETTE_COLOR_INIT (0x1e1e, 0x1e1e, 0x1e1e), /* 35 COL_BG */

	/* interface colors */
	PALETTE_COLOR_INIT (0x4040, 0x4040, 0x4040), /* 36 COL_MARKER (marker line) */
	PALETTE_COLOR_INIT (0x3737, 0x9494, 0xffff), /* 37 COL_NEW_DATA (tab: new data) */
	PALETTE_COLOR_INIT (0xd7d7, 0xbaba, 0x7d7d), /* 38 COL_HILIGHT (tab: nick mentioned) */
	PALETTE_COLOR_INIT (0xf4f4, 0x4747, 0x4747), /* 39 COL_NEW_MSG (tab: new message) */
	PALETTE_COLOR_INIT (0x8080, 0x8080, 0x8080), /* 40 COL_AWAY (tab: away) */
	PALETTE_COLOR_INIT (0xf4f4, 0x4747, 0x4747), /* 41 COL_SPELL (spellcheck underline) */
};

void
palette_get_xtext_colors (XTextColor *palette, size_t palette_len)
{
	size_t i;
	size_t count = palette_len < G_N_ELEMENTS (colors) ? palette_len : G_N_ELEMENTS (colors);

	for (i = 0; i < count; i++)
	{
		palette[i] = palette_color_from_gdk (&colors[i]);
	}
}

void
palette_user_set_color (int idx, const PaletteColor *col)
{
	if (!col)
		return;
	if (idx < 0 || idx > MAX_COL)
		return;

	if (!user_colors_valid)
	{
		memcpy (user_colors, colors, sizeof (user_colors));
		user_colors_valid = TRUE;
	}

#if GTK_CHECK_VERSION(3,0,0)
	user_colors[idx] = *col;
#else
	user_colors[idx].red = col->red;
	user_colors[idx].green = col->green;
	user_colors[idx].blue = col->blue;
	user_colors[idx].pixel = 0;
#endif
}

void
palette_dark_set_color (int idx, const PaletteColor *col)
{
	if (!col)
		return;
	if (idx < 0 || idx > MAX_COL)
		return;

	if (!dark_user_colors_valid)
	{
		/* Start from the currently active palette (should be dark when editing dark mode). */
		memcpy (dark_user_colors, colors, sizeof (dark_user_colors));
		dark_user_colors_valid = TRUE;
	}

#if GTK_CHECK_VERSION(3,0,0)
	dark_user_colors[idx] = *col;
#else
	dark_user_colors[idx].red = col->red;
	dark_user_colors[idx].green = col->green;
	dark_user_colors[idx].blue = col->blue;
	dark_user_colors[idx].pixel = 0;
#endif
}

void
palette_alloc (GtkWidget * widget)
{
	(void) widget;
}

void
palette_load (void)
{
	int i, j, fh;
	char prefname[256];
	struct stat st;
	char *cfg;
	guint16 red, green, blue;
	gboolean dark_found = FALSE;

	fh = zoitechat_open_file ("colors.conf", O_RDONLY, 0, 0);
	if (fh != -1)
	{
		fstat (fh, &st);
		cfg = g_malloc0 (st.st_size + 1);
		read (fh, cfg, st.st_size);

		/* Light palette (default behavior): mIRC colors 0-31. */
		for (i = 0; i < 32; i++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			{
				palette_color_set_rgb16 (&colors[i], red, green, blue);
			}
		}

		/* Light palette: our special colors are mapped at 256+. */
		for (i = 256, j = 32; j < MAX_COL + 1; i++, j++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			{
				palette_color_set_rgb16 (&colors[j], red, green, blue);
			}
		}

		/* Dark palette: start from curated defaults and optionally override from colors.conf. */
		memcpy (dark_user_colors, dark_colors, sizeof (dark_user_colors));

		for (i = 0; i < 32; i++)
		{
			g_snprintf (prefname, sizeof prefname, "dark_color_%d", i);
			if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			{
				palette_color_set_rgb16 (&dark_user_colors[i], red, green, blue);
				dark_found = TRUE;
			}
		}

		for (i = 256, j = 32; j < MAX_COL + 1; i++, j++)
		{
			g_snprintf (prefname, sizeof prefname, "dark_color_%d", i);
			if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			{
				palette_color_set_rgb16 (&dark_user_colors[j], red, green, blue);
				dark_found = TRUE;
			}
		}

		dark_user_colors_valid = dark_found;

		g_free (cfg);
		close (fh);
	}

	/* Snapshot the user's (light) palette for dark mode toggling. */
	memcpy (user_colors, colors, sizeof (user_colors));
	user_colors_valid = TRUE;
}


void
palette_save (void)
{
	int i, j, fh;
	char prefname[256];
	const PaletteColor *lightpal = colors;
	const PaletteColor *darkpal = NULL;
	gboolean dark_mode_active = fe_dark_mode_is_enabled ();

	/* If we're currently in dark mode, keep colors.conf's legacy keys as the user's light palette. */
	if (dark_mode_active && user_colors_valid)
		lightpal = user_colors;

	/* If we're currently in light mode, ensure the snapshot stays in sync. */
	if (!dark_mode_active)
	{
		memcpy (user_colors, colors, sizeof (user_colors));
		user_colors_valid = TRUE;
	}

	/* If dark mode is enabled but we haven't snapshotted a custom dark palette yet, capture it now. */
	if (dark_mode_active && !dark_user_colors_valid)
	{
		memcpy (dark_user_colors, colors, sizeof (dark_user_colors));
		dark_user_colors_valid = TRUE;
	}

	if (dark_user_colors_valid)
		darkpal = dark_user_colors;
	else if (dark_mode_active)
		darkpal = colors; /* current dark palette (likely defaults) */

	fh = zoitechat_open_file ("colors.conf", O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
	if (fh != -1)
	{
		/* Light palette (legacy keys) */
		for (i = 0; i < 32; i++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			guint16 red;
			guint16 green;
			guint16 blue;

			palette_color_get_rgb16 (&lightpal[i], &red, &green, &blue);
			cfg_put_color (fh, red, green, blue, prefname);
		}

		for (i = 256, j = 32; j < MAX_COL + 1; i++, j++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			{
				guint16 red;
				guint16 green;
				guint16 blue;

				palette_color_get_rgb16 (&lightpal[j], &red, &green, &blue);
				cfg_put_color (fh, red, green, blue, prefname);
			}
		}

		/* Dark palette (new keys) */
		if (darkpal)
		{
			for (i = 0; i < 32; i++)
			{
				g_snprintf (prefname, sizeof prefname, "dark_color_%d", i);
				guint16 red;
				guint16 green;
				guint16 blue;

				palette_color_get_rgb16 (&darkpal[i], &red, &green, &blue);
				cfg_put_color (fh, red, green, blue, prefname);
			}

			for (i = 256, j = 32; j < MAX_COL + 1; i++, j++)
			{
				g_snprintf (prefname, sizeof prefname, "dark_color_%d", i);
				{
					guint16 red;
					guint16 green;
					guint16 blue;

					palette_color_get_rgb16 (&darkpal[j], &red, &green, &blue);
					cfg_put_color (fh, red, green, blue, prefname);
				}
			}
		}

		close (fh);
	}
}


static gboolean
palette_color_eq (const PaletteColor *a, const PaletteColor *b)
{
	guint16 red_a;
	guint16 green_a;
	guint16 blue_a;
	guint16 red_b;
	guint16 green_b;
	guint16 blue_b;

	palette_color_get_rgb16 (a, &red_a, &green_a, &blue_a);
	palette_color_get_rgb16 (b, &red_b, &green_b, &blue_b);

	return red_a == red_b && green_a == green_b && blue_a == blue_b;
}

gboolean
palette_apply_dark_mode (gboolean enable)
{
	PaletteColor old_colors[MAX_COL + 1];
	int i;
	gboolean changed = FALSE;

	memcpy (old_colors, colors, sizeof (old_colors));

	/* Ensure we have a snapshot of the user's palette before overriding anything. */
	if (!user_colors_valid)
	{
		memcpy (user_colors, colors, sizeof (user_colors));
		user_colors_valid = TRUE;
	}

	if (enable)
	{
		if (dark_user_colors_valid)
			memcpy (colors, dark_user_colors, sizeof (colors));
		else
			memcpy (colors, dark_colors, sizeof (colors));
	}
	else
		memcpy (colors, user_colors, sizeof (colors));

	/* Track whether any palette entries changed. */
	(void) i;

	for (i = 0; i <= MAX_COL; i++)
	{
		if (!palette_color_eq (&old_colors[i], &colors[i]))
		{
			changed = TRUE;
			break;
		}
	}

	return changed;
}
