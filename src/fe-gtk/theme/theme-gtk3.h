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

#ifndef ZOITECHAT_THEME_GTK3_H
#define ZOITECHAT_THEME_GTK3_H

#include <glib.h>

typedef enum
{
	THEME_GTK3_VARIANT_FOLLOW_SYSTEM = 0,
	THEME_GTK3_VARIANT_PREFER_LIGHT = 1,
	THEME_GTK3_VARIANT_PREFER_DARK = 2
} ThemeGtk3Variant;

void theme_gtk3_init (void);
gboolean theme_gtk3_apply_current (GError **error);
gboolean theme_gtk3_apply (const char *theme_id, ThemeGtk3Variant variant, GError **error);
gboolean theme_gtk3_refresh (const char *theme_id, ThemeGtk3Variant variant, GError **error);
ThemeGtk3Variant theme_gtk3_variant_for_theme (const char *theme_id);
void theme_gtk3_invalidate_provider_cache (void);
void theme_gtk3_disable (void);
gboolean theme_gtk3_is_active (void);

#endif
