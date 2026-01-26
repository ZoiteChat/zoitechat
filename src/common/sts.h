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

#ifndef HEXCHAT_STS_H
#define HEXCHAT_STS_H

#include <glib.h>
#include <time.h>

G_BEGIN_DECLS

struct server;

typedef struct sts_profile
{
	char *host;
	guint16 port;
	time_t expires_at;
	guint64 duration;
	gboolean preload;
} sts_profile;

sts_profile *sts_profile_new (const char *host, guint16 port, time_t expires_at, guint64 duration, gboolean preload);
void sts_profile_free (sts_profile *profile);

char *sts_profile_serialize (const sts_profile *profile);
sts_profile *sts_profile_deserialize (const char *serialized);

void sts_init (void);
void sts_save (void);
void sts_cleanup (void);
gboolean sts_apply_policy_for_connection (struct server *serv, const char *hostname, int *port);
gboolean sts_handle_capability (struct server *serv, const char *value);
void sts_reschedule_on_disconnect (struct server *serv);

G_END_DECLS

#endif
