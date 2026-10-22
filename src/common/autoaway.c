/* ZoiteChat
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "zoitechat.h"
#include "zoitechatc.h"
#include "server.h"
#include "fe.h"
#include "autoaway.h"

static gint64 last_activity;
static int timer_tag;
static int previous_timeout;
static unsigned int previous_system;

void
autoaway_activity (void)
{
	last_activity = g_get_monotonic_time ();
}

/* A manual /AWAY takes ownership even if the server already says away. */
void
autoaway_manual (server *serv)
{
	serv->autoaway_owned = FALSE;
	autoaway_activity ();
}

void
autoaway_reset (server *serv)
{
	if (!serv->autoaway_owned)
		return;

	serv->autoaway_owned = FALSE;
	serv->reconnect_away = FALSE;
	g_clear_pointer (&serv->last_away_reason, g_free);
}

static void
autoaway_release (server *serv)
{
	if (!serv->autoaway_owned)
		return;

	/* Cancel even an unacknowledged AWAY. IRC replies may arrive later. */
	if (serv->connected && serv->end_of_motd)
		serv->p_set_back (serv);
	autoaway_reset (serv);
}

void
autoaway_check (gint64 now, gint64 desktop_idle)
{
	GSList *list;
	gint64 idle = MAX ((gint64)0, now - last_activity);
	gint64 limit = (gint64)prefs.hex_away_auto_timeout * 60 * G_USEC_PER_SEC;

	if (prefs.hex_away_auto_system && desktop_idle >= 0)
		idle = MIN (idle, desktop_idle);

	for (list = serv_list; list; list = list->next)
	{
		server *serv = list->data;

		if (!serv->connected)
		{
			/* Re-evaluate idle after login instead of restoring stale autoaway. */
			autoaway_reset (serv);
			continue;
		}
		if (!serv->end_of_motd)
			continue;

		if (limit <= 0 || idle < limit)
		{
			autoaway_release (serv);
			continue;
		}
		if (serv->is_away || serv->reconnect_away)
			continue;

		/* Retry a rejected/lost request slowly, never once per poll. */
		if (serv->autoaway_owned &&
		    now - serv->autoaway_sent_at < 30 * G_USEC_PER_SEC)
			continue;

		g_free (serv->last_away_reason);
		serv->last_away_reason = g_strdup (prefs.hex_away_auto_reason[0] ?
		                                 prefs.hex_away_auto_reason : _("Idle"));
		serv->autoaway_owned = TRUE;
		serv->autoaway_sent_at = now;
		serv->p_set_away (serv, serv->last_away_reason);
	}
}

static int
autoaway_timer (void)
{
	autoaway_check (g_get_monotonic_time (),
	                prefs.hex_away_auto_system ? fe_get_idle_time () : -1);
	return 1;
}

void
autoaway_reinit (void)
{
	if (previous_timeout != prefs.hex_away_auto_timeout ||
	    previous_system != prefs.hex_away_auto_system)
	{
		autoaway_activity ();
		previous_timeout = prefs.hex_away_auto_timeout;
		previous_system = prefs.hex_away_auto_system;
	}

	if (prefs.hex_away_auto_timeout > 0)
	{
		if (!timer_tag)
		{
			autoaway_activity ();
			timer_tag = fe_timeout_add_seconds (5, autoaway_timer, NULL);
		}
	}
	else
	{
		if (timer_tag)
		{
			fe_timeout_remove (timer_tag);
			timer_tag = 0;
		}
		/* Disabling autoaway must also work without another input event. */
		autoaway_check (g_get_monotonic_time (), -1);
	}
}
