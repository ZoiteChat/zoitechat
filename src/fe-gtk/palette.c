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


GdkColor colors[] = {
	/* colors for xtext */
	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 0 white */
	{0, 0x2e2e, 0x3434, 0x3636}, /* 1 black */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 2 blue */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 3 green */
	{0, 0xcccc, 0x0000, 0x0000}, /* 4 red */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 5 light red */
	{0, 0x5c5c, 0x3535, 0x6666}, /* 6 purple */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 7 orange */
	{0, 0xc4c4, 0xa0a0, 0x0000}, /* 8 yellow */
	{0, 0x7373, 0xd2d2, 0x1616}, /* 9 green */
	{0, 0x1111, 0xa8a8, 0x7979}, /* 10 aqua */
	{0, 0x5858, 0xa1a1, 0x9d9d}, /* 11 light aqua */
	{0, 0x5757, 0x7979, 0x9e9e}, /* 12 blue */
	{0, 0xa0d0, 0x42d4, 0x6562}, /* 13 light purple */
	{0, 0x5555, 0x5757, 0x5353}, /* 14 grey */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 15 light grey */

	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 16 white */
	{0, 0x2e2e, 0x3434, 0x3636}, /* 17 black */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 18 blue */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 19 green */
	{0, 0xcccc, 0x0000, 0x0000}, /* 20 red */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 21 light red */
	{0, 0x5c5c, 0x3535, 0x6666}, /* 22 purple */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 23 orange */
	{0, 0xc4c4, 0xa0a0, 0x0000}, /* 24 yellow */
	{0, 0x7373, 0xd2d2, 0x1616}, /* 25 green */
	{0, 0x1111, 0xa8a8, 0x7979}, /* 26 aqua */
	{0, 0x5858, 0xa1a1, 0x9d9d}, /* 27 light aqua */
	{0, 0x5757, 0x7979, 0x9e9e}, /* 28 blue */
	{0, 0xa0d0, 0x42d4, 0x6562}, /* 29 light purple */
	{0, 0x5555, 0x5757, 0x5353}, /* 30 grey */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 31 light grey */

	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 32 marktext Fore (white) */
	{0, 0x2020, 0x4a4a, 0x8787}, /* 33 marktext Back (blue) */
	{0, 0x2512, 0x29e8, 0x2b85}, /* 34 foreground (black) */
	{0, 0xfae0, 0xfae0, 0xf8c4}, /* 35 background (white) */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 36 marker line (red) */

	/* colors for GUI */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 37 tab New Data (dark red) */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 38 tab Nick Mentioned (blue) */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 39 tab New Message (red) */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 40 away user (grey) */
	{0, 0xa4a4, 0x0000, 0x0000}, /* 41 spell checker color (red) */
};

/* User palette snapshot (what we write to colors.conf) */
static GdkColor user_colors[MAX_COL + 1];
static gboolean user_colors_valid = FALSE;

/* Dark palette snapshot (saved separately so dark mode can have its own custom palette). */
static GdkColor dark_user_colors[MAX_COL + 1];
static gboolean dark_user_colors_valid = FALSE;

/* ZoiteChat's curated dark palette (applies when prefs.hex_gui_dark_mode is enabled). */
static const GdkColor dark_colors[MAX_COL + 1] = {
	/* mIRC colors 0-15 */
	{0, 0xe5e5, 0xe5e5, 0xe5e5}, /* 0 white */
	{0, 0x3c3c, 0x3c3c, 0x3c3c}, /* 1 black (dark gray for contrast) */
	{0, 0x5656, 0x9c9c, 0xd6d6}, /* 2 blue */
	{0, 0x0d0d, 0xbcbc, 0x7979}, /* 3 green */
	{0, 0xf4f4, 0x4747, 0x4747}, /* 4 red */
	{0, 0xcece, 0x9191, 0x7878}, /* 5 light red / brown */
	{0, 0xc5c5, 0x8686, 0xc0c0}, /* 6 purple */
	{0, 0xd7d7, 0xbaba, 0x7d7d}, /* 7 orange */
	{0, 0xdcdc, 0xdcdc, 0xaaaa}, /* 8 yellow */
	{0, 0xb5b5, 0xcece, 0xa8a8}, /* 9 light green */
	{0, 0x4e4e, 0xc9c9, 0xb0b0}, /* 10 aqua */
	{0, 0x9c9c, 0xdcdc, 0xfefe}, /* 11 light aqua */
	{0, 0x3737, 0x9494, 0xffff}, /* 12 light blue */
	{0, 0xd6d6, 0x7070, 0xd6d6}, /* 13 pink */
	{0, 0x8080, 0x8080, 0x8080}, /* 14 gray */
	{0, 0xc0c0, 0xc0c0, 0xc0c0}, /* 15 light gray */
	/* mIRC colors 16-31 (repeat) */
	{0, 0xe5e5, 0xe5e5, 0xe5e5}, {0, 0x3c3c, 0x3c3c, 0x3c3c},
	{0, 0x5656, 0x9c9c, 0xd6d6}, {0, 0x0d0d, 0xbcbc, 0x7979},
	{0, 0xf4f4, 0x4747, 0x4747}, {0, 0xcece, 0x9191, 0x7878},
	{0, 0xc5c5, 0x8686, 0xc0c0}, {0, 0xd7d7, 0xbaba, 0x7d7d},
	{0, 0xdcdc, 0xdcdc, 0xaaaa}, {0, 0xb5b5, 0xcece, 0xa8a8},
	{0, 0x4e4e, 0xc9c9, 0xb0b0}, {0, 0x9c9c, 0xdcdc, 0xfefe},
	{0, 0x3737, 0x9494, 0xffff}, {0, 0xd6d6, 0x7070, 0xd6d6},
	{0, 0x8080, 0x8080, 0x8080}, {0, 0xc0c0, 0xc0c0, 0xc0c0},

	/* selection colors */
	{0, 0xffff, 0xffff, 0xffff}, /* 32 COL_MARK_FG */
	{0, 0x2626, 0x4f4f, 0x7878}, /* 33 COL_MARK_BG */

	/* foreground/background */
	{0, 0xd4d4, 0xd4d4, 0xd4d4}, /* 34 COL_FG */
	{0, 0x1e1e, 0x1e1e, 0x1e1e}, /* 35 COL_BG */

	/* interface colors */
	{0, 0x4040, 0x4040, 0x4040}, /* 36 COL_MARKER (marker line) */
	{0, 0x3737, 0x9494, 0xffff}, /* 37 COL_NEW_DATA (tab: new data) */
	{0, 0xd7d7, 0xbaba, 0x7d7d}, /* 38 COL_HILIGHT (tab: nick mentioned) */
	{0, 0xf4f4, 0x4747, 0x4747}, /* 39 COL_NEW_MSG (tab: new message) */
	{0, 0x8080, 0x8080, 0x8080}, /* 40 COL_AWAY (tab: away) */
	{0, 0xf4f4, 0x4747, 0x4747}, /* 41 COL_SPELL (spellcheck underline) */
};

void
palette_user_set_color (int idx, const GdkColor *col)
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

	user_colors[idx].red = col->red;
	user_colors[idx].green = col->green;
	user_colors[idx].blue = col->blue;
	user_colors[idx].pixel = 0;
}

void
palette_dark_set_color (int idx, const GdkColor *col)
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

	dark_user_colors[idx].red = col->red;
	dark_user_colors[idx].green = col->green;
	dark_user_colors[idx].blue = col->blue;
	dark_user_colors[idx].pixel = 0;
}

void
palette_alloc (GtkWidget * widget)
{
	int i;
	static int done_alloc = FALSE;
	GdkColormap *cmap;

	if (!done_alloc)		  /* don't do it again */
	{
		done_alloc = TRUE;
		cmap = gtk_widget_get_colormap (widget);
		for (i = MAX_COL; i >= 0; i--)
			gdk_colormap_alloc_color (cmap, &colors[i], FALSE, TRUE);
	}
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
				colors[i].red = red;
				colors[i].green = green;
				colors[i].blue = blue;
			}
		}

		/* Light palette: our special colors are mapped at 256+. */
		for (i = 256, j = 32; j < MAX_COL + 1; i++, j++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			{
				colors[j].red = red;
				colors[j].green = green;
				colors[j].blue = blue;
			}
		}

		/* Dark palette: start from curated defaults and optionally override from colors.conf. */
		memcpy (dark_user_colors, dark_colors, sizeof (dark_user_colors));

		for (i = 0; i < 32; i++)
		{
			g_snprintf (prefname, sizeof prefname, "dark_color_%d", i);
			if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			{
				dark_user_colors[i].red = red;
				dark_user_colors[i].green = green;
				dark_user_colors[i].blue = blue;
				dark_found = TRUE;
			}
		}

		for (i = 256, j = 32; j < MAX_COL + 1; i++, j++)
		{
			g_snprintf (prefname, sizeof prefname, "dark_color_%d", i);
			if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			{
				dark_user_colors[j].red = red;
				dark_user_colors[j].green = green;
				dark_user_colors[j].blue = blue;
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
	const GdkColor *lightpal = colors;
	const GdkColor *darkpal = NULL;

	/* If we're currently in dark mode, keep colors.conf's legacy keys as the user's light palette. */
	if (prefs.hex_gui_dark_mode && user_colors_valid)
		lightpal = user_colors;

	/* If we're currently in light mode, ensure the snapshot stays in sync. */
	if (!prefs.hex_gui_dark_mode)
	{
		memcpy (user_colors, colors, sizeof (user_colors));
		user_colors_valid = TRUE;
	}

	/* If dark mode is enabled but we haven't snapshotted a custom dark palette yet, capture it now. */
	if (prefs.hex_gui_dark_mode && !dark_user_colors_valid)
	{
		memcpy (dark_user_colors, colors, sizeof (dark_user_colors));
		dark_user_colors_valid = TRUE;
	}

	if (dark_user_colors_valid)
		darkpal = dark_user_colors;
	else if (prefs.hex_gui_dark_mode)
		darkpal = colors; /* current dark palette (likely defaults) */

	fh = zoitechat_open_file ("colors.conf", O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
	if (fh != -1)
	{
		/* Light palette (legacy keys) */
		for (i = 0; i < 32; i++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			cfg_put_color (fh, lightpal[i].red, lightpal[i].green, lightpal[i].blue, prefname);
		}

		for (i = 256, j = 32; j < MAX_COL + 1; i++, j++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			cfg_put_color (fh, lightpal[j].red, lightpal[j].green, lightpal[j].blue, prefname);
		}

		/* Dark palette (new keys) */
		if (darkpal)
		{
			for (i = 0; i < 32; i++)
			{
				g_snprintf (prefname, sizeof prefname, "dark_color_%d", i);
				cfg_put_color (fh, darkpal[i].red, darkpal[i].green, darkpal[i].blue, prefname);
			}

			for (i = 256, j = 32; j < MAX_COL + 1; i++, j++)
			{
				g_snprintf (prefname, sizeof prefname, "dark_color_%d", i);
				cfg_put_color (fh, darkpal[j].red, darkpal[j].green, darkpal[j].blue, prefname);
			}
		}

		close (fh);
	}
}


static gboolean
palette_color_eq (const GdkColor *a, const GdkColor *b)
{
	return a->red == b->red && a->green == b->green && a->blue == b->blue;
}

gboolean
palette_apply_dark_mode (gboolean enable)
{
	GdkColor old_colors[MAX_COL + 1];
	GdkColormap *cmap;
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

	/* Allocate the new colors for GTK's colormap. */
	cmap = gdk_colormap_get_system ();
	for (i = 0; i <= MAX_COL; i++)
		gdk_colormap_alloc_color (cmap, &colors[i], FALSE, TRUE);

	for (i = 0; i <= MAX_COL; i++)
	{
		if (old_colors[i].red != colors[i].red ||
			 old_colors[i].green != colors[i].green ||
			 old_colors[i].blue != colors[i].blue)
		{
			changed = TRUE;
			break;
		}
	}

	return changed;
}

