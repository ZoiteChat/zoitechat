/* ZoiteChat
 * Copyright (C) 1998-2010 Peter Zelezny.
 * Copyright (C) 2009-2013 Berke Viktor.
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

#ifndef ZOITECHAT_PALETTE_H
#define ZOITECHAT_PALETTE_H

#include <stddef.h>

#include "xtext-color.h"

#if GTK_CHECK_VERSION(3,0,0)
typedef GdkRGBA PaletteColor;
#define PALETTE_GDK_TYPE GDK_TYPE_RGBA
#define PALETTE_FOREGROUND_PROPERTY "foreground-rgba"
#else
typedef GdkColor PaletteColor;
#define PALETTE_GDK_TYPE GDK_TYPE_COLOR
#define PALETTE_FOREGROUND_PROPERTY "foreground-gdk"
#endif

extern PaletteColor colors[];

static inline void
palette_color_get_rgb16 (const PaletteColor *color, guint16 *red, guint16 *green, guint16 *blue)
{
#if GTK_CHECK_VERSION(3,0,0)
	*red = (guint16) CLAMP (color->red * 65535.0 + 0.5, 0.0, 65535.0);
	*green = (guint16) CLAMP (color->green * 65535.0 + 0.5, 0.0, 65535.0);
	*blue = (guint16) CLAMP (color->blue * 65535.0 + 0.5, 0.0, 65535.0);
#else
	*red = color->red;
	*green = color->green;
	*blue = color->blue;
#endif
}

#define COL_MARK_FG 32
#define COL_MARK_BG 33
#define COL_FG 34
#define COL_BG 35
#define COL_MARKER 36
#define COL_NEW_DATA 37
#define COL_HILIGHT 38
#define COL_NEW_MSG 39
#define COL_AWAY 40
#define COL_SPELL 41
#define MAX_COL 41

void palette_alloc (GtkWidget * widget);
void palette_load (void);
void palette_save (void);

/* Keep a copy of the user's palette so dark mode can be toggled without losing it. */
void palette_user_set_color (int idx, const PaletteColor *col);
void palette_dark_set_color (int idx, const PaletteColor *col);

/*
 * Apply ZoiteChat's built-in "dark mode" palette.
 *
 * When enabled, ZoiteChat switches to a curated dark-friendly palette for:
 *  - message colors (mIRC palette)
 *  - selection colors
 *  - tab highlight colors
 *  - chat/user/channel list background + foreground
 *
 * The user's palette is preserved in-memory and written to colors.conf even
 * while dark mode is enabled, so disabling dark mode restores the previous
 * colors without surprises.
 *
 * Returns TRUE if any palette entries were changed.
 */
gboolean palette_apply_dark_mode (gboolean enable);

void palette_get_xtext_colors (XTextColor *palette, size_t palette_len);

#endif
