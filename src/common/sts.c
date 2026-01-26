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
#include <time.h>
#include <fcntl.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "zoitechat.h"
#include "cfgfiles.h"
#include "util.h"
#include "text.h"
#include "sts.h"

static GHashTable *sts_profiles = NULL;
static gboolean sts_loaded = FALSE;

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
sts_profile_new (const char *host, guint16 port, time_t expires_at, guint64 duration, gboolean preload)
{
	sts_profile *profile = g_new0 (sts_profile, 1);

	profile->host = g_strdup (host);
	profile->port = port;
	profile->expires_at = expires_at;
	profile->duration = duration;
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

	if (profile->duration > 0)
	{
		g_string_append_printf (serialized, " %" G_GUINT64_FORMAT, profile->duration);
	}

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
	guint64 duration = 0;
	gboolean preload = FALSE;
	gboolean duration_seen = FALSE;
	gchar **pairs = NULL;
	int i = 0;

	if (!serialized || !*serialized)
	{
		return NULL;
	}

	pairs = g_strsplit_set (serialized, " \t", -1);
	{
		const char *fields[5] = {0};
		int field_count = 0;

		for (i = 0; pairs[i]; i++)
		{
			if (!pairs[i][0])
			{
				continue;
			}

			if (field_count < 5)
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
				if (field_count == 4 && sts_parse_bool (fields[3]))
				{
					preload = TRUE;
				}
				else
				{
					duration = g_ascii_strtoull (fields[3], NULL, 10);
					duration_seen = TRUE;
				}
			}

			if (field_count >= 5)
			{
				duration_seen = TRUE;
				preload = sts_parse_bool (fields[4]);
			}
		}
	}

	if (!host || !*host || expires_at < 0)
	{
		g_free (host);
		g_strfreev (pairs);
		return NULL;
	}

	if (!duration_seen && duration == 0 && expires_at > 0)
	{
		time_t now = time (NULL);
		if (expires_at > now)
		{
			duration = (guint64) (expires_at - now);
		}
	}

	sts_profile *profile = sts_profile_new (host, port, (time_t) expires_at, duration, preload);
	g_free (host);
	g_strfreev (pairs);
	return profile;
}

static char *
sts_normalize_host (const char *host)
{
	char *normalized;
	gsize len;

	if (!host || !*host)
	{
		return NULL;
	}

	normalized = g_ascii_strdown (host, -1);
	g_strstrip (normalized);
	len = strlen (normalized);

	if (len > 2 && normalized[0] == '[' && normalized[len - 1] == ']')
	{
		char *trimmed = g_strndup (normalized + 1, len - 2);
		g_free (normalized);
		normalized = trimmed;
	}

	return normalized;
}

static void
sts_profiles_ensure (void)
{
	if (!sts_profiles)
	{
		sts_profiles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
											  (GDestroyNotify) sts_profile_free);
	}
}

static void
sts_profile_store (sts_profile *profile)
{
	char *normalized;

	if (!profile || !profile->host)
	{
		sts_profile_free (profile);
		return;
	}

	sts_profiles_ensure ();
	normalized = sts_normalize_host (profile->host);
	if (!normalized)
	{
		sts_profile_free (profile);
		return;
	}

	g_hash_table_replace (sts_profiles, normalized, profile);
}

static void
sts_profile_remove (const char *host)
{
	char *normalized;

	if (!host)
	{
		return;
	}

	sts_profiles_ensure ();
	normalized = sts_normalize_host (host);
	if (!normalized)
	{
		return;
	}

	g_hash_table_remove (sts_profiles, normalized);
	g_free (normalized);
}

static sts_profile *
sts_profile_lookup (const char *host, time_t now)
{
	char *normalized;
	sts_profile *profile = NULL;

	sts_profiles_ensure ();
	normalized = sts_normalize_host (host);
	if (!normalized)
	{
		return NULL;
	}

	profile = g_hash_table_lookup (sts_profiles, normalized);
	if (profile && profile->expires_at > 0 && profile->expires_at <= now)
	{
		g_hash_table_remove (sts_profiles, normalized);
		profile = NULL;
	}

	g_free (normalized);
	return profile;
}

static gboolean
sts_parse_value (const char *value, guint16 *port, guint64 *duration, gboolean *preload,
				 gboolean *has_port, gboolean *has_duration, gboolean *has_preload)
{
	char **tokens;
	gsize i;

	if (!value || !*value)
	{
		return FALSE;
	}

	*has_port = FALSE;
	*has_duration = FALSE;
	*has_preload = FALSE;

	tokens = g_strsplit (value, ",", -1);
	for (i = 0; tokens[i]; i++)
	{
		char *token = g_strstrip (tokens[i]);
		char *equals = strchr (token, '=');
		char *key = token;
		char *val = NULL;

		if (!*token)
		{
			continue;
		}

		if (equals)
		{
			*equals = '\0';
			val = equals + 1;
		}

		if (!g_ascii_strcasecmp (key, "port"))
		{
			gint64 port_value;

			if (*has_port || !val)
			{
				continue;
			}

			port_value = g_ascii_strtoll (val, NULL, 10);
			if (port_value > 0 && port_value <= G_MAXUINT16)
			{
				*port = (guint16) port_value;
				*has_port = TRUE;
			}
		}
		else if (!g_ascii_strcasecmp (key, "duration"))
		{
			guint64 duration_value;

			if (*has_duration || !val)
			{
				continue;
			}

			duration_value = g_ascii_strtoull (val, NULL, 10);
			*duration = duration_value;
			*has_duration = TRUE;
		}
		else if (!g_ascii_strcasecmp (key, "preload"))
		{
			if (*has_preload)
			{
				continue;
			}
			*preload = TRUE;
			*has_preload = TRUE;
		}
	}

	g_strfreev (tokens);
	return TRUE;
}

void
sts_init (void)
{
	sts_profiles_ensure ();
	if (sts_loaded)
	{
		return;
	}

	sts_loaded = TRUE;
	{
		int fh;
		char buf[512];

		fh = zoitechat_open_file ("sts.conf", O_RDONLY, 0, 0);
		if (fh != -1)
		{
			while (waitline (fh, buf, sizeof buf, FALSE) != -1)
			{
				if (buf[0] == '#' || buf[0] == '\0')
				{
					continue;
				}

				sts_profile *profile = sts_profile_deserialize (buf);
				if (!profile)
				{
					continue;
				}

				if (profile->expires_at <= time (NULL))
				{
					sts_profile_free (profile);
					continue;
				}

				if (profile->duration == 0)
				{
					sts_profile_free (profile);
					continue;
				}

				sts_profile_store (profile);
			}
			close (fh);
		}
	}
}

void
sts_save (void)
{
	GHashTableIter iter;
	gpointer key;
	gpointer value;
	int fh;

	sts_profiles_ensure ();
	fh = zoitechat_open_file ("sts.conf", O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
	if (fh == -1)
	{
		return;
	}

	g_hash_table_iter_init (&iter, sts_profiles);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		sts_profile *profile = value;
		char *serialized;

		if (!profile || profile->expires_at <= time (NULL) || profile->duration == 0)
		{
			continue;
		}

		serialized = sts_profile_serialize (profile);
		if (serialized)
		{
			write (fh, serialized, strlen (serialized));
			write (fh, "\n", 1);
			g_free (serialized);
		}
	}

	close (fh);
}

void
sts_cleanup (void)
{
	if (!sts_profiles)
	{
		return;
	}

	sts_save ();
	g_hash_table_destroy (sts_profiles);
	sts_profiles = NULL;
	sts_loaded = FALSE;
}

gboolean
sts_apply_policy_for_connection (struct server *serv, const char *hostname, int *port)
{
	sts_profile *profile;
	time_t now;

	if (!hostname || !*hostname || !port)
	{
		return TRUE;
	}

	sts_init ();
	sts_profiles_ensure ();
	now = time (NULL);
	profile = sts_profile_lookup (hostname, now);
	if (!profile)
	{
		return TRUE;
	}

	if (profile->port == 0)
	{
		sts_profile_remove (hostname);
		return TRUE;
	}

#ifdef USE_OPENSSL
	serv->use_ssl = TRUE;
	if (profile->port > 0)
	{
		*port = profile->port;
	}
	return TRUE;
#else
	PrintTextf (serv->server_session,
				_("STS policy requires TLS for %s, but TLS is not available.\n"),
				hostname);
	return FALSE;
#endif
}

gboolean
sts_handle_capability (struct server *serv, const char *value)
{
	guint16 port = 0;
	guint64 duration = 0;
	gboolean preload = FALSE;
	gboolean has_port = FALSE;
	gboolean has_duration = FALSE;
	gboolean has_preload = FALSE;
	const char *hostname;

	if (!serv || !value)
	{
		return FALSE;
	}

	sts_init ();
	if (!sts_parse_value (value, &port, &duration, &preload,
						  &has_port, &has_duration, &has_preload))
	{
		return FALSE;
	}

	hostname = serv->sts_host[0] ? serv->sts_host : serv->servername;
	if (!hostname || !*hostname)
	{
		return FALSE;
	}

	if (!serv->use_ssl)
	{
		if (!has_port)
		{
			if (serv->port > 0)
			{
				port = (guint16) serv->port;
				has_port = TRUE;
			}
			else
			{
				return FALSE;
			}
		}
#ifdef USE_OPENSSL
		if (serv->sts_upgrade_in_progress)
		{
			return TRUE;
		}

		serv->sts_upgrade_in_progress = TRUE;
		serv->use_ssl = TRUE;
		{
			char host_copy[128];

			safe_strcpy (host_copy, hostname, sizeof (host_copy));
			serv->disconnect (serv->server_session, FALSE, -1);
			serv->connect (serv, host_copy, (int) port, serv->no_login);
		}
#else
		PrintTextf (serv->server_session,
					_("STS upgrade requested for %s, but TLS is not available.\n"),
					hostname);
#endif
		return TRUE;
	}

	if (!has_duration)
	{
		return FALSE;
	}

	if (duration == 0)
	{
		sts_profile_remove (hostname);
		serv->sts_duration_seen = FALSE;
		return FALSE;
	}

	{
		time_t now = time (NULL);
		time_t expires_at = now + (time_t) duration;
		guint16 effective_port = serv->port > 0 ? (guint16) serv->port : port;
		sts_profile *profile;

		if (effective_port == 0)
		{
			return FALSE;
		}

		profile = sts_profile_new (hostname, effective_port, expires_at, duration,
								   has_preload ? preload : FALSE);
		sts_profile_store (profile);
		serv->sts_duration_seen = TRUE;
	}

	return FALSE;
}

void
sts_reschedule_on_disconnect (struct server *serv)
{
	sts_profile *profile;
	time_t now;

	if (!serv || !serv->sts_duration_seen)
	{
		return;
	}

	sts_init ();
	now = time (NULL);
	profile = sts_profile_lookup (serv->sts_host[0] ? serv->sts_host : serv->servername, now);
	if (!profile || profile->duration == 0)
	{
		return;
	}

	profile->expires_at = now + (time_t) profile->duration;
}
