/* ZoiteChat
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <gtk/gtk.h>
#include "../common/autoaway.h"
#include "idle.h"

#ifdef WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <math.h>
#elif defined(HAVE_XSS) && defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#include <X11/extensions/scrnsaver.h>
#endif

/* Do not count focus changes, redraws, timers or incoming IRC traffic. */
static void
idle_event (GdkEvent *event, gpointer data)
{
	switch (event->type)
	{
	case GDK_KEY_PRESS:
	case GDK_BUTTON_PRESS:
	case GDK_SCROLL:
	case GDK_MOTION_NOTIFY:
	case GDK_TOUCH_BEGIN:
	case GDK_TOUCH_UPDATE:
		autoaway_activity ();
		break;
	default:
		break;
	}

	gtk_main_do_event (event);
}

void
idle_init (void)
{
	autoaway_activity ();
	/* Keep GTK's normal dispatch, including detached windows and dialogs. */
	gdk_event_handler_set (idle_event, NULL, NULL);
}

/* Microseconds since desktop input, or -1 for the portable app fallback. */
gint64
fe_get_idle_time (void)
{
#ifdef WIN32
	LASTINPUTINFO input = {0};
	DWORD elapsed;

	input.cbSize = sizeof input;
	if (!GetLastInputInfo (&input))
		return -1;

	/* DWORD subtraction handles the 49-day GetTickCount wrap. Some injected
	 * input has a slightly newer timestamp; do not treat it as weeks idle. */
	elapsed = GetTickCount () - input.dwTime;
	if ((gint32)elapsed < 0)
		elapsed = 0;
	return (gint64)elapsed * 1000;
#elif defined(__APPLE__)
	CFTimeInterval seconds = CGEventSourceSecondsSinceLastEventType (
		kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType);
	if (!isfinite (seconds) || seconds < 0 ||
	    seconds >= (double)G_MAXINT64 / G_USEC_PER_SEC)
		return -1;
	return (gint64)(seconds * G_USEC_PER_SEC);
#elif defined(HAVE_XSS) && defined(GDK_WINDOWING_X11)
	GdkDisplay *display = gdk_display_get_default ();
	XScreenSaverInfo info = {0};
	Display *xdisplay;
	int event_base, error_base, error;
	Status ok;
	const char *wayland = g_getenv ("WAYLAND_DISPLAY");

	/* XWayland idle is not desktop-wide. Never pass a Wayland display to Xlib. */
	if (!display || !GDK_IS_X11_DISPLAY (display) || (wayland && *wayland))
		return -1;

	xdisplay = gdk_x11_display_get_xdisplay (display);
	gdk_x11_display_error_trap_push (display);
	ok = XScreenSaverQueryExtension (xdisplay, &event_base, &error_base);
	if (ok)
		ok = XScreenSaverQueryInfo (xdisplay, DefaultRootWindow (xdisplay), &info);
	error = gdk_x11_display_error_trap_pop (display);
	if (!ok || error)
		return -1;
	return (gint64)info.idle * 1000;
#else
	return -1;
#endif
}
