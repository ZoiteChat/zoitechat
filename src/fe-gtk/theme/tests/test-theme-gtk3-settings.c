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

#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "../theme-gtk3.h"
#include "../../../common/gtk3-theme-service.h"

struct session *current_sess;
struct session *current_tab;
struct session *lastact_sess;
struct zoitechatprefs prefs;

static gboolean gtk_available;
static char *temp_root;
static char *theme_parent_root;
static char *theme_child_root;
static char *theme_switch_root;

gboolean
theme_policy_system_prefers_dark (void)
{
	return FALSE;
}

static void
remove_tree (const char *path)
{
	GDir *dir;
	const char *name;

	if (!path || !g_file_test (path, G_FILE_TEST_EXISTS))
		return;
	if (!g_file_test (path, G_FILE_TEST_IS_DIR))
	{
		g_remove (path);
		return;
	}

	dir = g_dir_open (path, 0, NULL);
	if (dir)
	{
		while ((name = g_dir_read_name (dir)) != NULL)
		{
			char *child = g_build_filename (path, name, NULL);
			remove_tree (child);
			g_free (child);
		}
		g_dir_close (dir);
	}
	g_rmdir (path);
}

static void
write_file (const char *path, const char *contents)
{
	gboolean ok = g_file_set_contents (path, contents, -1, NULL);
	g_assert_true (ok);
}

static void
ensure_css_dir (const char *theme_root, const char *css_dir)
{
	char *dir = g_build_filename (theme_root, css_dir, NULL);
	char *css = g_build_filename (dir, "gtk.css", NULL);
	int rc = g_mkdir_with_parents (dir, 0700);
	g_assert_cmpint (rc, ==, 0);
	write_file (css, "* { }\n");
	g_free (css);
	g_free (dir);
}

static void
write_settings (const char *theme_root, const char *css_dir, const char *settings)
{
	char *path = g_build_filename (theme_root, css_dir, "settings.ini", NULL);
	write_file (path, settings);
	g_free (path);
}

static ZoitechatGtk3Theme *
make_theme (const char *id, const char *path)
{
	ZoitechatGtk3Theme *theme = g_new0 (ZoitechatGtk3Theme, 1);
	theme->id = g_strdup (id);
	theme->display_name = g_strdup (id);
	theme->path = g_strdup (path);
	theme->source = ZOITECHAT_GTK3_THEME_SOURCE_USER;
	return theme;
}

void
zoitechat_gtk3_theme_free (ZoitechatGtk3Theme *theme)
{
	if (!theme)
		return;
	g_free (theme->id);
	g_free (theme->display_name);
	g_free (theme->path);
	g_free (theme->thumbnail_path);
	g_free (theme);
}

ZoitechatGtk3Theme *
zoitechat_gtk3_theme_find_by_id (const char *theme_id)
{
	if (g_strcmp0 (theme_id, "layered") == 0)
		return make_theme (theme_id, theme_child_root);
	if (g_strcmp0 (theme_id, "switch") == 0)
		return make_theme (theme_id, theme_switch_root);
	return NULL;
}

char *
zoitechat_gtk3_theme_pick_css_dir_for_minor (const char *theme_root, int preferred_minor)
{
	char *path;
	(void) preferred_minor;
	path = g_build_filename (theme_root, "gtk-3.24", "gtk.css", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
	{
		g_free (path);
		return g_strdup ("gtk-3.24");
	}
	g_free (path);
	path = g_build_filename (theme_root, "gtk-3.0", "gtk.css", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
	{
		g_free (path);
		return g_strdup ("gtk-3.0");
	}
	g_free (path);
	return NULL;
}

char *
zoitechat_gtk3_theme_pick_css_dir (const char *theme_root)
{
	return zoitechat_gtk3_theme_pick_css_dir_for_minor (theme_root, -1);
}

GPtrArray *
zoitechat_gtk3_theme_build_inheritance_chain (const char *theme_root)
{
	GPtrArray *chain = g_ptr_array_new_with_free_func (g_free);
	if (g_strcmp0 (theme_root, theme_child_root) == 0)
	{
		g_ptr_array_add (chain, g_strdup (theme_parent_root));
		g_ptr_array_add (chain, g_strdup (theme_child_root));
		return chain;
	}
	if (g_strcmp0 (theme_root, theme_switch_root) == 0)
	{
		g_ptr_array_add (chain, g_strdup (theme_switch_root));
		return chain;
	}
	g_ptr_array_unref (chain);
	return NULL;
}

static gboolean
get_bool_setting (const char *name)
{
	GtkSettings *settings = gtk_settings_get_default ();
	gboolean value = FALSE;
	g_object_get (settings, name, &value, NULL);
	return value;
}

static gint
get_int_setting (const char *name)
{
	GtkSettings *settings = gtk_settings_get_default ();
	gint value = 0;
	g_object_get (settings, name, &value, NULL);
	return value;
}

static void
setup_themes (void)
{
	char *path;

	temp_root = g_dir_make_tmp ("zoitechat-theme-gtk3-settings-XXXXXX", NULL);
	g_assert_nonnull (temp_root);
	theme_parent_root = g_build_filename (temp_root, "parent", NULL);
	theme_child_root = g_build_filename (temp_root, "child", NULL);
	theme_switch_root = g_build_filename (temp_root, "switch", NULL);
	g_assert_cmpint (g_mkdir_with_parents (theme_parent_root, 0700), ==, 0);
	g_assert_cmpint (g_mkdir_with_parents (theme_child_root, 0700), ==, 0);
	g_assert_cmpint (g_mkdir_with_parents (theme_switch_root, 0700), ==, 0);

	ensure_css_dir (theme_parent_root, "gtk-3.24");
	write_settings (theme_parent_root, "gtk-3.24",
		"[Settings]\n"
		"gtk-enable-animations=true\n"
		"gtk-cursor-blink-time=111\n");

	ensure_css_dir (theme_child_root, "gtk-3.0");
	ensure_css_dir (theme_child_root, "gtk-3.24");
	write_settings (theme_child_root, "gtk-3.0",
		"[Settings]\n"
		"gtk-enable-animations=false\n"
		"gtk-cursor-blink-time=222\n");
	write_settings (theme_child_root, "gtk-3.24",
		"[Settings]\n"
		"gtk-cursor-blink-time=333\n");

	ensure_css_dir (theme_switch_root, "gtk-3.24");
	write_settings (theme_switch_root, "gtk-3.24",
		"[Settings]\n"
		"gtk-enable-animations=false\n"
		"gtk-cursor-blink-time=444\n");

	path = g_build_filename (theme_parent_root, "index.theme", NULL);
	write_file (path, "[Desktop Entry]\nName=parent\n");
	g_free (path);
	path = g_build_filename (theme_child_root, "index.theme", NULL);
	write_file (path, "[Desktop Entry]\nName=child\nInherits=parent\n");
	g_free (path);
	path = g_build_filename (theme_switch_root, "index.theme", NULL);
	write_file (path, "[Desktop Entry]\nName=switch\n");
	g_free (path);
}

static void
teardown_themes (void)
{
	g_assert_nonnull (temp_root);
	remove_tree (temp_root);
	g_free (theme_parent_root);
	g_free (theme_child_root);
	g_free (theme_switch_root);
	g_free (temp_root);
	theme_parent_root = NULL;
	theme_child_root = NULL;
	theme_switch_root = NULL;
	temp_root = NULL;
}

static void
test_settings_layer_precedence (void)
{
	GError *error = NULL;

	if (!gtk_available)
	{
		g_test_message ("GTK display not available");
		return;
	}

	g_assert_true (theme_gtk3_apply ("layered", THEME_GTK3_VARIANT_PREFER_LIGHT, &error));
	g_assert_no_error (error);
	g_assert_false (get_bool_setting ("gtk-enable-animations"));
	g_assert_cmpint (get_int_setting ("gtk-cursor-blink-time"), ==, 333);
	g_assert_true (theme_gtk3_is_active ());
	theme_gtk3_disable ();
}

static void
test_settings_restored_on_disable_and_switch (void)
{
	GError *error = NULL;
	gboolean default_animations;
	gint default_blink;
	char *default_theme_name = NULL;
	char *active_theme_name = NULL;

	if (!gtk_available)
	{
		g_test_message ("GTK display not available");
		return;
	}


	default_animations = get_bool_setting ("gtk-enable-animations");
	default_blink = get_int_setting ("gtk-cursor-blink-time");
	g_object_get (gtk_settings_get_default (), "gtk-theme-name", &default_theme_name, NULL);

	g_assert_true (theme_gtk3_apply ("layered", THEME_GTK3_VARIANT_PREFER_LIGHT, &error));
	g_assert_no_error (error);
	g_assert_cmpint (get_int_setting ("gtk-cursor-blink-time"), ==, 333);
	g_object_get (gtk_settings_get_default (), "gtk-theme-name", &active_theme_name, NULL);
	g_assert_cmpstr (active_theme_name, ==, "child");
	g_free (active_theme_name);
	active_theme_name = NULL;

	g_assert_true (theme_gtk3_apply ("switch", THEME_GTK3_VARIANT_PREFER_LIGHT, &error));
	g_assert_no_error (error);
	g_assert_false (get_bool_setting ("gtk-enable-animations"));
	g_assert_cmpint (get_int_setting ("gtk-cursor-blink-time"), ==, 444);

	theme_gtk3_disable ();
	g_assert_cmpint (get_int_setting ("gtk-cursor-blink-time"), ==, default_blink);
	g_assert_cmpint (get_bool_setting ("gtk-enable-animations"), ==, default_animations);
	g_object_get (gtk_settings_get_default (), "gtk-theme-name", &active_theme_name, NULL);
	g_assert_cmpstr (active_theme_name, ==, default_theme_name);
	g_free (active_theme_name);
	g_free (default_theme_name);
	g_assert_false (theme_gtk3_is_active ());
}

int
main (int argc, char **argv)
{
	int rc;

	g_test_init (&argc, &argv, NULL);
	gtk_available = gtk_init_check (&argc, &argv);
	setup_themes ();

	g_test_add_func ("/theme/gtk3/settings_layer_precedence", test_settings_layer_precedence);
	g_test_add_func ("/theme/gtk3/settings_restored_on_disable_and_switch", test_settings_restored_on_disable_and_switch);

	prefs.hex_gui_gtk3_variant = THEME_GTK3_VARIANT_PREFER_LIGHT;

	if (!gtk_available)
		g_test_message ("Skipping GTK3 settings tests because GTK initialization failed");

	rc = g_test_run ();
	theme_gtk3_disable ();
	teardown_themes ();
	return rc;
}
