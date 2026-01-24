/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fe-gtk.h"

#ifdef WIN32
#include <gdk/gdkwin32.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../common/zoitechat.h"
#include "../common/fe.h"
#include "../common/util.h"
#include "../common/text.h"
#include "../common/cfgfiles.h"
#include "../common/zoitechatc.h"
#include "../common/plugin.h"
#include "../common/server.h"
#include "../common/url.h"
#include "gtkutil.h"
#include "maingui.h"
#include "pixmaps.h"
#include "chanlist.h"
#include "joind.h"
#include "xtext.h"
#include "palette.h"
#include "menu.h"
#include "notifygui.h"
#include "textgui.h"
#include "fkeys.h"
#include "plugin-tray.h"
#include "urlgrab.h"
#include "setup.h"
#include "plugin-notification.h"

#ifdef USE_LIBCANBERRA
#include <canberra.h>
#endif

cairo_surface_t *channelwin_pix;

#ifdef USE_LIBCANBERRA
static ca_context *ca_con;
#endif

#ifdef HAVE_GTK_MAC
GtkosxApplication *osx_app;
#endif

/* === command-line parameter parsing : requires glib 2.6 === */

static char *arg_cfgdir = NULL;
static gint arg_show_autoload = 0;
static gint arg_show_config = 0;
static gint arg_show_version = 0;
static gint arg_minimize = 0;

static const GOptionEntry gopt_entries[] = 
{
 {"no-auto",	'a', 0, G_OPTION_ARG_NONE,	&arg_dont_autoconnect, N_("Don't auto connect to servers"), NULL},
 {"cfgdir",	'd', 0, G_OPTION_ARG_STRING,	&arg_cfgdir, N_("Use a different config directory"), "PATH"},
 {"no-plugins",	'n', 0, G_OPTION_ARG_NONE,	&arg_skip_plugins, N_("Don't auto load any plugins"), NULL},
 {"plugindir",	'p', 0, G_OPTION_ARG_NONE,	&arg_show_autoload, N_("Show plugin/script auto-load directory"), NULL},
 {"configdir",	'u', 0, G_OPTION_ARG_NONE,	&arg_show_config, N_("Show user config directory"), NULL},
 {"url",	 0,  G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_url, N_("Open an irc://server:port/channel?key URL"), "URL"},
 {"command",	'c', 0, G_OPTION_ARG_STRING,	&arg_command, N_("Execute command:"), "COMMAND"},
#ifdef USE_DBUS
 {"existing",	'e', 0, G_OPTION_ARG_NONE,	&arg_existing, N_("Open URL or execute command in an existing ZoiteChat"), NULL},
#endif
 {"minimize",	 0,  0, G_OPTION_ARG_INT,	&arg_minimize, N_("Begin minimized. Level 0=Normal 1=Iconified 2=Tray"), N_("level")},
 {"version",	'v', 0, G_OPTION_ARG_NONE,	&arg_show_version, N_("Show version information"), NULL},
 {G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &arg_urls, N_("Open an irc://server:port/channel?key URL"), "URL"},
 {NULL}
};

#ifdef WIN32
static void
create_msg_dialog (gchar *title, gchar *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", message);
	gtk_window_set_title (GTK_WINDOW (dialog), title);

/* On Win32 we automatically have the icon. If we try to load it explicitly, it will look ugly for some reason. */
#ifndef WIN32
	pixmaps_init ();
	gtk_window_set_icon (GTK_WINDOW (dialog), pix_zoitechat);
#endif

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}
#endif

int
fe_args (int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	char *buffer;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	context = g_option_context_new (NULL);
#ifdef WIN32
	g_option_context_set_help_enabled (context, FALSE);	/* disable stdout help as stdout is unavailable for subsystem:windows */
#endif
	g_option_context_add_main_entries (context, gopt_entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_parse (context, &argc, &argv, &error);

#ifdef WIN32
	if (error)											/* workaround for argv not being available when using subsystem:windows */
	{
		if (error->message)								/* the error message contains argv so search for patterns in that */
		{
			if (strstr (error->message, "--help-all") != NULL)
			{
				buffer = g_option_context_get_help (context, FALSE, NULL);
				gtk_init (&argc, &argv);
				create_msg_dialog ("Long Help", buffer);
				g_free (buffer);
				return 0;
			}
			else if (strstr (error->message, "--help") != NULL || strstr (error->message, "-?") != NULL)
			{
				buffer = g_option_context_get_help (context, TRUE, NULL);
				gtk_init (&argc, &argv);
				create_msg_dialog ("Help", buffer);
				g_free (buffer);
				return 0;
			}
			else 
			{
				buffer = g_strdup_printf ("%s\n", error->message);
				gtk_init (&argc, &argv);
				create_msg_dialog ("Error", buffer);
				g_free (buffer);
				return 1;
			}
		}
	}
#else
	if (error)
	{
		if (error->message)
			printf ("%s\n", error->message);
		return 1;
	}
#endif

	g_option_context_free (context);

	if (arg_show_version)
	{
		buffer = g_strdup_printf ("%s %s", PACKAGE_NAME, PACKAGE_VERSION);
#ifdef WIN32
		gtk_init (&argc, &argv);
		create_msg_dialog ("Version Information", buffer);
#else
		puts (buffer);
#endif
		g_free (buffer);

		return 0;
	}

	if (arg_show_autoload)
	{
		buffer = g_strdup_printf ("%s%caddons%c", get_xdir(), G_DIR_SEPARATOR, G_DIR_SEPARATOR);
#ifdef WIN32
		gtk_init (&argc, &argv);
		create_msg_dialog ("Plugin/Script Auto-load Directory", buffer);
#else
		puts (buffer);
#endif
		g_free (buffer);

		return 0;
	}

	if (arg_show_config)
	{
		buffer = g_strdup_printf ("%s%c", get_xdir(), G_DIR_SEPARATOR);
#ifdef WIN32
		gtk_init (&argc, &argv);
		create_msg_dialog ("User Config Directory", buffer);
#else
		puts (buffer);
#endif
		g_free (buffer);

		return 0;
	}

#ifdef WIN32
	/* this is mainly for irc:// URL handling. When windows calls us from */
	/* I.E, it doesn't give an option of "Start in" directory, like short */
	/* cuts can. So we have to set the current dir manually, to the path  */
	/* of the exe. */
	{
		char *tmp = g_strdup (argv[0]);
		char *sl;

		sl = strrchr (tmp, G_DIR_SEPARATOR);
		if (sl)
		{
			*sl = 0;
			chdir (tmp);
		}
		g_free (tmp);
	}
#endif

	gtk_init (&argc, &argv);

#ifdef HAVE_GTK_MAC
	osx_app = g_object_new(GTKOSX_TYPE_APPLICATION, NULL);
#endif

	return -1;
}

const char cursor_color_rc[] =
	"style \"xc-ib-st\""
	"{"
		"GtkEntry::cursor-color=\"#%02x%02x%02x\""
	"}"
	"widget \"*.zoitechat-inputbox\" style : application \"xc-ib-st\"";

static const char adwaita_workaround_rc[] =
	"style \"zoitechat-input-workaround\""
	"{"
		"engine \"pixmap\" {"
			"image {"
				"function = FLAT_BOX\n"
				"state    = NORMAL\n"
			"}"
			"image {"
				"function = FLAT_BOX\n"
				"state    = ACTIVE\n"
			"}"
		"}"
	"}"
	"widget \"*.zoitechat-inputbox\" style \"zoitechat-input-workaround\"";

static gboolean
fe_system_prefers_dark (void)
{
	GtkSettings *settings = gtk_settings_get_default ();
	gboolean prefer_dark = FALSE;
	char *theme_name = NULL;
#ifdef G_OS_WIN32
	gboolean have_win_pref = FALSE;
#endif

	if (!settings)
		return FALSE;

#ifdef G_OS_WIN32
	{
		DWORD value = 1;
		DWORD value_size = sizeof (value);
		LSTATUS status;

		status = RegGetValueW (HKEY_CURRENT_USER,
		                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		                       L"AppsUseLightTheme",
		                       RRF_RT_REG_DWORD,
		                       NULL,
		                       &value,
		                       &value_size);
		if (status != ERROR_SUCCESS)
			status = RegGetValueW (HKEY_CURRENT_USER,
			                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
			                       L"SystemUsesLightTheme",
			                       RRF_RT_REG_DWORD,
			                       NULL,
			                       &value,
			                       &value_size);

		if (status == ERROR_SUCCESS)
		{
			prefer_dark = (value == 0);
			have_win_pref = TRUE;
		}
	}
#endif

#ifdef G_OS_WIN32
	if (!have_win_pref)
#endif
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (settings),
	                                  "gtk-application-prefer-dark-theme"))
	{
		g_object_get (settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
	}

	if (!prefer_dark)
	{
		g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
		if (theme_name)
		{
			char *lower = g_ascii_strdown (theme_name, -1);
			if (g_str_has_suffix (lower, "-dark") || g_strrstr (lower, "dark"))
				prefer_dark = TRUE;
			g_free (lower);
			g_free (theme_name);
		}
	}

	return prefer_dark;
}

static gboolean auto_dark_mode_enabled = FALSE;

#ifndef G_OS_WIN32
static gboolean gtk_prefer_dark_initial = FALSE;
static gboolean gtk_prefer_dark_initial_valid = FALSE;

static void
fe_capture_gtk_dark_preference (GtkSettings *settings)
{
	if (!settings || gtk_prefer_dark_initial_valid)
		return;

	if (!g_object_class_find_property (G_OBJECT_GET_CLASS (settings),
	                                   "gtk-application-prefer-dark-theme"))
		return;

	g_object_get (settings, "gtk-application-prefer-dark-theme", &gtk_prefer_dark_initial, NULL);
	gtk_prefer_dark_initial_valid = TRUE;
}
#endif

#ifdef G_OS_WIN32
static char *fe_win32_light_theme = NULL;
static char *fe_win32_dark_theme = NULL;
static char *fe_win32_light_gtkrc = NULL;
static char *fe_win32_dark_gtkrc = NULL;
static char *fe_win32_original_gtkrc_env = NULL;

static const char *
fe_win32_get_gtkrc_dir (void)
{
	static char *gtkrc_dir = NULL;

	if (!gtkrc_dir)
	{
		char *install_dir = g_win32_get_package_installation_directory_of_module (NULL);
		if (!install_dir)
			install_dir = g_strdup (".");

		gtkrc_dir = g_build_filename (install_dir, "etc", "gtk-2.0", NULL);
		g_free (install_dir);
	}

	return gtkrc_dir;
}

static char *
fe_win32_get_gtkrc_path (const char *name)
{
	const char *gtkrc_dir = fe_win32_get_gtkrc_dir ();

	if (!gtkrc_dir || !name || name[0] == '\0')
		return NULL;

	return g_build_filename (gtkrc_dir, name, NULL);
}

static char *
fe_win32_extract_theme_name (const char *gtkrc_path)
{
	char *contents = NULL;
	char *match = NULL;
	char *result = NULL;
	GMatchInfo *match_info = NULL;
	GError *error = NULL;
	static const char *patterns[] = {
		"gtk-theme-name\\s*=\\s*\"([^\"]+)\"",
		"gtk-theme-name\\s*=\\s*'([^']+)'"
	};
	gsize len = 0;
	int i;

	if (!gtkrc_path || !g_file_test (gtkrc_path, G_FILE_TEST_EXISTS))
		return NULL;

	if (!g_file_get_contents (gtkrc_path, &contents, &len, &error))
	{
		g_clear_error (&error);
		return NULL;
	}

	for (i = 0; i < (int) G_N_ELEMENTS (patterns); i++)
	{
		GRegex *regex = g_regex_new (patterns[i], G_REGEX_MULTILINE, 0, NULL);
		if (!regex)
			continue;

		if (g_regex_match (regex, contents, 0, &match_info))
		{
			match = g_match_info_fetch (match_info, 1);
			if (match && match[0] != '\0')
			{
				result = g_strdup (match);
				g_free (match);
				g_match_info_free (match_info);
				g_regex_unref (regex);
				break;
			}
		}

		g_free (match);
		if (match_info)
			g_match_info_free (match_info);
		match_info = NULL;
		g_regex_unref (regex);
	}

	g_free (contents);
	return result;
}

static gboolean
fe_win32_theme_exists (const char *theme_name)
{
	const char *home_dir = g_get_home_dir ();
	const char *user_data_dir = g_get_user_data_dir ();
	const char * const *system_dirs = g_get_system_data_dirs ();
	char *theme_path = NULL;
	gboolean found = FALSE;
	int i;

	if (!theme_name || theme_name[0] == '\0')
		return FALSE;

	if (home_dir && home_dir[0])
	{
		theme_path = g_build_filename (home_dir, ".themes", theme_name, "gtk-2.0", "gtkrc", NULL);
		found = g_file_test (theme_path, G_FILE_TEST_EXISTS);
		g_free (theme_path);
		if (found)
			return TRUE;
	}

	if (user_data_dir && user_data_dir[0])
	{
		theme_path = g_build_filename (user_data_dir, "themes", theme_name, "gtk-2.0", "gtkrc", NULL);
		found = g_file_test (theme_path, G_FILE_TEST_EXISTS);
		g_free (theme_path);
		if (found)
			return TRUE;
	}

	for (i = 0; system_dirs && system_dirs[i]; i++)
	{
		theme_path = g_build_filename (system_dirs[i], "themes", theme_name, "gtk-2.0", "gtkrc", NULL);
		found = g_file_test (theme_path, G_FILE_TEST_EXISTS);
		g_free (theme_path);
		if (found)
			return TRUE;
	}

	return FALSE;
}

static char *
fe_win32_strip_dark_suffix (const char *theme_name)
{
	size_t len;

	if (!theme_name)
		return NULL;

	len = strlen (theme_name);
	if (len > 5 && g_ascii_strcasecmp (theme_name + len - 5, "-dark") == 0)
		return g_strndup (theme_name, len - 5);

	return NULL;
}

static void
fe_win32_update_theme_cache (GtkSettings *settings)
{
	char *current_theme = NULL;
	char *light_theme = NULL;
	char *derived_dark = NULL;
	char *explicit_light_theme = NULL;
	char *explicit_dark_theme = NULL;
	char *gtkrc_light = NULL;
	char *gtkrc_dark = NULL;
	char *suffix = NULL;
	gboolean have_explicit_light;
	gboolean have_explicit_dark;
	int i;
	static const char *dark_suffixes[] = { "-Dark", "-dark" };

	if (!settings)
		return;

	g_object_get (settings, "gtk-theme-name", &current_theme, NULL);
	if (!current_theme || current_theme[0] == '\0')
	{
		g_free (current_theme);
		return;
	}

	gtkrc_light = fe_win32_get_gtkrc_path ("gtkrc.light");
	gtkrc_dark = fe_win32_get_gtkrc_path ("gtkrc.dark");
	explicit_light_theme = fe_win32_extract_theme_name (gtkrc_light);
	explicit_dark_theme = fe_win32_extract_theme_name (gtkrc_dark);

	if (explicit_light_theme && !fe_win32_theme_exists (explicit_light_theme))
	{
		g_free (explicit_light_theme);
		explicit_light_theme = NULL;
	}
	if (explicit_dark_theme && !fe_win32_theme_exists (explicit_dark_theme))
	{
		g_free (explicit_dark_theme);
		explicit_dark_theme = NULL;
	}

	have_explicit_light = explicit_light_theme != NULL;
	have_explicit_dark = explicit_dark_theme != NULL;

	if (have_explicit_light || have_explicit_dark)
	{
		char *stripped_dark = NULL;

		if (have_explicit_light)
			light_theme = explicit_light_theme;
		else if (have_explicit_dark)
		{
			stripped_dark = fe_win32_strip_dark_suffix (explicit_dark_theme);
			if (stripped_dark && fe_win32_theme_exists (stripped_dark))
				light_theme = stripped_dark;
			else
			{
				g_free (stripped_dark);
				light_theme = g_strdup (current_theme);
			}
		}
		else
			light_theme = g_strdup (current_theme);

		if (have_explicit_light)
			explicit_light_theme = NULL;
		derived_dark = have_explicit_dark ? explicit_dark_theme : NULL;
		if (have_explicit_dark)
			explicit_dark_theme = NULL;

		g_free (fe_win32_light_gtkrc);
		fe_win32_light_gtkrc = have_explicit_light ? gtkrc_light : NULL;
		if (!have_explicit_light)
			g_free (gtkrc_light);
		gtkrc_light = NULL;

		g_free (fe_win32_dark_gtkrc);
		fe_win32_dark_gtkrc = have_explicit_dark ? gtkrc_dark : NULL;
		if (!have_explicit_dark)
			g_free (gtkrc_dark);
		gtkrc_dark = NULL;
	}
	else
	{
		g_free (fe_win32_light_gtkrc);
		fe_win32_light_gtkrc = NULL;
		g_free (fe_win32_dark_gtkrc);
		fe_win32_dark_gtkrc = NULL;
	}

	g_free (explicit_light_theme);
	g_free (explicit_dark_theme);
	g_free (gtkrc_light);
	g_free (gtkrc_dark);

	if (!light_theme)
	{
		light_theme = fe_win32_strip_dark_suffix (current_theme);
		if (!light_theme || !fe_win32_theme_exists (light_theme))
		{
			g_free (light_theme);
			if (fe_win32_theme_exists (current_theme))
				light_theme = g_strdup (current_theme);
		}
	}

	if (!light_theme)
		light_theme = g_strdup ("MS-Windows");

	if (!derived_dark)
	{
		for (i = 0; i < (int) G_N_ELEMENTS (dark_suffixes); i++)
		{
			suffix = g_strconcat (light_theme, dark_suffixes[i], NULL);
			if (fe_win32_theme_exists (suffix))
			{
				derived_dark = suffix;
				break;
			}
			g_free (suffix);
			suffix = NULL;
		}

		if (!derived_dark && fe_win32_theme_exists (current_theme))
		{
			char *stripped = fe_win32_strip_dark_suffix (current_theme);
			if (stripped && g_strcmp0 (stripped, light_theme) == 0)
				derived_dark = g_strdup (current_theme);
			g_free (stripped);
		}

		if (!derived_dark && fe_win32_theme_exists ("MS-Windows-Dark"))
			derived_dark = g_strdup ("MS-Windows-Dark");
	}

	g_free (fe_win32_light_theme);
	fe_win32_light_theme = light_theme;

	g_free (fe_win32_dark_theme);
	fe_win32_dark_theme = derived_dark;

	g_free (current_theme);
}

static void
fe_win32_apply_theme_for_dark_mode (GtkSettings *settings, gboolean enabled)
{
	const char *theme_name;
	char *current_theme = NULL;
	char *new_env = NULL;
	const char *target_gtkrc;
	GList *toplevels = NULL;
	GList *iter = NULL;
	gboolean env_changed = FALSE;
	gboolean theme_changed = FALSE;

	if (!settings)
		return;

	if (!fe_win32_original_gtkrc_env)
	{
		const char *env = g_getenv ("GTK2_RC_FILES");
		if (env && env[0])
			fe_win32_original_gtkrc_env = g_strdup (env);
	}

	fe_win32_update_theme_cache (settings);

	target_gtkrc = enabled ? fe_win32_dark_gtkrc : fe_win32_light_gtkrc;
	if (target_gtkrc)
	{
		if (fe_win32_original_gtkrc_env && fe_win32_original_gtkrc_env[0])
			new_env = g_strconcat (fe_win32_original_gtkrc_env, G_SEARCHPATH_SEPARATOR_S, target_gtkrc, NULL);
		else
			new_env = g_strdup (target_gtkrc);

		if (g_strcmp0 (g_getenv ("GTK2_RC_FILES"), new_env) != 0)
			env_changed = g_setenv ("GTK2_RC_FILES", new_env, TRUE);
	}
	else if (fe_win32_original_gtkrc_env && fe_win32_original_gtkrc_env[0])
	{
		if (g_strcmp0 (g_getenv ("GTK2_RC_FILES"), fe_win32_original_gtkrc_env) != 0)
			env_changed = g_setenv ("GTK2_RC_FILES", fe_win32_original_gtkrc_env, TRUE);
	}
	else if (g_getenv ("GTK2_RC_FILES"))
	{
		g_unsetenv ("GTK2_RC_FILES");
		env_changed = TRUE;
	}

	g_free (new_env);

	theme_name = enabled ? fe_win32_dark_theme : fe_win32_light_theme;
	if (!theme_name)
	{
		if (env_changed)
			goto reset_styles;
		return;
	}

	if (!fe_win32_theme_exists (theme_name))
	{
		if (env_changed)
			goto reset_styles;
		return;
	}

	g_object_get (settings, "gtk-theme-name", &current_theme, NULL);
	if (g_strcmp0 (current_theme, theme_name) != 0)
		theme_changed = TRUE;
	g_free (current_theme);

	if (!env_changed && !theme_changed)
		return;

	if (theme_changed)
		g_object_set (settings, "gtk-theme-name", theme_name, NULL);

reset_styles:
	gtk_rc_reparse_all_for_settings (settings, TRUE);
	gtk_rc_reset_styles (settings);

	toplevels = gtk_window_list_toplevels ();
	for (iter = toplevels; iter; iter = iter->next)
	{
		GtkWidget *widget = GTK_WIDGET (iter->data);
		gtk_widget_reset_rc_styles (widget);
		gtk_widget_queue_draw (widget);
	}
	g_list_free (toplevels);

	fe_win32_update_theme_cache (settings);
}

void
fe_win32_apply_theme_for_mode (unsigned int mode)
{
	GtkSettings *settings = gtk_settings_get_default ();

	if (!settings)
		return;

	fe_win32_apply_theme_for_dark_mode (settings, fe_dark_mode_is_enabled_for (mode));
}
#endif

static void
fe_auto_dark_mode_changed (GtkSettings *settings, GParamSpec *pspec, gpointer data)
{
	gboolean enabled;

	(void) settings;
	(void) pspec;
	(void) data;

#ifdef G_OS_WIN32
	if (settings && prefs.hex_gui_dark_mode != ZOITECHAT_DARK_MODE_AUTO)
		fe_win32_update_theme_cache (settings);
#endif

	if (prefs.hex_gui_dark_mode != ZOITECHAT_DARK_MODE_AUTO)
		return;

	enabled = fe_system_prefers_dark ();
	if (enabled == auto_dark_mode_enabled)
		return;

	auto_dark_mode_enabled = enabled;
#ifdef G_OS_WIN32
	fe_win32_apply_theme_for_dark_mode (gtk_settings_get_default (), enabled);
#endif
	palette_apply_dark_mode (enabled);
	setup_apply_real (0, TRUE, FALSE, FALSE);
}

void
fe_set_auto_dark_mode_state (gboolean enabled)
{
	auto_dark_mode_enabled = enabled;
}

void
fe_refresh_auto_dark_mode (void)
{
	fe_auto_dark_mode_changed (NULL, NULL, NULL);
}

gboolean
fe_dark_mode_is_enabled_for (unsigned int mode)
{
	switch (mode)
	{
	case ZOITECHAT_DARK_MODE_DARK:
		return TRUE;
	case ZOITECHAT_DARK_MODE_LIGHT:
		return FALSE;
	case ZOITECHAT_DARK_MODE_AUTO:
	default:
		return fe_system_prefers_dark ();
	}
}

gboolean
fe_dark_mode_is_enabled (void)
{
	return fe_dark_mode_is_enabled_for (prefs.hex_gui_dark_mode);
}

#ifndef G_OS_WIN32
void
fe_apply_gtk_dark_preference_for_mode (unsigned int mode)
{
	GtkSettings *settings;
	gboolean target_prefer_dark = FALSE;
	gboolean current_prefer_dark = FALSE;

	settings = gtk_settings_get_default ();
	if (!settings)
		return;

	fe_capture_gtk_dark_preference (settings);

	if (!g_object_class_find_property (G_OBJECT_GET_CLASS (settings),
	                                   "gtk-application-prefer-dark-theme"))
		return;

	switch (mode)
	{
	case ZOITECHAT_DARK_MODE_DARK:
		target_prefer_dark = TRUE;
		break;
	case ZOITECHAT_DARK_MODE_LIGHT:
		target_prefer_dark = FALSE;
		break;
	case ZOITECHAT_DARK_MODE_AUTO:
	default:
		target_prefer_dark = gtk_prefer_dark_initial_valid ? gtk_prefer_dark_initial : fe_system_prefers_dark ();
		break;
	}

	g_object_get (settings, "gtk-application-prefer-dark-theme", &current_prefer_dark, NULL);
	if (current_prefer_dark != target_prefer_dark)
		g_object_set (settings, "gtk-application-prefer-dark-theme", target_prefer_dark, NULL);
}
#else
void
fe_apply_gtk_dark_preference_for_mode (unsigned int mode)
{
	(void) mode;
}
#endif

GtkStyle *
create_input_style (GtkStyle *style)
{
	char buf[256];
	static int done_rc = FALSE;

	pango_font_description_free (style->font_desc);
	style->font_desc = pango_font_description_from_string (prefs.hex_text_font);

	/* fall back */
	if (pango_font_description_get_size (style->font_desc) == 0)
	{
		g_snprintf (buf, sizeof (buf), _("Failed to open font:\n\n%s"), prefs.hex_text_font);
		fe_message (buf, FE_MSG_ERROR);
		pango_font_description_free (style->font_desc);
		style->font_desc = pango_font_description_from_string ("sans 11");
	}

	if (prefs.hex_gui_input_style && !done_rc)
	{
		GtkSettings *settings = gtk_settings_get_default ();
		char *theme_name;

		/* gnome-themes-standard 3.20+ relies on images to do theming
		 * so we have to override that. */
		g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
		if (g_str_has_prefix (theme_name, "Adwaita") || g_str_has_prefix (theme_name, "Yaru"))
			gtk_rc_parse_string (adwaita_workaround_rc);
		g_free (theme_name);

		done_rc = TRUE;
		sprintf (buf, cursor_color_rc, (colors[COL_FG].red >> 8),
			(colors[COL_FG].green >> 8), (colors[COL_FG].blue >> 8));
		gtk_rc_parse_string (buf);
	}

	return style;
}

void
fe_init (void)
{
	GtkSettings *settings;

	settings = gtk_settings_get_default ();
	if (settings)
	{
		auto_dark_mode_enabled = fe_system_prefers_dark ();
#ifdef G_OS_WIN32
		fe_win32_apply_theme_for_dark_mode (settings, fe_dark_mode_is_enabled ());
#endif
		fe_apply_gtk_dark_preference_for_mode (prefs.hex_gui_dark_mode);
	}

	palette_load ();
	palette_apply_dark_mode (fe_dark_mode_is_enabled ());
	key_init ();
	pixmaps_init ();

#ifdef HAVE_GTK_MAC
	gtkosx_application_set_dock_icon_pixbuf (osx_app, pix_zoitechat);
#endif
	channelwin_pix = pixmap_load_from_file (prefs.hex_text_background);
	input_style = create_input_style (gtk_style_new ());

	if (settings)
	{
		g_signal_connect (settings, "notify::gtk-application-prefer-dark-theme",
						  G_CALLBACK (fe_auto_dark_mode_changed), NULL);
		g_signal_connect (settings, "notify::gtk-theme-name",
						  G_CALLBACK (fe_auto_dark_mode_changed), NULL);
	}
}

#ifdef HAVE_GTK_MAC
static void
gtkosx_application_terminate (GtkosxApplication *app, gpointer userdata)
{
	zoitechat_exit();
}
#endif

void
fe_main (void)
{
#ifdef HAVE_GTK_MAC
	gtkosx_application_ready(osx_app);
	g_signal_connect (G_OBJECT(osx_app), "NSApplicationWillTerminate",
					G_CALLBACK(gtkosx_application_terminate), NULL);
#endif

	gtk_main ();

	/* sleep for 2 seconds so any QUIT messages are not lost. The  */
	/* GUI is closed at this point, so the user doesn't even know! */
	if (prefs.wait_on_exit)
		sleep (2);
}

void
fe_cleanup (void)
{
	/* it's saved when pressing OK in setup.c */
	/*palette_save ();*/
}

void
fe_exit (void)
{
	gtk_main_quit ();
}

int
fe_timeout_add (int interval, void *callback, void *userdata)
{
	return g_timeout_add (interval, (GSourceFunc) callback, userdata);
}

int
fe_timeout_add_seconds (int interval, void *callback, void *userdata)
{
	return g_timeout_add_seconds (interval, (GSourceFunc) callback, userdata);
}

void
fe_timeout_remove (int tag)
{
	g_source_remove (tag);
}

#ifdef WIN32

static void
log_handler (const gchar   *log_domain,
		       GLogLevelFlags log_level,
		       const gchar   *message,
		       gpointer	      unused_data)
{
	session *sess;

	/* if (getenv ("HEXCHAT_WARNING_IGNORE")) this gets ignored sometimes, so simply just disable all warnings */
		return;

	sess = find_dialog (serv_list->data, "(warnings)");
	if (!sess)
		sess = new_ircwindow (serv_list->data, "(warnings)", SESS_DIALOG, 0);

	PrintTextf (sess, "%s\t%s\n", log_domain, message);
	if (getenv ("HEXCHAT_WARNING_ABORT"))
		abort ();
}

#endif

/* install tray stuff */

static int
fe_idle (gpointer data)
{
	session *sess = sess_list->data;

	plugin_add (sess, NULL, NULL, notification_plugin_init, notification_plugin_deinit, NULL, FALSE);

	plugin_add (sess, NULL, NULL, tray_plugin_init, tray_plugin_deinit, NULL, FALSE);

	if (arg_minimize == 1)
		gtk_window_iconify (GTK_WINDOW (sess->gui->window));
	else if (arg_minimize == 2)
		tray_toggle_visibility (FALSE);

	return 0;
}

void
fe_new_window (session *sess, int focus)
{
	int tab = FALSE;

	if (sess->type == SESS_DIALOG)
	{
		if (prefs.hex_gui_tab_dialogs)
			tab = TRUE;
	} else
	{
		if (prefs.hex_gui_tab_chans)
			tab = TRUE;
	}

	mg_changui_new (sess, NULL, tab, focus);

#ifdef WIN32
	g_log_set_handler ("GLib", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("GLib-GObject", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("Gdk", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("Gtk", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
#endif

	if (!sess_list->next)
		g_idle_add (fe_idle, NULL);

	sess->scrollback_replay_marklast = gtk_xtext_set_marker_last;
}

void
fe_new_server (struct server *serv)
{
	serv->gui = g_new0 (struct server_gui, 1);
}

void
fe_message (char *msg, int flags)
{
	GtkWidget *dialog;
	int type = GTK_MESSAGE_WARNING;

	if (flags & FE_MSG_ERROR)
		type = GTK_MESSAGE_ERROR;
	if (flags & FE_MSG_INFO)
		type = GTK_MESSAGE_INFO;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent_window), 0, type,
												GTK_BUTTONS_OK, "%s", msg);
	if (flags & FE_MSG_MARKUP)
		gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), msg);
	g_signal_connect (G_OBJECT (dialog), "response",
							G_CALLBACK (gtk_widget_destroy), 0);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_widget_show (dialog);

	if (flags & FE_MSG_WAIT)
		gtk_dialog_run (GTK_DIALOG (dialog));
}

void
fe_idle_add (void *func, void *data)
{
	g_idle_add (func, data);
}

void
fe_input_remove (int tag)
{
	g_source_remove (tag);
}

int
fe_input_add (int sok, int flags, void *func, void *data)
{
	int tag, type = 0;
	GIOChannel *channel;

#ifdef WIN32
	if (flags & FIA_FD)
		channel = g_io_channel_win32_new_fd (sok);
	else
		channel = g_io_channel_win32_new_socket (sok);
#else
	channel = g_io_channel_unix_new (sok);
#endif

	if (flags & FIA_READ)
		type |= G_IO_IN | G_IO_HUP | G_IO_ERR;
	if (flags & FIA_WRITE)
		type |= G_IO_OUT | G_IO_ERR;
	if (flags & FIA_EX)
		type |= G_IO_PRI;

	tag = g_io_add_watch (channel, type, (GIOFunc) func, data);
	g_io_channel_unref (channel);

	return tag;
}

void
fe_set_topic (session *sess, char *topic, char *stripped_topic)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		if (prefs.hex_text_stripcolor_topic)
		{
			gtk_entry_set_text (GTK_ENTRY (sess->gui->topic_entry), stripped_topic);
		}
		else
		{
			gtk_entry_set_text (GTK_ENTRY (sess->gui->topic_entry), topic);
		}
		mg_set_topic_tip (sess);
	}
	else
	{
		g_free (sess->res->topic_text);

		if (prefs.hex_text_stripcolor_topic)
		{
			sess->res->topic_text = g_strdup (stripped_topic);
		}
		else
		{
			sess->res->topic_text = g_strdup (topic);
		}
	}
}

static void
fe_update_mode_entry (session *sess, GtkWidget *entry, char **text, char *new_text)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		if (sess->gui->flag_wid[0])	/* channel mode buttons enabled? */
			gtk_entry_set_text (GTK_ENTRY (entry), new_text);
	} else
	{
		if (sess->gui->is_tab)
		{
			g_free (*text);
			*text = g_strdup (new_text);
		}
	}
}

void
fe_update_channel_key (struct session *sess)
{
	fe_update_mode_entry (sess, sess->gui->key_entry,
								 &sess->res->key_text, sess->channelkey);
	fe_set_title (sess);
}

void
fe_update_channel_limit (struct session *sess)
{
	char tmp[16];

	sprintf (tmp, "%d", sess->limit);
	fe_update_mode_entry (sess, sess->gui->limit_entry,
								 &sess->res->limit_text, tmp);
	fe_set_title (sess);
}

int
fe_is_chanwindow (struct server *serv)
{
	if (!serv->gui->chanlist_window)
		return 0;
	return 1;
}

void
fe_notify_update (char *name)
{
	if (!name)
		notify_gui_update ();
}

void
fe_text_clear (struct session *sess, int lines)
{
	gtk_xtext_clear (sess->res->buffer, lines);
}

void
fe_close_window (struct session *sess)
{
	if (sess->gui->is_tab)
		mg_tab_close (sess);
	else
		gtk_widget_destroy (sess->gui->window);
}

void
fe_progressbar_start (session *sess)
{
	if (!sess->gui->is_tab || current_tab == sess)
	/* if it's the focused tab, create it for real! */
		mg_progressbar_create (sess->gui);
	else
	/* otherwise just remember to create on when it gets focused */
		sess->res->c_graph = TRUE;
}

void
fe_progressbar_end (server *serv)
{
	GSList *list = sess_list;
	session *sess;

	while (list)				  /* check all windows that use this server and  *
									   * remove the connecting graph, if it has one. */
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (sess->gui->bar)
				mg_progressbar_destroy (sess->gui);
			sess->res->c_graph = FALSE;
		}
		list = list->next;
	}
}

void
fe_print_text (struct session *sess, char *text, time_t stamp,
			   gboolean no_activity)
{
	PrintTextRaw (sess->res->buffer, (unsigned char *)text, prefs.hex_text_indent, stamp);

	if (no_activity || !sess->gui->is_tab)
		return;

	if (sess == current_tab)
		fe_set_tab_color (sess, FE_COLOR_NONE);
	else if (sess->tab_state & TAB_STATE_NEW_HILIGHT)
		fe_set_tab_color (sess, FE_COLOR_NEW_HILIGHT);
	else if (sess->tab_state & TAB_STATE_NEW_MSG)
		fe_set_tab_color (sess, FE_COLOR_NEW_MSG);
	else
		fe_set_tab_color (sess, FE_COLOR_NEW_DATA);
}

void
fe_beep (session *sess)
{
#ifdef WIN32
	/* Play the "Instant Message Notification" system sound
	 */
	if (!PlaySoundW (L"Notification.IM", NULL, SND_ALIAS | SND_ASYNC))
	{
		/* The user does not have the "Instant Message Notification" sound set. Fall back to system beep.
		 */
		Beep (1000, 50);
	}
#else
#ifdef USE_LIBCANBERRA
	if (ca_con == NULL)
	{
		ca_context_create (&ca_con);
		ca_context_change_props (ca_con,
										CA_PROP_APPLICATION_ID, "zoitechat",
										CA_PROP_APPLICATION_NAME, DISPLAY_NAME,
										CA_PROP_APPLICATION_ICON_NAME, "zoitechat", NULL);
	}

	if (ca_context_play (ca_con, 0, CA_PROP_EVENT_ID, "message-new-instant", NULL) != 0)
#endif
	gdk_beep ();
#endif
}

void
fe_lastlog (session *sess, session *lastlog_sess, char *sstr, gtk_xtext_search_flags flags)
{
	GError *err = NULL;
	xtext_buffer *buf, *lbuf;

	buf = sess->res->buffer;

	if (gtk_xtext_is_empty (buf))
	{
		PrintText (lastlog_sess, _("Search buffer is empty.\n"));
		return;
	}

	lbuf = lastlog_sess->res->buffer;
	if (flags & regexp)
	{
		GRegexCompileFlags gcf = (flags & case_match)? 0: G_REGEX_CASELESS;

		lbuf->search_re = g_regex_new (sstr, gcf, 0, &err);
		if (err)
		{
			PrintText (lastlog_sess, _(err->message));
			g_error_free (err);
			return;
		}
	}
	else
	{
		if (flags & case_match)
		{
			lbuf->search_nee = g_strdup (sstr);
		}
		else
		{
			lbuf->search_nee = g_utf8_casefold (sstr, strlen (sstr));
		}
		lbuf->search_lnee = strlen (lbuf->search_nee);
	}
	lbuf->search_flags = flags;
	lbuf->search_text = g_strdup (sstr);
	gtk_xtext_lastlog (lbuf, buf);
}

void
fe_set_lag (server *serv, long lag)
{
	GSList *list = sess_list;
	session *sess;
	gdouble per;
	char lagtext[64];
	char lagtip[128];
	unsigned long nowtim;

	if (lag == -1)
	{
		if (!serv->lag_sent)
			return;
		nowtim = make_ping_time ();
		lag = nowtim - serv->lag_sent;
	}

	/* if there is no pong for >30s report the lag as +30s */
	if (lag > 30000 && serv->lag_sent)
		lag=30000;

	per = ((double)lag) / 1000.0;
	if (per > 1.0)
		per = 1.0;

	g_snprintf (lagtext, sizeof (lagtext) - 1, "%s%ld.%lds",
			  serv->lag_sent ? "+" : "", lag / 1000, (lag/100) % 10);
	g_snprintf (lagtip, sizeof (lagtip) - 1, "Lag: %s%ld.%ld seconds",
				 serv->lag_sent ? "+" : "", lag / 1000, (lag/100) % 10);

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			g_free (sess->res->lag_tip);
			sess->res->lag_tip = g_strdup (lagtip);

			if (!sess->gui->is_tab || current_tab == sess)
			{
				if (sess->gui->lagometer)
				{
					gtk_progress_bar_set_fraction ((GtkProgressBar *) sess->gui->lagometer, per);
					gtk_widget_set_tooltip_text (gtk_widget_get_parent (sess->gui->lagometer), lagtip);
				}
				if (sess->gui->laginfo)
					gtk_label_set_text ((GtkLabel *) sess->gui->laginfo, lagtext);
			} else
			{
				sess->res->lag_value = per;
				g_free (sess->res->lag_text);
				sess->res->lag_text = g_strdup (lagtext);
			}
		}
		list = list->next;
	}
}

void
fe_set_throttle (server *serv)
{
	GSList *list = sess_list;
	struct session *sess;
	float per;
	char tbuf[96];
	char tip[160];

	per = (float) serv->sendq_len / 1024.0;
	if (per > 1.0)
		per = 1.0;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			g_snprintf (tbuf, sizeof (tbuf) - 1, _("%d bytes"), serv->sendq_len);
			g_snprintf (tip, sizeof (tip) - 1, _("Network send queue: %d bytes"), serv->sendq_len);

			g_free (sess->res->queue_tip);
			sess->res->queue_tip = g_strdup (tip);

			if (!sess->gui->is_tab || current_tab == sess)
			{
				if (sess->gui->throttlemeter)
				{
					gtk_progress_bar_set_fraction ((GtkProgressBar *) sess->gui->throttlemeter, per);
					gtk_widget_set_tooltip_text (gtk_widget_get_parent (sess->gui->throttlemeter), tip);
				}
				if (sess->gui->throttleinfo)
					gtk_label_set_text ((GtkLabel *) sess->gui->throttleinfo, tbuf);
			} else
			{
				sess->res->queue_value = per;
				g_free (sess->res->queue_text);
				sess->res->queue_text = g_strdup (tbuf);
			}
		}
		list = list->next;
	}
}

void
fe_ctrl_gui (session *sess, fe_gui_action action, int arg)
{
	switch (action)
	{
	case FE_GUI_HIDE:
		gtk_widget_hide (sess->gui->window); break;
	case FE_GUI_SHOW:
		gtk_widget_show (sess->gui->window);
		gtk_window_present (GTK_WINDOW (sess->gui->window));
		break;
	case FE_GUI_FOCUS:
		mg_bring_tofront_sess (sess); break;
	case FE_GUI_FLASH:
		fe_flash_window (sess); break;
	case FE_GUI_COLOR:
		fe_set_tab_color (sess, arg); break;
	case FE_GUI_ICONIFY:
		gtk_window_iconify (GTK_WINDOW (sess->gui->window)); break;
	case FE_GUI_MENU:
		menu_bar_toggle ();	/* toggle menubar on/off */
		break;
	case FE_GUI_ATTACH:
		mg_detach (sess, arg);	/* arg: 0=toggle 1=detach 2=attach */
		break;
	case FE_GUI_APPLY:
		setup_apply_real (TRUE, TRUE, TRUE, FALSE);
	}
}

static void
dcc_saveas_cb (struct DCC *dcc, char *file)
{
	if (is_dcc (dcc))
	{
		if (dcc->dccstat == STAT_QUEUED)
		{
			if (file)
				dcc_get_with_destfile (dcc, file);
			else if (dcc->resume_sent == 0)
				dcc_abort (dcc->serv->front_session, dcc);
		}
	}
}

void
fe_confirm (const char *message, void (*yesproc)(void *), void (*noproc)(void *), void *ud)
{
	/* warning, assuming fe_confirm is used by DCC only! */
	struct DCC *dcc = ud;

	if (dcc->file)
	{
		char *filepath = g_build_filename (prefs.hex_dcc_dir, dcc->file, NULL);
		gtkutil_file_req (NULL, message, dcc_saveas_cb, ud, filepath, NULL,
								FRF_WRITE|FRF_NOASKOVERWRITE|FRF_FILTERISINITIAL);
		g_free (filepath);
	}
}

int
fe_gui_info (session *sess, int info_type)
{
	switch (info_type)
	{
	case 0:	/* window status */
		if (!gtk_widget_get_visible (GTK_WIDGET (sess->gui->window)))
		{
			return 2;	/* hidden (iconified or systray) */
		}

		if (gtk_window_is_active (GTK_WINDOW (sess->gui->window)))
		{
			return 1;	/* active/focused */
		}

		return 0;		/* normal (no keyboard focus or behind a window) */
	}

	return -1;
}

void *
fe_gui_info_ptr (session *sess, int info_type)
{
	switch (info_type)
	{
	case 0:	/* native window pointer (for plugins) */
#ifdef WIN32
		return gdk_win32_window_get_impl_hwnd (gtk_widget_get_window (sess->gui->window));
#else
		return sess->gui->window;
#endif
		break;

	case 1:	/* GtkWindow * (for plugins) */
		return sess->gui->window;
	}
	return NULL;
}

char *
fe_get_inputbox_contents (session *sess)
{
	/* not the current tab */
	if (sess->res->input_text)
		return sess->res->input_text;

	/* current focused tab */
	return SPELL_ENTRY_GET_TEXT (sess->gui->input_box);
}

int
fe_get_inputbox_cursor (session *sess)
{
	/* not the current tab (we don't remember the cursor pos) */
	if (sess->res->input_text)
		return 0;

	/* current focused tab */
	return SPELL_ENTRY_GET_POS (sess->gui->input_box);
}

void
fe_set_inputbox_cursor (session *sess, int delta, int pos)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		if (delta)
			pos += SPELL_ENTRY_GET_POS (sess->gui->input_box);
		SPELL_ENTRY_SET_POS (sess->gui->input_box, pos);
	} else
	{
		/* we don't support changing non-front tabs yet */
	}
}

void
fe_set_inputbox_contents (session *sess, char *text)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		SPELL_ENTRY_SET_TEXT (sess->gui->input_box, text);
	} else
	{
		g_free (sess->res->input_text);
		sess->res->input_text = g_strdup (text);
	}
}

#ifdef __APPLE__
static char *
url_escape_hostname (const char *url)
{
    char *host_start, *host_end, *ret, *hostname;

    host_start = strstr (url, "://");
    if (host_start != NULL)
    {
        *host_start = '\0';
        host_start += 3;
        host_end = strchr (host_start, '/');

        if (host_end != NULL)
        {
            *host_end = '\0';
            host_end++;
        }

        hostname = g_hostname_to_ascii (host_start);
        if (host_end != NULL)
            ret = g_strdup_printf ("%s://%s/%s", url, hostname, host_end);
        else
            ret = g_strdup_printf ("%s://%s", url, hostname);

        g_free (hostname);
        return ret;
    }

    return g_strdup (url);
}

static void
osx_show_uri (const char *url)
{
    char *escaped_url, *encoded_url, *open, *cmd;

    escaped_url = url_escape_hostname (url);
    encoded_url = g_filename_from_utf8 (escaped_url, -1, NULL, NULL, NULL);
    if (encoded_url)
    {
        open = g_find_program_in_path ("open");
        cmd = g_strjoin (" ", open, encoded_url, NULL);

        zoitechat_exec (cmd);

        g_free (encoded_url);
        g_free (cmd);
    }

    g_free (escaped_url);
}

#endif

static inline char *
escape_uri (const char *uri)
{
	return g_uri_escape_string(uri, G_URI_RESERVED_CHARS_GENERIC_DELIMITERS G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, FALSE);
}

static inline gboolean
uri_contains_forbidden_characters (const char *uri)
{
	while (*uri)
	{
		if (!g_ascii_isalnum (*uri) && !strchr ("-._~:/?#[]@!$&'()*+,;=", *uri))
			return TRUE;
		uri++;
	}

	return FALSE;
}

static char *
maybe_escape_uri (const char *uri)
{
	/* The only way to know if a string has already been escaped or not
	 * is by fulling parsing each segement but we can try some more simple heuristics. */

	/* If we find characters that should clearly be escaped. */
	if (uri_contains_forbidden_characters (uri))
		return escape_uri (uri);

	/* If it fails to be unescaped then it was not escaped. */
	char *unescaped = g_uri_unescape_string (uri, NULL);
	if (!unescaped)
		return escape_uri (uri);
	g_free (unescaped);

	/* At this point it is probably safe to pass through as-is. */
	return g_strdup (uri);
}

static void
fe_open_url_inner (const char *url)
{
#ifdef WIN32
	gunichar2 *url_utf16 = g_utf8_to_utf16 (url, -1, NULL, NULL, NULL);

	if (url_utf16 == NULL)
	{
		return;
	}

	ShellExecuteW (0, L"open", url_utf16, NULL, NULL, SW_SHOWNORMAL);

	g_free (url_utf16);
#elif defined(__APPLE__)
    osx_show_uri (url);
#else
	char *escaped_url = maybe_escape_uri (url);
	g_debug ("Opening URL \"%s\" (%s)", escaped_url, url);
	gtk_show_uri (NULL, escaped_url, GDK_CURRENT_TIME, NULL);
	g_free (escaped_url);
#endif
}

void
fe_open_url (const char *url)
{
	int url_type = url_check_word (url);
	char *uri;

	/* gvfs likes file:// */
	if (url_type == WORD_PATH)
	{
#ifndef WIN32
		uri = g_strconcat ("file://", url, NULL);
		fe_open_url_inner (uri);
		g_free (uri);
#else
		fe_open_url_inner (url);
#endif
	}
	/* IPv6 addr. Add http:// */
	else if (url_type == WORD_HOST6)
	{
		/* IPv6 addrs in urls should be enclosed in [ ] */
		if (*url != '[')
			uri = g_strdup_printf ("http://[%s]", url);
		else
			uri = g_strdup_printf ("http://%s", url);

		fe_open_url_inner (uri);
		g_free (uri);
	}
	/* the http:// part's missing, prepend it, otherwise it won't always work */
	else if (strchr (url, ':') == NULL)
	{
		uri = g_strdup_printf ("http://%s", url);
		fe_open_url_inner (uri);
		g_free (uri);
	}
	/* we have a sane URL, send it to the browser untouched */
	else
	{
		fe_open_url_inner (url);
	}
}

void
fe_server_event (server *serv, int type, int arg)
{
	GSList *list = sess_list;
	session *sess;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv && (current_tab == sess || !sess->gui->is_tab))
		{
			session_gui *gui = sess->gui;

			switch (type)
			{
			case FE_SE_CONNECTING:	/* connecting in progress */
			case FE_SE_RECONDELAY:	/* reconnect delay begun */
				/* enable Disconnect item */
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_DISCONNECT], 1);
				break;

			case FE_SE_CONNECT:
				/* enable Disconnect and Away menu items */
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_AWAY], 1);
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_DISCONNECT], 1);
				break;

			case FE_SE_LOGGEDIN:	/* end of MOTD */
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_JOIN], 1);
				/* if number of auto-join channels is zero, open joind */
				if (arg == 0)
					joind_open (serv);
				break;

			case FE_SE_DISCONNECT:
				/* disable Disconnect and Away menu items */
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_AWAY], 0);
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_DISCONNECT], 0);
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_JOIN], 0);
				/* close the join-dialog, if one exists */
				joind_close (serv);
			}
		}
		list = list->next;
	}
}

void
fe_get_file (const char *title, char *initial,
				 void (*callback) (void *userdata, char *file), void *userdata,
				 int flags)
				
{
	/* OK: Call callback once per file, then once more with file=NULL. */
	/* CANCEL: Call callback once with file=NULL. */
	gtkutil_file_req (NULL, title, callback, userdata, initial, NULL, flags | FRF_FILTERISINITIAL);
}

void
fe_open_chan_list (server *serv, char *filter, int do_refresh)
{
	chanlist_opengui (serv, do_refresh);
}

const char *
fe_get_default_font (void)
{
#ifdef WIN32
	if (gtkutil_find_font ("Consolas"))
		return "Consolas 10";
	else
#else
#ifdef __APPLE__
	if (gtkutil_find_font ("Menlo"))
		return "Menlo 13";
	else
#endif
#endif
		return NULL;
}
