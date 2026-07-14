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

#include "theme-policy.h"

#include <string.h>

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "theme-gtk3.h"
#include "../fe-gtk.h"
#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"

/* org.freedesktop.appearance color-scheme values (XDG desktop portal) */
enum
{
	THEME_POLICY_COLOR_SCHEME_NO_PREFERENCE = 0,
	THEME_POLICY_COLOR_SCHEME_PREFER_DARK = 1,
	THEME_POLICY_COLOR_SCHEME_PREFER_LIGHT = 2
};

static ThemePolicyAppearanceChangedFunc policy_appearance_changed_cb;

/* GtkSettings state captured before ZoiteChat writes to it, so that values
 * coming from the user's own GTK configuration are not mistaken for values
 * this application set. */
static gboolean policy_initial_gtk_state_captured;
static gboolean policy_initial_prefer_dark;
static char *policy_initial_theme_name;

#ifndef G_OS_WIN32
static gboolean policy_portal_watch_started;
static gboolean policy_portal_value_valid;
static guint32 policy_portal_color_scheme;

static gboolean policy_gsettings_checked;
static GSettings *policy_gsettings_interface;
#endif

static void
policy_capture_initial_gtk_state (void)
{
	GtkSettings *settings;

	if (policy_initial_gtk_state_captured)
		return;

	settings = gtk_settings_get_default ();
	if (!settings)
		return;

	g_object_get (settings,
	              "gtk-application-prefer-dark-theme", &policy_initial_prefer_dark,
	              "gtk-theme-name", &policy_initial_theme_name,
	              NULL);
	policy_initial_gtk_state_captured = TRUE;
}

static gboolean
policy_theme_name_is_dark (const char *name)
{
	char *lower;
	gboolean dark;

	if (!name || !name[0])
		return FALSE;

	lower = g_ascii_strdown (name, -1);
	dark = strstr (lower, "dark") != NULL || strstr (lower, "noir") != NULL ||
	       strstr (lower, "black") != NULL;
	g_free (lower);
	return dark;
}

#ifndef G_OS_WIN32

static void
policy_notify_appearance_changed (void)
{
	if (policy_appearance_changed_cb)
		policy_appearance_changed_cb ();
}

static gboolean
policy_color_scheme_from_variant (GVariant *value, guint32 *color_scheme)
{
	GVariant *inner;
	gboolean ok = FALSE;

	if (!value || !color_scheme)
		return FALSE;

	inner = g_variant_ref (value);
	while (g_variant_is_of_type (inner, G_VARIANT_TYPE_VARIANT))
	{
		GVariant *unwrapped = g_variant_get_variant (inner);

		g_variant_unref (inner);
		inner = unwrapped;
	}

	if (g_variant_is_of_type (inner, G_VARIANT_TYPE_UINT32))
	{
		*color_scheme = g_variant_get_uint32 (inner);
		ok = TRUE;
	}

	g_variant_unref (inner);
	return ok;
}

static void
policy_portal_store_color_scheme (guint32 color_scheme)
{
	gboolean changed;

	changed = !policy_portal_value_valid || policy_portal_color_scheme != color_scheme;
	policy_portal_value_valid = TRUE;
	policy_portal_color_scheme = color_scheme;

	if (changed)
		policy_notify_appearance_changed ();
}

static void
policy_portal_setting_changed_cb (GDBusConnection *connection, const char *sender_name,
                                  const char *object_path, const char *interface_name,
                                  const char *signal_name, GVariant *parameters,
                                  gpointer user_data)
{
	const char *setting_namespace = NULL;
	const char *setting_key = NULL;
	GVariant *value = NULL;
	guint32 color_scheme = THEME_POLICY_COLOR_SCHEME_NO_PREFERENCE;

	(void) connection;
	(void) sender_name;
	(void) object_path;
	(void) interface_name;
	(void) signal_name;
	(void) user_data;

	if (!parameters || !g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(ssv)")))
		return;

	g_variant_get (parameters, "(&s&sv)", &setting_namespace, &setting_key, &value);
	if (g_strcmp0 (setting_namespace, "org.freedesktop.appearance") == 0 &&
	    g_strcmp0 (setting_key, "color-scheme") == 0 &&
	    policy_color_scheme_from_variant (value, &color_scheme))
		policy_portal_store_color_scheme (color_scheme);

	if (value)
		g_variant_unref (value);
}

static void
policy_portal_read_ready_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GVariant *reply;
	GVariant *value = NULL;
	guint32 color_scheme = THEME_POLICY_COLOR_SCHEME_NO_PREFERENCE;

	(void) user_data;

	reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, NULL);
	if (!reply)
		return;

	if (g_variant_is_of_type (reply, G_VARIANT_TYPE ("(v)")))
	{
		g_variant_get (reply, "(v)", &value);
		if (policy_color_scheme_from_variant (value, &color_scheme))
			policy_portal_store_color_scheme (color_scheme);
		if (value)
			g_variant_unref (value);
	}

	g_variant_unref (reply);
}

static void
policy_portal_bus_ready_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GDBusConnection *connection;

	(void) source;
	(void) user_data;

	connection = g_bus_get_finish (result, NULL);
	if (!connection)
		return;

	g_dbus_connection_signal_subscribe (connection,
	                                    "org.freedesktop.portal.Desktop",
	                                    "org.freedesktop.portal.Settings",
	                                    "SettingChanged",
	                                    "/org/freedesktop/portal/desktop",
	                                    NULL,
	                                    G_DBUS_SIGNAL_FLAGS_NONE,
	                                    policy_portal_setting_changed_cb,
	                                    NULL,
	                                    NULL);

	g_dbus_connection_call (connection,
	                        "org.freedesktop.portal.Desktop",
	                        "/org/freedesktop/portal/desktop",
	                        "org.freedesktop.portal.Settings",
	                        "Read",
	                        g_variant_new ("(ss)", "org.freedesktop.appearance", "color-scheme"),
	                        G_VARIANT_TYPE ("(v)"),
	                        G_DBUS_CALL_FLAGS_NONE,
	                        3000,
	                        NULL,
	                        policy_portal_read_ready_cb,
	                        NULL);
	g_object_unref (connection);
}

static void
policy_portal_watch_start (void)
{
	if (policy_portal_watch_started)
		return;

	policy_portal_watch_started = TRUE;
	g_bus_get (G_BUS_TYPE_SESSION, NULL, policy_portal_bus_ready_cb, NULL);
}

static void
policy_gsettings_changed_cb (GSettings *settings, const char *key, gpointer user_data)
{
	(void) settings;
	(void) key;
	(void) user_data;

	policy_notify_appearance_changed ();
}

static GSettings *
policy_get_interface_gsettings (void)
{
	GSettingsSchemaSource *source;
	GSettingsSchema *schema;
	gboolean has_key;

	if (policy_gsettings_checked)
		return policy_gsettings_interface;

	policy_gsettings_checked = TRUE;

	source = g_settings_schema_source_get_default ();
	if (!source)
		return NULL;

	schema = g_settings_schema_source_lookup (source, "org.gnome.desktop.interface", TRUE);
	if (!schema)
		return NULL;

	/* Requires GLib 2.40, always satisfied by the GTK 3.22 dependency. */
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	has_key = g_settings_schema_has_key (schema, "color-scheme");
	G_GNUC_END_IGNORE_DEPRECATIONS
	g_settings_schema_unref (schema);
	if (!has_key)
		return NULL;

	policy_gsettings_interface = g_settings_new ("org.gnome.desktop.interface");
	g_signal_connect (policy_gsettings_interface, "changed::color-scheme",
	                  G_CALLBACK (policy_gsettings_changed_cb), NULL);
	return policy_gsettings_interface;
}

static gboolean
policy_gsettings_color_scheme (gboolean *prefers_dark)
{
	GSettings *settings;
	char *scheme;
	gboolean explicit_preference = FALSE;

	settings = policy_get_interface_gsettings ();
	if (!settings)
		return FALSE;

	scheme = g_settings_get_string (settings, "color-scheme");
	if (g_strcmp0 (scheme, "prefer-dark") == 0)
	{
		*prefers_dark = TRUE;
		explicit_preference = TRUE;
	}
	else if (g_strcmp0 (scheme, "prefer-light") == 0)
	{
		*prefers_dark = FALSE;
		explicit_preference = TRUE;
	}
	g_free (scheme);
	return explicit_preference;
}

static gboolean
policy_gtk_heuristic_prefers_dark (void)
{
	GtkSettings *settings;
	char *theme_name = NULL;
	gboolean dark;

	policy_capture_initial_gtk_state ();

	/* gtk-application-prefer-dark-theme is overwritten by ZoiteChat at
	 * runtime, so only its captured startup value (settings.ini, XSETTINGS)
	 * reflects the user's own configuration. */
	if (policy_initial_gtk_state_captured && policy_initial_prefer_dark)
		return TRUE;

	if (theme_gtk3_is_active ())
	{
		/* gtk-theme-name currently holds the in-app theme override;
		 * fall back to the captured system theme name. */
		theme_name = g_strdup (policy_initial_theme_name);
	}
	else
	{
		settings = gtk_settings_get_default ();
		if (settings)
			g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
	}

	dark = policy_theme_name_is_dark (theme_name);
	g_free (theme_name);
	return dark;
}

#endif /* !G_OS_WIN32 */

void
theme_policy_init (void)
{
	policy_capture_initial_gtk_state ();
#ifndef G_OS_WIN32
	policy_portal_watch_start ();
#endif
}

void
theme_policy_set_appearance_changed_callback (ThemePolicyAppearanceChangedFunc callback)
{
	policy_appearance_changed_cb = callback;
}

gboolean
theme_policy_system_prefers_dark (void)
{
#ifdef G_OS_WIN32
	gboolean prefer_dark = FALSE;

	policy_capture_initial_gtk_state ();

	if (fe_win32_high_contrast_is_enabled ())
		return FALSE;
	if (fe_win32_try_get_system_dark (&prefer_dark))
		return prefer_dark;
	return policy_theme_name_is_dark (policy_initial_theme_name);
#else
	gboolean prefers_dark = FALSE;

	policy_capture_initial_gtk_state ();
	policy_portal_watch_start ();

	if (policy_portal_value_valid &&
	    policy_portal_color_scheme != THEME_POLICY_COLOR_SCHEME_NO_PREFERENCE)
		return policy_portal_color_scheme == THEME_POLICY_COLOR_SCHEME_PREFER_DARK;

	if (policy_gsettings_color_scheme (&prefers_dark))
		return prefers_dark;

	return policy_gtk_heuristic_prefers_dark ();
#endif
}

gboolean
theme_policy_is_dark_mode_active (unsigned int mode)
{
	if (mode == ZOITECHAT_DARK_MODE_AUTO)
		return theme_policy_system_prefers_dark ();

	return fe_dark_mode_is_enabled_for (mode);
}

gboolean
theme_policy_is_app_dark_mode_active (void)
{
	return theme_policy_is_dark_mode_active (prefs.hex_gui_dark_mode);
}
