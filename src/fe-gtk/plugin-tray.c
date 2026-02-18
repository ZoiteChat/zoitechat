/* X-Chat
 * Copyright (C) 2006-2007 Peter Zelezny.
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

#include <string.h>
#include "../common/zoitechat-plugin.h"
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/inbound.h"
#include "../common/server.h"
#include "../common/fe.h"
#include "../common/util.h"
#include "../common/outbound.h"
#include "fe-gtk.h"
#include "pixmaps.h"
#include "maingui.h"
#include "menu.h"
#include "gtkutil.h"

#if HAVE_GTK3
#include <gio/gio.h>
#if defined(GTK_DISABLE_DEPRECATED)
typedef struct _GtkStatusIcon GtkStatusIcon;
#endif
#ifndef WIN32
#if defined(HAVE_AYATANA_APPINDICATOR)
#include <libayatana-appindicator/app-indicator.h>
#elif defined(HAVE_APPINDICATOR)
#include <libappindicator/app-indicator.h>
#endif
#endif
#endif

#define ICON_TRAY_PREFERENCES "zc-menu-preferences"
#define ICON_TRAY_QUIT "zc-menu-quit"

#ifndef WIN32
#include <unistd.h>
#endif

typedef enum	/* current icon status */
{
	TRAY_ICON_NONE,
	TRAY_ICON_NORMAL,
	TRAY_ICON_MESSAGE,
	TRAY_ICON_HIGHLIGHT,
	TRAY_ICON_FILEOFFER,
	TRAY_ICON_CUSTOM1,
	TRAY_ICON_CUSTOM2
} TrayIconState;

typedef enum
{
	WS_FOCUSED,
	WS_NORMAL,
	WS_HIDDEN
} WinStatus;

#if HAVE_GTK3 && !defined(WIN32) && (defined(HAVE_AYATANA_APPINDICATOR) || defined(HAVE_APPINDICATOR))
#define HAVE_APPINDICATOR_BACKEND 1
#else
#define HAVE_APPINDICATOR_BACKEND 0
#endif

#if HAVE_APPINDICATOR_BACKEND
/* GTK3: use AppIndicator/StatusNotifier item for tray integration. */
typedef GIcon *TrayIcon;
typedef GIcon *TrayCustomIcon;
#define tray_icon_free(i) g_object_unref(i)

#define ICON_NORMAL_NAME "net.zoite.Zoitechat"
#define ICON_MSG_NAME "mail-unread"
#define ICON_HILIGHT_NAME "dialog-warning"
#define ICON_FILE_NAME "folder-download"

static TrayIcon tray_icon_normal;
static TrayIcon tray_icon_msg;
static TrayIcon tray_icon_hilight;
static TrayIcon tray_icon_file;

#define ICON_NORMAL tray_icon_normal
#define ICON_MSG tray_icon_msg
#define ICON_HILIGHT tray_icon_hilight
#define ICON_FILE tray_icon_file
#endif

#if !HAVE_APPINDICATOR_BACKEND
typedef GdkPixbuf* TrayIcon;
typedef GdkPixbuf* TrayCustomIcon;
#define tray_icon_from_file(f) gdk_pixbuf_new_from_file(f,NULL)
#define tray_icon_free(i) g_object_unref(i)

#define ICON_NORMAL pix_tray_normal
#define ICON_MSG pix_tray_message
#define ICON_HILIGHT pix_tray_highlight
#define ICON_FILE pix_tray_fileoffer
#endif
#if HAVE_GTK3 && defined(GTK_DISABLE_DEPRECATED) && !HAVE_APPINDICATOR_BACKEND
GtkStatusIcon *gtk_status_icon_new_from_pixbuf (GdkPixbuf *pixbuf);
void gtk_status_icon_set_from_pixbuf (GtkStatusIcon *status_icon, GdkPixbuf *pixbuf);
void gtk_status_icon_set_tooltip_text (GtkStatusIcon *status_icon, const gchar *text);
gboolean gtk_status_icon_is_embedded (GtkStatusIcon *status_icon);
#endif
#define TIMEOUT 500

void tray_apply_setup (void);
static gboolean tray_menu_try_restore (void);
static void tray_cleanup (void);
static void tray_init (void);
static void tray_set_icon_state (TrayIcon icon, TrayIconState state);
static void tray_menu_restore_cb (GtkWidget *item, gpointer userdata);
static void tray_menu_notify_cb (GObject *tray, GParamSpec *pspec, gpointer user_data);
#if HAVE_APPINDICATOR_BACKEND
static void tray_menu_show_cb (GtkWidget *menu, gpointer userdata);
#endif
#if !HAVE_APPINDICATOR_BACKEND
static void tray_menu_cb (GtkWidget *widget, guint button, guint time, gpointer userdata);
#endif

typedef struct
{
	gboolean (*init)(void);
	void (*set_icon)(TrayIcon icon);
	void (*set_tooltip)(const char *text);
	gboolean (*is_embedded)(void);
	void (*cleanup)(void);
} TrayBackendOps;

#if HAVE_APPINDICATOR_BACKEND
static AppIndicator *tray_indicator;
static GtkWidget *tray_menu;
#endif
#if !HAVE_APPINDICATOR_BACKEND
static GtkStatusIcon *tray_status_icon;
#endif
static gboolean tray_backend_active = FALSE;

static gint flash_tag;
static TrayIconState tray_icon_state;
static TrayIcon tray_flash_icon;
static TrayIconState tray_flash_state;
#if defined(WIN32)
static guint tray_menu_timer;
static gint64 tray_menu_inactivetime;
#endif
static zoitechat_plugin *ph;

static TrayCustomIcon custom_icon1;
static TrayCustomIcon custom_icon2;

static int tray_priv_count = 0;
static int tray_pub_count = 0;
static int tray_hilight_count = 0;
static int tray_file_count = 0;
static int tray_restore_timer = 0;

#if HAVE_APPINDICATOR_BACKEND
static TrayCustomIcon
tray_icon_from_file (const char *filename)
{
	GFile *file;
	TrayCustomIcon icon;

	if (!filename)
		return NULL;

	file = g_file_new_for_path (filename);
	icon = g_file_icon_new (file);
	g_object_unref (file);

	return icon;
}

static char *
tray_gtk3_cache_pixbuf_icon (const char *basename, GdkPixbuf *pixbuf)
{
	char *cache_dir;
	char *filename;
	char *path;

	if (!pixbuf || !basename)
		return NULL;

	cache_dir = g_build_filename (g_get_user_cache_dir (), "zoitechat", "tray-icons", NULL);
	if (g_mkdir_with_parents (cache_dir, 0700) != 0)
	{
		g_free (cache_dir);
		return NULL;
	}

	filename = g_strdup_printf ("%s.png", basename);
	path = g_build_filename (cache_dir, filename, NULL);
	g_free (filename);
	g_free (cache_dir);

	if (!g_file_test (path, G_FILE_TEST_EXISTS))
		gdk_pixbuf_save (pixbuf, path, "png", NULL, NULL);

	return path;
}

static char *
tray_gtk3_fallback_icon_path_for_name (const char *name)
{
	if (g_strcmp0 (name, ICON_NORMAL_NAME) == 0)
		return tray_gtk3_cache_pixbuf_icon ("tray_normal", pix_tray_normal);
	if (g_strcmp0 (name, ICON_MSG_NAME) == 0)
		return tray_gtk3_cache_pixbuf_icon ("tray_message", pix_tray_message);
	if (g_strcmp0 (name, ICON_HILIGHT_NAME) == 0)
		return tray_gtk3_cache_pixbuf_icon ("tray_highlight", pix_tray_highlight);
	if (g_strcmp0 (name, ICON_FILE_NAME) == 0)
		return tray_gtk3_cache_pixbuf_icon ("tray_fileoffer", pix_tray_fileoffer);

	return NULL;
}

static void
tray_gtk3_icons_init (void)
{
	if (!tray_icon_normal)
		tray_icon_normal = g_themed_icon_new (ICON_NORMAL_NAME);
	if (!tray_icon_msg)
		tray_icon_msg = g_themed_icon_new (ICON_MSG_NAME);
	if (!tray_icon_hilight)
		tray_icon_hilight = g_themed_icon_new (ICON_HILIGHT_NAME);
	if (!tray_icon_file)
		tray_icon_file = g_themed_icon_new (ICON_FILE_NAME);
}

static void
tray_gtk3_icons_cleanup (void)
{
	g_clear_object (&tray_icon_normal);
	g_clear_object (&tray_icon_msg);
	g_clear_object (&tray_icon_hilight);
	g_clear_object (&tray_icon_file);
}

static GtkIconTheme *tray_gtk3_icon_theme = NULL;
static gulong tray_gtk3_icon_theme_changed_handler = 0;

static void
tray_gtk3_reapply_icon_state (void)
{
	switch (tray_icon_state)
	{
	case TRAY_ICON_NORMAL:
		tray_set_icon_state (ICON_NORMAL, TRAY_ICON_NORMAL);
		break;
	case TRAY_ICON_MESSAGE:
		tray_set_icon_state (ICON_MSG, TRAY_ICON_MESSAGE);
		break;
	case TRAY_ICON_HIGHLIGHT:
		tray_set_icon_state (ICON_HILIGHT, TRAY_ICON_HIGHLIGHT);
		break;
	case TRAY_ICON_FILEOFFER:
		tray_set_icon_state (ICON_FILE, TRAY_ICON_FILEOFFER);
		break;
	case TRAY_ICON_CUSTOM1:
		tray_set_icon_state (custom_icon1, TRAY_ICON_CUSTOM1);
		break;
	case TRAY_ICON_CUSTOM2:
		tray_set_icon_state (custom_icon2, TRAY_ICON_CUSTOM2);
		break;
	case TRAY_ICON_NONE:
	default:
		break;
	}
}

static void
tray_gtk3_theme_changed_cb (GtkIconTheme *theme, gpointer user_data)
{
	(void)theme;
	(void)user_data;

	if (!tray_backend_active)
		return;

	tray_gtk3_icons_cleanup ();
	tray_gtk3_icons_init ();
	tray_gtk3_reapply_icon_state ();
}

static const char *
tray_gtk3_icon_to_name (TrayIcon icon, char **allocated)
{
	const char * const *names;
	GtkIconTheme *theme;
	GFile *file;

	if (!icon)
		return NULL;

	if (G_IS_THEMED_ICON (icon))
	{
		names = g_themed_icon_get_names (G_THEMED_ICON (icon));
		if (names && names[0])
		{
			theme = gtk_icon_theme_get_default ();
			if (theme && gtk_icon_theme_has_icon (theme, names[0]))
				return names[0];

			*allocated = tray_gtk3_fallback_icon_path_for_name (names[0]);
			if (*allocated)
				return *allocated;
		}
	}

	if (G_IS_FILE_ICON (icon))
	{
		file = g_file_icon_get_file (G_FILE_ICON (icon));
		if (file)
		{
			*allocated = g_file_get_path (file);
			if (*allocated)
				return *allocated;
		}
	}

	*allocated = g_icon_to_string (icon);
	return *allocated;
}

static void
tray_app_indicator_set_icon (TrayIcon icon)
{
	char *icon_name_alloc = NULL;
	const char *icon_name;

	if (!tray_indicator)
		return;

	icon_name = tray_gtk3_icon_to_name (icon, &icon_name_alloc);
	if (!icon_name)
		return;

	app_indicator_set_icon_full (tray_indicator, icon_name, _(DISPLAY_NAME));
	app_indicator_set_status (tray_indicator, APP_INDICATOR_STATUS_ACTIVE);

	g_free (icon_name_alloc);
}

static void
tray_app_indicator_set_tooltip (const char *text)
{
	if (!tray_indicator)
		return;

	app_indicator_set_title (tray_indicator, text ? text : "");
}

static gboolean
tray_app_indicator_is_embedded (void)
{
	gboolean connected = TRUE;
	GObjectClass *klass;

	if (!tray_indicator)
		return FALSE;

	klass = G_OBJECT_GET_CLASS (tray_indicator);
	if (klass && g_object_class_find_property (klass, "connected"))
	{
		g_object_get (tray_indicator, "connected", &connected, NULL);
	}

	return connected;
}

static void
tray_app_indicator_cleanup (void)
{
	if (tray_indicator)
	{
		g_object_unref (tray_indicator);
		tray_indicator = NULL;
	}

	if (tray_menu)
	{
		gtk_widget_destroy (tray_menu);
		tray_menu = NULL;
	}
}

static gboolean
tray_app_indicator_init (void)
{
	GObjectClass *klass;

	tray_indicator = app_indicator_new ("zoitechat", ICON_NORMAL_NAME,
		APP_INDICATOR_CATEGORY_COMMUNICATIONS);
	if (!tray_indicator)
		return FALSE;

	tray_menu = gtk_menu_new ();
	g_signal_connect (G_OBJECT (tray_menu), "show",
		G_CALLBACK (tray_menu_show_cb), NULL);
	app_indicator_set_menu (tray_indicator, GTK_MENU (tray_menu));
	app_indicator_set_status (tray_indicator, APP_INDICATOR_STATUS_ACTIVE);

	klass = G_OBJECT_GET_CLASS (tray_indicator);
	if (klass && g_object_class_find_property (klass, "connected"))
	{
		g_signal_connect (G_OBJECT (tray_indicator), "notify::connected",
			G_CALLBACK (tray_menu_notify_cb), NULL);
	}

	return TRUE;
}

static const TrayBackendOps tray_backend_ops = {
	tray_app_indicator_init,
	tray_app_indicator_set_icon,
	tray_app_indicator_set_tooltip,
	tray_app_indicator_is_embedded,
	tray_app_indicator_cleanup
};
#endif

#if !HAVE_APPINDICATOR_BACKEND
static void
tray_status_icon_set_icon (TrayIcon icon)
{
	if (!tray_status_icon)
		return;

	gtk_status_icon_set_from_pixbuf (tray_status_icon, icon);
}

static void
tray_status_icon_set_tooltip (const char *text)
{
	if (!tray_status_icon)
		return;

	gtk_status_icon_set_tooltip_text (tray_status_icon, text);
}

static gboolean
tray_status_icon_is_embedded (void)
{
	if (!tray_status_icon)
		return FALSE;

	return gtk_status_icon_is_embedded (tray_status_icon);
}

static void
tray_status_icon_cleanup (void)
{
	if (tray_status_icon)
	{
		g_object_unref (tray_status_icon);
		tray_status_icon = NULL;
	}
}

static gboolean
tray_status_icon_init (void)
{
	tray_status_icon = gtk_status_icon_new_from_pixbuf (ICON_NORMAL);
	if (!tray_status_icon)
		return FALSE;

	g_signal_connect (G_OBJECT (tray_status_icon), "popup-menu",
		G_CALLBACK (tray_menu_cb), tray_status_icon);

	g_signal_connect (G_OBJECT (tray_status_icon), "activate",
		G_CALLBACK (tray_menu_restore_cb), NULL);

	g_signal_connect (G_OBJECT (tray_status_icon), "notify::embedded",
		G_CALLBACK (tray_menu_notify_cb), NULL);

	return TRUE;
}

static const TrayBackendOps tray_backend_ops = {
	tray_status_icon_init,
	tray_status_icon_set_icon,
	tray_status_icon_set_tooltip,
	tray_status_icon_is_embedded,
	tray_status_icon_cleanup
};
#endif

static gboolean
tray_backend_init (void)
{
	if (!tray_backend_ops.init)
		return FALSE;

#if HAVE_APPINDICATOR_BACKEND
	tray_gtk3_icons_init ();
	if (!tray_gtk3_icon_theme)
		tray_gtk3_icon_theme = gtk_icon_theme_get_default ();
	if (tray_gtk3_icon_theme && tray_gtk3_icon_theme_changed_handler == 0)
	{
		tray_gtk3_icon_theme_changed_handler = g_signal_connect (
			tray_gtk3_icon_theme,
			"changed",
			G_CALLBACK (tray_gtk3_theme_changed_cb),
			NULL);
	}
#endif
	tray_backend_active = tray_backend_ops.init ();
	return tray_backend_active;
}

static void
tray_backend_set_icon (TrayIcon icon)
{
	if (tray_backend_active && tray_backend_ops.set_icon)
		tray_backend_ops.set_icon (icon);
}

static void
tray_backend_set_tooltip (const char *text)
{
	if (tray_backend_active && tray_backend_ops.set_tooltip)
		tray_backend_ops.set_tooltip (text);
}

static gboolean
tray_backend_is_embedded (void)
{
	if (!tray_backend_active || !tray_backend_ops.is_embedded)
		return FALSE;

	return tray_backend_ops.is_embedded ();
}

static void
tray_backend_cleanup (void)
{
	if (tray_backend_ops.cleanup)
		tray_backend_ops.cleanup ();

#if HAVE_APPINDICATOR_BACKEND
	if (tray_gtk3_icon_theme && tray_gtk3_icon_theme_changed_handler)
	{
		g_signal_handler_disconnect (tray_gtk3_icon_theme,
			tray_gtk3_icon_theme_changed_handler);
		tray_gtk3_icon_theme_changed_handler = 0;
	}
	tray_gtk3_icon_theme = NULL;
	tray_gtk3_icons_cleanup ();
#endif
	tray_backend_active = FALSE;
}

static WinStatus
tray_get_window_status (void)
{
	const char *st;

	st = zoitechat_get_info (ph, "win_status");

	if (!st)
		return WS_HIDDEN;

	if (!strcmp (st, "active"))
		return WS_FOCUSED;

	if (!strcmp (st, "hidden"))
		return WS_HIDDEN;

	return WS_NORMAL;
}

static int
tray_count_channels (void)
{
	int cons = 0;
	GSList *list;
	session *sess;

	for (list = sess_list; list; list = list->next)
	{
		sess = list->data;
		if (sess->server->connected && sess->channel[0] &&
			 sess->type == SESS_CHANNEL)
			cons++;
	}
	return cons;
}

static int
tray_count_networks (void)
{
	int cons = 0;
	GSList *list;

	for (list = serv_list; list; list = list->next)
	{
		if (((server *)list->data)->connected)
			cons++;
	}
	return cons;
}

static void
tray_set_icon_state (TrayIcon icon, TrayIconState state)
{
	tray_backend_set_icon (icon);
	tray_icon_state = state;
}

static void
tray_set_custom_icon_state (TrayCustomIcon icon, TrayIconState state)
{
	tray_backend_set_icon (icon);
	tray_icon_state = state;
}

void
fe_tray_set_tooltip (const char *text)
{
	if (!tray_backend_active)
		return;

	tray_backend_set_tooltip (text);
}

static void
tray_set_tipf (const char *format, ...)
{
	va_list args;
	char *buf;

	va_start (args, format);
	buf = g_strdup_vprintf (format, args);
	va_end (args);

	fe_tray_set_tooltip (buf);
	g_free (buf);
}

static void
tray_stop_flash (void)
{
	int nets, chans;

	if (flash_tag)
	{
		g_source_remove (flash_tag);
		flash_tag = 0;
	}

	if (tray_backend_active)
	{
		tray_set_icon_state (ICON_NORMAL, TRAY_ICON_NORMAL);
		nets = tray_count_networks ();
		chans = tray_count_channels ();
		if (nets)
			tray_set_tipf (_("Connected to %u networks and %u channels - %s"),
								nets, chans, _(DISPLAY_NAME));
		else
			tray_set_tipf ("%s - %s", _("Not connected."), _(DISPLAY_NAME));
	}

	if (custom_icon1)
	{
		tray_icon_free (custom_icon1);
		custom_icon1 = NULL;
	}

	if (custom_icon2)
	{
		tray_icon_free (custom_icon2);
		custom_icon2 = NULL;
	}

	tray_flash_icon = NULL;
	tray_flash_state = TRAY_ICON_NONE;
}

static void
tray_reset_counts (void)
{
	tray_priv_count = 0;
	tray_pub_count = 0;
	tray_hilight_count = 0;
	tray_file_count = 0;
}

static int
tray_timeout_cb (gpointer userdata)
{
	(void)userdata;

	if (custom_icon1)
	{
		if (tray_icon_state == TRAY_ICON_CUSTOM1)
		{
			if (custom_icon2)
				tray_set_custom_icon_state (custom_icon2, TRAY_ICON_CUSTOM2);
			else
				tray_set_icon_state (ICON_NORMAL, TRAY_ICON_NORMAL);
		}
		else
		{
			tray_set_custom_icon_state (custom_icon1, TRAY_ICON_CUSTOM1);
		}
	}
	else
	{
		if (tray_icon_state == TRAY_ICON_NORMAL)
			tray_set_icon_state (tray_flash_icon, tray_flash_state);
		else
			tray_set_icon_state (ICON_NORMAL, TRAY_ICON_NORMAL);
	}
	return 1;
}

static void
tray_set_flash (TrayIcon icon, TrayIconState state)
{
	if (!tray_backend_active)
		return;

	/* already flashing the same icon */
	if (flash_tag && tray_icon_state == state)
		return;

	/* no flashing if window is focused */
	if (tray_get_window_status () == WS_FOCUSED)
		return;

	tray_stop_flash ();

	tray_flash_icon = icon;
	tray_flash_state = state;
	tray_set_icon_state (icon, state);
	if (prefs.hex_gui_tray_blink)
		flash_tag = g_timeout_add (TIMEOUT, (GSourceFunc) tray_timeout_cb, NULL);
}

void
fe_tray_set_flash (const char *filename1, const char *filename2, int tout)
{
	tray_apply_setup ();
	if (!tray_backend_active)
		return;

	tray_stop_flash ();

	if (tout == -1)
		tout = TIMEOUT;

	custom_icon1 = tray_icon_from_file (filename1);
	if (filename2)
		custom_icon2 = tray_icon_from_file (filename2);

	tray_set_custom_icon_state (custom_icon1, TRAY_ICON_CUSTOM1);
	flash_tag = g_timeout_add (tout, (GSourceFunc) tray_timeout_cb, NULL);
}

void
fe_tray_set_icon (feicon icon)
{
	tray_apply_setup ();
	if (!tray_backend_active)
		return;

	tray_stop_flash ();

	switch (icon)
	{
	case FE_ICON_NORMAL:
		break;
	case FE_ICON_MESSAGE:
	case FE_ICON_PRIVMSG:
		tray_set_flash (ICON_MSG, TRAY_ICON_MESSAGE);
		break;
	case FE_ICON_HIGHLIGHT:
		tray_set_flash (ICON_HILIGHT, TRAY_ICON_HIGHLIGHT);
		break;
	case FE_ICON_FILEOFFER:
		tray_set_flash (ICON_FILE, TRAY_ICON_FILEOFFER);
	}
}

void
fe_tray_set_file (const char *filename)
{
	tray_apply_setup ();
	if (!tray_backend_active)
		return;

	tray_stop_flash ();

	if (filename)
	{
		custom_icon1 = tray_icon_from_file (filename);
		tray_set_custom_icon_state (custom_icon1, TRAY_ICON_CUSTOM1);
	}
}

gboolean
tray_toggle_visibility (gboolean force_hide)
{
	static int x, y;
	static GdkScreen *screen;
	static int maximized;
	static int fullscreen;
	GtkWindow *win;

	if (!tray_backend_active)
		return FALSE;

	/* ph may have an invalid context now */
	zoitechat_set_context (ph, zoitechat_find_context (ph, NULL, NULL));

	win = GTK_WINDOW (zoitechat_get_info (ph, "gtkwin_ptr"));

	tray_stop_flash ();
	tray_reset_counts ();

	if (!win)
		return FALSE;

	if (force_hide || gtk_widget_get_visible (GTK_WIDGET (win)))
	{
		if (prefs.hex_gui_tray_away)
			zoitechat_command (ph, "ALLSERV AWAY");
		gtk_window_get_position (win, &x, &y);
		screen = gtk_window_get_screen (win);
		maximized = prefs.hex_gui_win_state;
		fullscreen = prefs.hex_gui_win_fullscreen;
		gtk_widget_hide (GTK_WIDGET (win));
	}
	else
	{
		if (prefs.hex_gui_tray_away)
			zoitechat_command (ph, "ALLSERV BACK");
		gtk_window_set_screen (win, screen);
		gtk_window_move (win, x, y);
		if (maximized)
			gtk_window_maximize (win);
		if (fullscreen)
			gtk_window_fullscreen (win);
		gtk_widget_show (GTK_WIDGET (win));
		gtk_window_deiconify (win);
		gtk_window_present (win);
	}

	return TRUE;
}

static void
tray_menu_restore_cb (GtkWidget *item, gpointer userdata)
{
	(void)item;
	(void)userdata;

	tray_toggle_visibility (FALSE);
}

static void
tray_menu_notify_cb (GObject *tray, GParamSpec *pspec, gpointer user_data)
{
	(void)tray;
	(void)pspec;
	(void)user_data;

	if (tray_backend_active)
	{
		if (!tray_backend_is_embedded ())
		{
			tray_restore_timer = g_timeout_add(500, (GSourceFunc)tray_menu_try_restore, NULL);
		}
		else
		{
			if (tray_restore_timer)
			{
				g_source_remove (tray_restore_timer);
				tray_restore_timer = 0;
			}
		}
	}
}

static gboolean
tray_menu_try_restore (void)
{
	tray_cleanup();
	tray_init();
	return TRUE;
}

static void
tray_menu_quit_cb (GtkWidget *item, gpointer userdata)
{
	(void)item;
	(void)userdata;

	mg_open_quit_dialog (FALSE);
}

/* returns 0-mixed 1-away 2-back */

static int
tray_find_away_status (void)
{
	GSList *list;
	server *serv;
	int away = 0;
	int back = 0;

	for (list = serv_list; list; list = list->next)
	{
		serv = list->data;

		if (serv->is_away || serv->reconnect_away)
			away++;
		else
			back++;
	}

	if (away && back)
		return 0;

	if (away)
		return 1;

	return 2;
}

static void
tray_foreach_server (GtkWidget *item, char *cmd)
{
	GSList *list;
	server *serv;

	for (list = serv_list; list; list = list->next)
	{
		serv = list->data;
		if (serv->connected)
			handle_command (serv->server_session, cmd, FALSE);
	}
}

static GtkWidget *
tray_make_item (GtkWidget *menu, char *label, void *callback, void *userdata)
{
	GtkWidget *item;

	if (label)
		item = gtk_menu_item_new_with_mnemonic (label);
	else
		item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate",
							G_CALLBACK (callback), userdata);
	gtk_widget_show (item);

	return item;
}

#ifndef WIN32
static void
tray_toggle_cb (GtkCheckMenuItem *item, unsigned int *setting)
{
	*setting = gtk_check_menu_item_get_active (item);
}

static void
blink_item (unsigned int *setting, GtkWidget *menu, char *label)
{
	menu_toggle_item (label, menu, tray_toggle_cb, setting, *setting);
}
#endif

static void
tray_menu_destroy (GtkWidget *menu, gpointer userdata)
{
	(void)userdata;

	gtk_widget_destroy (menu);
	g_object_unref (menu);
#ifdef WIN32
	g_source_remove (tray_menu_timer);
#endif
}

#ifdef WIN32
static gboolean
tray_menu_enter_cb (GtkWidget *menu)
{
	(void)menu;

	tray_menu_inactivetime = 0;
	return FALSE;
}

static gboolean
tray_menu_left_cb (GtkWidget *menu)
{
	(void)menu;

	tray_menu_inactivetime = g_get_real_time ();
	return FALSE;
}

static gboolean
tray_check_hide (GtkWidget *menu)
{
	if (tray_menu_inactivetime && g_get_real_time () - tray_menu_inactivetime  >= 2000000)
	{
		tray_menu_destroy (menu, NULL);
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}
#endif

static void
tray_menu_settings (GtkWidget * wid, gpointer none)
{
	(void)wid;
	(void)none;

	extern void setup_open (void);
	setup_open ();
}

static void
tray_menu_populate (GtkWidget *menu)
{
	GtkWidget *submenu;
	GtkWidget *item;
	int away_status;

	/* ph may have an invalid context now */
	zoitechat_set_context (ph, zoitechat_find_context (ph, NULL, NULL));

	if (tray_get_window_status () == WS_HIDDEN)
		tray_make_item (menu, _("_Restore Window"), tray_menu_restore_cb, NULL);
	else
		tray_make_item (menu, _("_Hide Window"), tray_menu_restore_cb, NULL);
	tray_make_item (menu, NULL, tray_menu_quit_cb, NULL);

#ifndef WIN32 /* submenus are buggy on win32 */
	submenu = mg_submenu (menu, _("_Blink on"));
	blink_item (&prefs.hex_input_tray_chans, submenu, _("Channel Message"));
	blink_item (&prefs.hex_input_tray_priv, submenu, _("Private Message"));
	blink_item (&prefs.hex_input_tray_hilight, submenu, _("Highlighted Message"));
	/*blink_item (BIT_FILEOFFER, submenu, _("File Offer"));*/

	submenu = mg_submenu (menu, _("_Change status"));
#else /* so show away/back in main tray menu */
	submenu = menu;
#endif

	away_status = tray_find_away_status ();
	item = tray_make_item (submenu, _("_Away"), tray_foreach_server, "away");
	if (away_status == 1)
		gtk_widget_set_sensitive (item, FALSE);
	item = tray_make_item (submenu, _("_Back"), tray_foreach_server, "back");
	if (away_status == 2)
		gtk_widget_set_sensitive (item, FALSE);

	menu_add_plugin_items (menu, "\x5$TRAY", NULL);

	tray_make_item (menu, NULL, tray_menu_quit_cb, NULL);
	mg_create_icon_item (_("_Preferences"), ICON_TRAY_PREFERENCES, menu, tray_menu_settings, NULL);
	tray_make_item (menu, NULL, tray_menu_quit_cb, NULL);
	mg_create_icon_item (_("_Quit"), ICON_TRAY_QUIT, menu, tray_menu_quit_cb, NULL);
}

#if HAVE_GTK3 && !defined(WIN32)
static void
tray_menu_clear (GtkWidget *menu)
{
	GList *children;
	GList *iter;

	children = gtk_container_get_children (GTK_CONTAINER (menu));
	for (iter = children; iter; iter = iter->next)
		gtk_widget_destroy (GTK_WIDGET (iter->data));
	g_list_free (children);
}

static void
tray_menu_show_cb (GtkWidget *menu, gpointer userdata)
{
	(void)userdata;

	tray_menu_clear (menu);
	tray_menu_populate (menu);
}
#endif

#if !HAVE_APPINDICATOR_BACKEND
static void
tray_menu_cb (GtkWidget *widget, guint button, guint time, gpointer userdata)
{
	static GtkWidget *menu;

	(void)widget;

	/* close any old menu */
	if (G_IS_OBJECT (menu))
	{
		tray_menu_destroy (menu, NULL);
	}

	menu = gtk_menu_new ();
	/*gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (widget));*/
	tray_menu_populate (menu);

	g_object_ref (menu);
	g_object_ref_sink (menu);
	g_object_unref (menu);
	g_signal_connect (G_OBJECT (menu), "selection-done",
		G_CALLBACK (tray_menu_destroy), NULL);
#ifdef WIN32
	g_signal_connect (G_OBJECT (menu), "leave-notify-event",
		G_CALLBACK (tray_menu_left_cb), NULL);
	g_signal_connect (G_OBJECT (menu), "enter-notify-event",
		G_CALLBACK (tray_menu_enter_cb), NULL);

	tray_menu_timer = g_timeout_add (500, (GSourceFunc)tray_check_hide, menu);
#endif

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
		userdata, button, time);
}
#endif

static void
tray_init (void)
{
	flash_tag = 0;
	tray_icon_state = TRAY_ICON_NONE;
	tray_flash_icon = NULL;
	tray_flash_state = TRAY_ICON_NONE;
	custom_icon1 = NULL;
	custom_icon2 = NULL;

	if (!tray_backend_init ())
		return;
	tray_icon_state = TRAY_ICON_NORMAL;
	tray_set_icon_state (ICON_NORMAL, TRAY_ICON_NORMAL);
}

static int
tray_hilight_cb (char *word[], void *userdata)
{
	/*if (tray_icon_state == TRAY_ICON_HIGHLIGHT)
		return ZOITECHAT_EAT_NONE;*/

	if (prefs.hex_input_tray_hilight)
	{
		tray_set_flash (ICON_HILIGHT, TRAY_ICON_HIGHLIGHT);

		/* FIXME: hides any previous private messages */
		tray_hilight_count++;
		if (tray_hilight_count == 1)
			tray_set_tipf (_("Highlighted message from: %s (%s) - %s"),
								word[1], zoitechat_get_info (ph, "channel"), _(DISPLAY_NAME));
		else
			tray_set_tipf (_("%u highlighted messages, latest from: %s (%s) - %s"),
								tray_hilight_count, word[1], zoitechat_get_info (ph, "channel"),
								_(DISPLAY_NAME));
	}

	return ZOITECHAT_EAT_NONE;
}

static int
tray_message_cb (char *word[], void *userdata)
{
	if (/*tray_icon_state == TRAY_ICON_MESSAGE ||*/ tray_icon_state == TRAY_ICON_HIGHLIGHT)
		return ZOITECHAT_EAT_NONE;
		
	if (prefs.hex_input_tray_chans)
	{
		tray_set_flash (ICON_MSG, TRAY_ICON_MESSAGE);

		tray_pub_count++;
		if (tray_pub_count == 1)
			tray_set_tipf (_("Channel message from: %s (%s) - %s"),
								word[1], zoitechat_get_info (ph, "channel"), _(DISPLAY_NAME));
		else
			tray_set_tipf (_("%u channel messages. - %s"), tray_pub_count, _(DISPLAY_NAME));
	}

	return ZOITECHAT_EAT_NONE;
}

static void
tray_priv (char *from, char *text)
{
	const char *network;

	if (alert_match_word (from, prefs.hex_irc_no_hilight))
		return;

	network = zoitechat_get_info (ph, "network");
	if (!network)
		network = zoitechat_get_info (ph, "server");

	if (prefs.hex_input_tray_priv)
	{
		tray_set_flash (ICON_MSG, TRAY_ICON_MESSAGE);

		tray_priv_count++;
		if (tray_priv_count == 1)
			tray_set_tipf (_("Private message from: %s (%s) - %s"), from,
								network, _(DISPLAY_NAME));
		else
			tray_set_tipf (_("%u private messages, latest from: %s (%s) - %s"),
								tray_priv_count, from, network, _(DISPLAY_NAME));
	}
}

static int
tray_priv_cb (char *word[], void *userdata)
{
	tray_priv (word[1], word[2]);

	return ZOITECHAT_EAT_NONE;
}

static int
tray_invited_cb (char *word[], void *userdata)
{
	if (!prefs.hex_away_omit_alerts || tray_find_away_status () != 1)
		tray_priv (word[2], "Invited");

	return ZOITECHAT_EAT_NONE;
}

static int
tray_dcc_cb (char *word[], void *userdata)
{
	const char *network;

/*	if (tray_icon_state == TRAY_ICON_FILEOFFER)
		return ZOITECHAT_EAT_NONE;*/

	network = zoitechat_get_info (ph, "network");
	if (!network)
		network = zoitechat_get_info (ph, "server");

	if (prefs.hex_input_tray_priv && (!prefs.hex_away_omit_alerts || tray_find_away_status () != 1))
	{
		tray_set_flash (ICON_FILE, TRAY_ICON_FILEOFFER);

		tray_file_count++;
		if (tray_file_count == 1)
			tray_set_tipf (_("File offer from: %s (%s) - %s"), word[1], network,
								_(DISPLAY_NAME));
		else
			tray_set_tipf (_("%u file offers, latest from: %s (%s) - %s"),
								tray_file_count, word[1], network, _(DISPLAY_NAME));
	}

	return ZOITECHAT_EAT_NONE;
}

static int
tray_focus_cb (char *word[], void *userdata)
{
	tray_stop_flash ();
	tray_reset_counts ();
	return ZOITECHAT_EAT_NONE;
}

static void
tray_cleanup (void)
{
	tray_stop_flash ();

	if (tray_backend_active)
		tray_backend_cleanup ();
}

void
tray_apply_setup (void)
{
	if (tray_backend_active)
	{
		if (!prefs.hex_gui_tray)
			tray_cleanup ();
	}
	else
	{
		GtkWindow *window = GTK_WINDOW(zoitechat_get_info (ph, "gtkwin_ptr"));
		if (prefs.hex_gui_tray && gtkutil_tray_icon_supported (window))
			tray_init ();
	}
}

int
tray_plugin_init (zoitechat_plugin *plugin_handle, char **plugin_name,
				char **plugin_desc, char **plugin_version, char *arg)
{
	/* we need to save this for use with any zoitechat_* functions */
	ph = plugin_handle;

	*plugin_name = "";
	*plugin_desc = "";
	*plugin_version = "";

	zoitechat_hook_print (ph, "Channel Msg Hilight", -1, tray_hilight_cb, NULL);
	zoitechat_hook_print (ph, "Channel Action Hilight", -1, tray_hilight_cb, NULL);

	zoitechat_hook_print (ph, "Channel Message", -1, tray_message_cb, NULL);
	zoitechat_hook_print (ph, "Channel Action", -1, tray_message_cb, NULL);
	zoitechat_hook_print (ph, "Channel Notice", -1, tray_message_cb, NULL);

	zoitechat_hook_print (ph, "Private Message", -1, tray_priv_cb, NULL);
	zoitechat_hook_print (ph, "Private Message to Dialog", -1, tray_priv_cb, NULL);
	zoitechat_hook_print (ph, "Private Action", -1, tray_priv_cb, NULL);
	zoitechat_hook_print (ph, "Private Action to Dialog", -1, tray_priv_cb, NULL);
	zoitechat_hook_print (ph, "Notice", -1, tray_priv_cb, NULL);
	zoitechat_hook_print (ph, "Invited", -1, tray_invited_cb, NULL);

	zoitechat_hook_print (ph, "DCC Offer", -1, tray_dcc_cb, NULL);

	zoitechat_hook_print (ph, "Focus Window", -1, tray_focus_cb, NULL);

	GtkWindow *window = GTK_WINDOW(zoitechat_get_info (ph, "gtkwin_ptr"));
	if (prefs.hex_gui_tray && gtkutil_tray_icon_supported (window))
		tray_init ();

	return 1;       /* return 1 for success */
}

int
tray_plugin_deinit (zoitechat_plugin *plugin_handle)
{
#ifdef WIN32
	tray_cleanup ();
#endif
	return 1;
}
