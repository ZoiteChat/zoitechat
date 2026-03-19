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

#ifndef ZOITECHAT_GTK3_THEME_SERVICE_H
#define ZOITECHAT_GTK3_THEME_SERVICE_H

#include "zoitechat.h"

typedef enum
{
	ZOITECHAT_GTK3_THEME_SOURCE_SYSTEM = 0,
	ZOITECHAT_GTK3_THEME_SOURCE_USER = 1
} ZoitechatGtk3ThemeSource;

typedef struct
{
	char *id;
	char *display_name;
	char *path;
	gboolean has_dark_variant;
	char *thumbnail_path;
	ZoitechatGtk3ThemeSource source;
} ZoitechatGtk3Theme;

char *zoitechat_gtk3_theme_service_get_user_themes_dir (void);
GPtrArray *zoitechat_gtk3_theme_service_discover (void);
void zoitechat_gtk3_theme_free (ZoitechatGtk3Theme *theme);
ZoitechatGtk3Theme *zoitechat_gtk3_theme_find_by_id (const char *theme_id);
gboolean zoitechat_gtk3_theme_service_import (const char *source_path, char **imported_id, GError **error);
gboolean zoitechat_gtk3_theme_service_remove_user_theme (const char *theme_id, GError **error);
char *zoitechat_gtk3_theme_pick_css_dir_for_minor (const char *theme_root, int preferred_minor);
char *zoitechat_gtk3_theme_pick_css_dir (const char *theme_root);
GPtrArray *zoitechat_gtk3_theme_build_inheritance_chain (const char *theme_root);

#endif
