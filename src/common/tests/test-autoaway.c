/* ZoiteChat
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <string.h>
#include "../zoitechat.h"
#include "../autoaway.h"

struct zoitechatprefs prefs;
GSList *serv_list;
static server servers[3];
static int away_count, back_count, timer_adds, timer_removes;
static gint64 base;
static char *sent_reason;

/* Protocol stubs deliberately do not acknowledge requests. */
static void
send_away (server *serv, char *reason)
{
	away_count++;
	g_free (sent_reason);
	sent_reason = g_strdup (reason);
}

static void
send_back (server *serv)
{
	back_count++;
}

int
fe_timeout_add_seconds (int interval, void *callback, void *data)
{
	g_assert_cmpint (interval, ==, 5);
	timer_adds++;
	return 42;
}

void
fe_timeout_remove (int tag)
{
	g_assert_cmpint (tag, ==, 42);
	timer_removes++;
}

gint64
fe_get_idle_time (void)
{
	return -1;
}

static void
setup (void)
{
	int i;

	prefs.hex_away_auto_timeout = 0;
	autoaway_reinit ();
	for (i = 0; i < 3; i++)
		g_free (servers[i].last_away_reason);
	g_slist_free (serv_list);
	serv_list = NULL;
	memset (servers, 0, sizeof servers);
	memset (&prefs, 0, sizeof prefs);
	away_count = back_count = timer_adds = timer_removes = 0;
	g_clear_pointer (&sent_reason, g_free);
	prefs.hex_away_auto_timeout = 10;
	prefs.hex_away_auto_system = TRUE;
	g_strlcpy (prefs.hex_away_auto_reason, "Auto idle", sizeof prefs.hex_away_auto_reason);
	for (i = 0; i < 3; i++)
	{
		servers[i].connected = TRUE;
		servers[i].end_of_motd = TRUE;
		servers[i].p_set_away = send_away;
		servers[i].p_set_back = send_back;
	}
	serv_list = g_slist_append (NULL, &servers[0]);
	autoaway_activity ();
	base = g_get_monotonic_time ();
}

static void
test_fallback_boundary (void)
{
	setup ();
	autoaway_check (base + 599 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 0);
	autoaway_check (base + 600 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 1);
	g_assert_true (servers[0].autoaway_owned);
	g_assert_cmpstr (sent_reason, ==, "Auto idle");
	/* An ACK is not required to avoid flooding the connection. */
	autoaway_check (base + 605 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 1);
	servers[0].is_away = TRUE;
	autoaway_check (base + 640 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 1);
	autoaway_activity ();
	autoaway_check (g_get_monotonic_time (), -1);
	g_assert_cmpint (back_count, ==, 1);
	g_assert_false (servers[0].autoaway_owned);
	g_assert_null (servers[0].last_away_reason);
}

static void
test_desktop_activity (void)
{
	setup ();
	autoaway_check (base + 900 * G_USEC_PER_SEC, 5 * G_USEC_PER_SEC);
	g_assert_cmpint (away_count, ==, 0);
	autoaway_check (base + 900 * G_USEC_PER_SEC, 700 * G_USEC_PER_SEC);
	g_assert_cmpint (away_count, ==, 1);
	servers[0].is_away = TRUE;
	autoaway_check (base + 905 * G_USEC_PER_SEC, 0);
	g_assert_cmpint (back_count, ==, 1);
	/* App input wins even if the native backend still has an old sample. */
	autoaway_activity ();
	autoaway_check (g_get_monotonic_time (), 900 * G_USEC_PER_SEC);
	g_assert_cmpint (away_count, ==, 1);
}

static void
test_app_mode (void)
{
	setup ();
	prefs.hex_away_auto_system = FALSE;
	autoaway_check (base + 600 * G_USEC_PER_SEC, 0);
	g_assert_cmpint (away_count, ==, 1);
}

static void
test_manual_away (void)
{
	setup ();
	servers[0].is_away = TRUE;
	servers[0].last_away_reason = g_strdup ("Lunch");
	autoaway_check (base + 900 * G_USEC_PER_SEC, -1);
	autoaway_check (base + 905 * G_USEC_PER_SEC, 0);
	prefs.hex_away_auto_timeout = 0;
	autoaway_reinit ();
	g_assert_cmpint (away_count, ==, 0);
	g_assert_cmpint (back_count, ==, 0);
	g_assert_cmpstr (servers[0].last_away_reason, ==, "Lunch");
}

static void
test_manual_takeover (void)
{
	setup ();
	autoaway_check (base + 600 * G_USEC_PER_SEC, -1);
	servers[0].is_away = TRUE;
	/* This is also called for /AWAY with no reason, even if already away. */
	autoaway_manual (&servers[0]);
	autoaway_check (g_get_monotonic_time (), 0);
	prefs.hex_away_auto_timeout = 0;
	autoaway_reinit ();
	g_assert_cmpint (back_count, ==, 0);
	g_assert_true (servers[0].is_away);
}

static void
test_pending_away_cancel (void)
{
	setup ();
	autoaway_check (base + 600 * G_USEC_PER_SEC, -1);
	g_assert_false (servers[0].is_away);
	autoaway_activity ();
	autoaway_check (g_get_monotonic_time (), -1);
	g_assert_cmpint (back_count, ==, 1);
	/* A late 306 must not make us own the away state again. */
	servers[0].is_away = TRUE;
	autoaway_check (g_get_monotonic_time (), -1);
	g_assert_cmpint (back_count, ==, 1);
}

static void
test_login_and_multiple_servers (void)
{
	setup ();
	serv_list = g_slist_append (serv_list, &servers[1]);
	serv_list = g_slist_append (serv_list, &servers[2]);
	servers[1].is_away = TRUE;
	servers[1].last_away_reason = g_strdup ("Manual");
	servers[2].end_of_motd = FALSE;
	autoaway_check (base + 600 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 1);
	servers[2].end_of_motd = TRUE;
	autoaway_check (base + 605 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 2);
	autoaway_check (base + 610 * G_USEC_PER_SEC, 0);
	g_assert_cmpint (back_count, ==, 2);
	g_assert_cmpstr (servers[1].last_away_reason, ==, "Manual");
}

static void
test_disconnect_and_reconnect (void)
{
	setup ();
	autoaway_check (base + 600 * G_USEC_PER_SEC, -1);
	servers[0].connected = FALSE;
	servers[0].is_away = TRUE;
	servers[0].reconnect_away = TRUE;
	autoaway_check (base + 605 * G_USEC_PER_SEC, -1);
	g_assert_false (servers[0].autoaway_owned);
	g_assert_false (servers[0].reconnect_away);
	g_assert_null (servers[0].last_away_reason);
	g_assert_cmpint (back_count, ==, 0);
	servers[0].is_away = FALSE;
	servers[0].connected = TRUE;
	servers[0].end_of_motd = FALSE;
	autoaway_check (base + 610 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 1);
	servers[0].end_of_motd = TRUE;
	autoaway_check (base + 615 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 2);
	/* server_set_defaults resets ownership even before the next timer tick. */
	servers[0].reconnect_away = TRUE;
	autoaway_reset (&servers[0]);
	g_assert_false (servers[0].reconnect_away);
	/* Existing manual reconnect-away must be left alone. */
	servers[0].reconnect_away = TRUE;
	servers[0].last_away_reason = g_strdup ("Manual");
	autoaway_reset (&servers[0]);
	g_assert_true (servers[0].reconnect_away);
	g_assert_cmpstr (servers[0].last_away_reason, ==, "Manual");
}

static void
test_retry (void)
{
	setup ();
	autoaway_check (base + 600 * G_USEC_PER_SEC, -1);
	autoaway_check (base + 629 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 1);
	autoaway_check (base + 630 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 2);
}

static void
test_disable_and_timers (void)
{
	setup ();
	autoaway_reinit ();
	autoaway_reinit ();
	g_assert_cmpint (timer_adds, ==, 1);
	base = g_get_monotonic_time ();
	autoaway_check (base + 600 * G_USEC_PER_SEC, -1);
	prefs.hex_away_auto_timeout = 0;
	autoaway_reinit ();
	g_assert_cmpint (back_count, ==, 1);
	g_assert_cmpint (timer_removes, ==, 1);
	g_assert_false (servers[0].autoaway_owned);
	autoaway_reinit ();
	g_assert_cmpint (back_count, ==, 1);
	prefs.hex_away_auto_timeout = -1;
	autoaway_reinit ();
	g_assert_cmpint (away_count, ==, 1);
}

static void
test_empty_reason_and_large_timeout (void)
{
	setup ();
	prefs.hex_away_auto_reason[0] = 0;
	autoaway_check (base + 600 * G_USEC_PER_SEC, -1);
	g_assert_nonnull (sent_reason);
	g_assert_cmpstr (sent_reason, !=, "");
	setup ();
	prefs.hex_away_auto_timeout = G_MAXINT;
	autoaway_check (base + 900 * G_USEC_PER_SEC, -1);
	g_assert_cmpint (away_count, ==, 0);
}

int
main (int argc, char **argv)
{
	int result, i;
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/autoaway/fallback-boundary", test_fallback_boundary);
	g_test_add_func ("/autoaway/desktop-activity", test_desktop_activity);
	g_test_add_func ("/autoaway/app-mode", test_app_mode);
	g_test_add_func ("/autoaway/manual-away", test_manual_away);
	g_test_add_func ("/autoaway/manual-takeover", test_manual_takeover);
	g_test_add_func ("/autoaway/pending-cancel", test_pending_away_cancel);
	g_test_add_func ("/autoaway/login-multiple-servers", test_login_and_multiple_servers);
	g_test_add_func ("/autoaway/reconnect", test_disconnect_and_reconnect);
	g_test_add_func ("/autoaway/retry", test_retry);
	g_test_add_func ("/autoaway/disable-timers", test_disable_and_timers);
	g_test_add_func ("/autoaway/empty-reason-large-timeout", test_empty_reason_and_large_timeout);
	result = g_test_run ();
	for (i = 0; i < 3; i++)
		g_free (servers[i].last_away_reason);
	g_free (sent_reason);
	g_slist_free (serv_list);
	return result;
}
