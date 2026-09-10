/* ZoiteChat
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

#ifndef ZOITECHAT_EMOJI_PICKER_H
#define ZOITECHAT_EMOJI_PICKER_H

#include <gtk/gtk.h>

/* Creates the emoji picker button for an input entry.  The returned
 * GtkMenuButton opens a popover with search, categories, recently used
 * emoji and skin-tone selection; activating an emoji inserts its Unicode
 * sequence into target_entry at the cursor.  The popover contents are
 * built lazily on first open. */
GtkWidget *emoji_picker_button_new (GtkWidget *target_entry);

#endif
