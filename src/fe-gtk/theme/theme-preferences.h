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

#ifndef ZOITECHAT_THEME_PREFERENCES_H
#define ZOITECHAT_THEME_PREFERENCES_H

#include "../fe-gtk.h"
#include "../../common/zoitechat.h"

#include "theme-access.h"

GtkWidget *theme_preferences_create_page (GtkWindow *parent,
                                                struct zoitechatprefs *setup_prefs,
                                                gboolean *color_change_flag);
GtkWidget *theme_preferences_create_color_page (GtkWindow *parent,
                                                struct zoitechatprefs *setup_prefs,
                                                gboolean *color_change_flag);
void theme_preferences_apply_to_session (session_gui *gui, InputStyle *input_style);
void theme_preferences_stage_begin (void);
void theme_preferences_stage_apply (void);
void theme_preferences_stage_commit (void);
void theme_preferences_stage_discard (void);

#endif
