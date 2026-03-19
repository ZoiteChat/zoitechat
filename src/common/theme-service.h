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

#ifndef ZOITECHAT_THEME_SERVICE_H
#define ZOITECHAT_THEME_SERVICE_H

#include "zoitechat.h"

char *zoitechat_theme_service_get_themes_dir (void);
GStrv zoitechat_theme_service_discover_themes (void);
gboolean zoitechat_theme_service_apply (const char *theme_name, GError **error);
void zoitechat_theme_service_set_post_apply_callback (void (*callback) (void));
void zoitechat_theme_service_run_post_apply_callback (void);

#endif
