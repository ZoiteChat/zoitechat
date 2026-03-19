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

#ifndef ZOITECHAT_PREFERENCES_PERSISTENCE_H
#define ZOITECHAT_PREFERENCES_PERSISTENCE_H

#include <glib.h>

typedef struct
{
	gboolean success;
	gboolean retry_possible;
	gboolean partial_failure;
	gboolean config_failed;
	gboolean theme_failed;
	const char *failed_file;
} PreferencesPersistenceResult;

PreferencesPersistenceResult preferences_persistence_save_all (void);

#endif
