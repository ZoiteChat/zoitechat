/* ZoiteChat
 * Copyright (C) 2024
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

#include <glib.h>

#include "sts.h"

static gboolean
sts_parse_bool (const char *value)
{
	if (!value || !*value)
	{
		return FALSE;
	}

	return g_ascii_strcasecmp (value, "1") == 0 ||
		g_ascii_strcasecmp (value, "true") == 0 ||
		g_ascii_strcasecmp (value, "yes") == 0;
}

sts_profile *
sts_profile_new (const char *host, guint16 port, time_t expires_at, gboolean preload)
{
	sts_profile *profile = g_new0 (sts_profile, 1);

	profile->host = g_strdup (host);
	profile->port = port;
	profile->expires_at = expires_at;
	profile->preload = preload;

	return profile;
}

void
sts_profile_free (sts_profile *profile)
{
	if (!profile)
	{
		return;
	}

	g_free (profile->host);
	g_free (profile);
}

char *
sts_profile_serialize (const sts_profile *profile)
{
	GString *serialized;
	char *escaped_host;
	char *result;

	if (!profile || !profile->host || !*profile->host)
	{
		return NULL;
	}

	escaped_host = g_strdup (profile->host);
	serialized = g_string_new (escaped_host);
	g_free (escaped_host);

	g_string_append_printf (serialized, " %u %" G_GINT64_FORMAT,
								profile->port, (gint64) profile->expires_at);

	if (profile->preload)
	{
		g_string_append (serialized, " 1");
	}

	result = g_string_free (serialized, FALSE);
	return result;
}

sts_profile *
sts_profile_deserialize (const char *serialized)
{
	char *host = NULL;
	guint16 port = 0;
	gint64 expires_at = -1;
	gboolean preload = FALSE;
	gchar **pairs = NULL;
	int i = 0;

	if (!serialized || !*serialized)
	{
		return NULL;
	}

	pairs = g_strsplit_set (serialized, " \t", -1);
	{
		const char *fields[4] = {0};
		int field_count = 0;

		for (i = 0; pairs[i]; i++)
		{
			if (!pairs[i][0])
			{
				continue;
			}

			if (field_count < 4)
			{
				fields[field_count++] = pairs[i];
			}
		}

		if (field_count >= 3)
		{
			host = g_strdup (fields[0]);

			gint64 port_value = g_ascii_strtoll (fields[1], NULL, 10);
			if (port_value > 0 && port_value <= G_MAXUINT16)
			{
				port = (guint16) port_value;
			}

			expires_at = g_ascii_strtoll (fields[2], NULL, 10);

			if (field_count >= 4)
			{
				preload = sts_parse_bool (fields[3]);
			}
		}
	}

	if (!host || !*host || expires_at < 0)
	{
		g_free (host);
		g_strfreev (pairs);
		return NULL;
	}

	sts_profile *profile = sts_profile_new (host, port, (time_t) expires_at, preload);
	g_free (host);
	g_strfreev (pairs);
	return profile;
}
