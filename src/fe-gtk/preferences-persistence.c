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

#include "preferences-persistence.h"

#include "../common/cfgfiles.h"
#include "theme/theme-runtime.h"

PreferencesPersistenceResult
preferences_persistence_save_all (void)
{
	PreferencesPersistenceResult result;
	char *config_temp;
	char *theme_temp;

	result.success = FALSE;
	result.retry_possible = TRUE;
	result.partial_failure = FALSE;
	result.config_failed = FALSE;
	result.theme_failed = FALSE;
	result.failed_file = NULL;
	config_temp = NULL;
	theme_temp = NULL;

	if (!save_config_prepare (&config_temp))
	{
		result.config_failed = TRUE;
		goto done;
	}

	if (!theme_runtime_save_prepare (&theme_temp))
	{
		result.theme_failed = TRUE;
		goto done;
	}

	if (!save_config_finalize (config_temp))
	{
		result.config_failed = TRUE;
		goto done;
	}

	if (!theme_runtime_save_finalize (theme_temp))
	{
		result.theme_failed = TRUE;
		result.partial_failure = TRUE;
		goto done;
	}

	result.success = TRUE;

 done:
	if (!result.success)
	{
		if (result.config_failed && result.theme_failed)
			result.failed_file = "zoitechat.conf and colors.conf";
		else if (result.config_failed)
			result.failed_file = "zoitechat.conf";
		else if (result.theme_failed)
			result.failed_file = "colors.conf";
	}

	save_config_discard (config_temp);
	theme_runtime_save_discard (theme_temp);
	g_free (config_temp);
	g_free (theme_temp);

	return result;
}
