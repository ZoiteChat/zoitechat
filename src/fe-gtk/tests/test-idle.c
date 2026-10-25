/* ZoiteChat
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <gtk/gtk.h>
#include "../idle.h"

static int activity_count, dispatch_count;
static void test_gtk_dispatch (GdkEvent *event);

void
autoaway_activity (void)
{
	activity_count++;
}

/* Exercise the real handler without delivering synthetic input to a widget. */
#define gtk_main_do_event test_gtk_dispatch
#include "../idle.c"
#undef gtk_main_do_event

static void
test_gtk_dispatch (GdkEvent *event)
{
	dispatch_count++;
}

static void
test_user_input_and_dispatch (void)
{
	GdkEvent event = {0};
	GdkEventType input[] = {GDK_KEY_PRESS, GDK_BUTTON_PRESS, GDK_SCROLL,
	                       GDK_MOTION_NOTIFY, GDK_TOUCH_BEGIN, GDK_TOUCH_UPDATE};
	GdkEventType other[] = {GDK_FOCUS_CHANGE, GDK_CONFIGURE, GDK_WINDOW_STATE,
	                       GDK_PROPERTY_NOTIFY, GDK_VISIBILITY_NOTIFY};
	unsigned int i;

	activity_count = dispatch_count = 0;
	for (i = 0; i < G_N_ELEMENTS (input); i++)
	{
		event.type = input[i];
		idle_event (&event, NULL);
	}
	g_assert_cmpint (activity_count, ==, G_N_ELEMENTS (input));
	for (i = 0; i < G_N_ELEMENTS (other); i++)
	{
		event.type = other[i];
		idle_event (&event, NULL);
	}
	g_assert_cmpint (activity_count, ==, G_N_ELEMENTS (input));
	g_assert_cmpint (dispatch_count, ==, G_N_ELEMENTS (input) + G_N_ELEMENTS (other));
}

static void
test_native_idle (void)
{
	gint64 idle = fe_get_idle_time ();
	g_assert_cmpint (idle, >=, -1);
#if defined(HAVE_XSS) && defined(GDK_WINDOWING_X11)
	if (gdk_display_get_default () && GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
	{
		/* XWayland must use the app fallback, even if XSS is compiled in. */
		char *saved = g_strdup (g_getenv ("WAYLAND_DISPLAY"));
		g_unsetenv ("WAYLAND_DISPLAY");
		g_assert_cmpint (fe_get_idle_time (), >=, 0);
		g_setenv ("WAYLAND_DISPLAY", "wayland-test", TRUE);
		g_assert_cmpint (fe_get_idle_time (), ==, -1);
		if (saved)
			g_setenv ("WAYLAND_DISPLAY", saved, TRUE);
		else
			g_unsetenv ("WAYLAND_DISPLAY");
		g_free (saved);
	}
#endif
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	gtk_init_check (&argc, &argv);
	g_test_add_func ("/idle/user-input-and-dispatch", test_user_input_and_dispatch);
	g_test_add_func ("/idle/native-backend", test_native_idle);
	return g_test_run ();
}
