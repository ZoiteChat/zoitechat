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
#include <gio/gio.h>

#include "../theme-gtk3.h"
#include "../../../common/gtk3-theme-service.h"

struct session *current_sess;
struct session *current_tab;
struct session *lastact_sess;
struct zoitechatprefs prefs;

static gboolean gtk_available;
static char *temp_root;
static char *xdg_data_home;
static char *theme_parent_root;
static char *theme_child_root;
static char *theme_switch_root;
static char *theme_chrome_root;

static gboolean stub_system_prefers_dark;

gboolean
theme_policy_system_prefers_dark (void)
{
	return stub_system_prefers_dark;
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
	if (g_strcmp0 (theme_id, "chrome") == 0)
		return make_theme (theme_id, theme_chrome_root);
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
	if (g_strcmp0 (theme_root, theme_chrome_root) == 0)
	{
		g_ptr_array_add (chain, g_strdup (theme_chrome_root));
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

static gboolean
has_default_seat (void)
{
	GdkDisplay *display = gdk_display_get_default ();

	return display && GDK_IS_SEAT (gdk_display_get_default_seat (display));
}

static void
setup_themes (void)
{
	char *path;

	temp_root = g_dir_make_tmp ("zoitechat-theme-gtk3-settings-XXXXXX", NULL);
	g_assert_nonnull (temp_root);
	xdg_data_home = g_build_filename (temp_root, "data", NULL);
	g_assert_cmpint (g_mkdir_with_parents (xdg_data_home, 0700), ==, 0);
	g_setenv ("XDG_DATA_HOME", xdg_data_home, TRUE);

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

	theme_chrome_root = g_build_filename (temp_root, "chrome", NULL);
	g_assert_cmpint (g_mkdir_with_parents (theme_chrome_root, 0700), ==, 0);
	ensure_css_dir (theme_chrome_root, "gtk-3.24");
	/* Defines named colors but no window/menubar/menu rules of its own,
	 * so all chrome styling must come from the backfill provider. */
	{
		char *css = g_build_filename (theme_chrome_root, "gtk-3.24", "gtk.css", NULL);
		write_file (css,
			"@define-color theme_bg_color #e8f4e8;\n"
			"@define-color theme_fg_color #102010;\n"
			"@define-color theme_selected_bg_color #3465a4;\n"
			"@define-color theme_selected_fg_color #ffffff;\n"
			"label { color: #102010; }\n");
		g_free (css);
	}

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
	g_free (theme_chrome_root);
	g_free (xdg_data_home);
	g_free (temp_root);
	theme_parent_root = NULL;
	theme_child_root = NULL;
	theme_switch_root = NULL;
	theme_chrome_root = NULL;
	xdg_data_home = NULL;
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
	if (has_default_seat ())
	{
		g_object_get (gtk_settings_get_default (), "gtk-theme-name", &active_theme_name, NULL);
		g_assert_cmpstr (active_theme_name, ==, "child");
		g_free (active_theme_name);
		active_theme_name = NULL;
	}

	g_assert_true (theme_gtk3_apply ("switch", THEME_GTK3_VARIANT_PREFER_LIGHT, &error));
	g_assert_no_error (error);
	g_assert_false (get_bool_setting ("gtk-enable-animations"));
	g_assert_cmpint (get_int_setting ("gtk-cursor-blink-time"), ==, 444);

	theme_gtk3_disable ();
	g_assert_cmpint (get_int_setting ("gtk-cursor-blink-time"), ==, default_blink);
	g_assert_cmpint (get_bool_setting ("gtk-enable-animations"), ==, default_animations);
	if (has_default_seat ())
	{
		g_object_get (gtk_settings_get_default (), "gtk-theme-name", &active_theme_name, NULL);
		g_assert_cmpstr (active_theme_name, ==, default_theme_name);
		g_free (active_theme_name);
	}
	g_free (default_theme_name);
	g_assert_false (theme_gtk3_is_active ());
}

static void
test_follow_system_variant_resolves_to_theme_variant (void)
{
	GError *error = NULL;

	if (!gtk_available)
	{
		g_test_message ("GTK display not available");
		return;
	}

	/* An explicitly selected in-app theme must not follow the OS
	 * light/dark preference: the legacy FOLLOW_SYSTEM variant resolves
	 * to the variant the theme itself provides, so the window chrome
	 * (menu bar) stays on the theme even when the system prefers dark. */
	stub_system_prefers_dark = TRUE;
	g_assert_true (theme_gtk3_apply ("switch", THEME_GTK3_VARIANT_FOLLOW_SYSTEM, &error));
	g_assert_no_error (error);
	g_assert_true (theme_gtk3_is_active ());
	g_assert_cmpint (theme_gtk3_active_variant (), ==, THEME_GTK3_VARIANT_PREFER_LIGHT);
	g_assert_false (get_bool_setting ("gtk-application-prefer-dark-theme"));

	stub_system_prefers_dark = FALSE;
	theme_gtk3_disable ();
}

static void
test_menu_bar_follows_active_theme_colors (void)
{
	GError *error = NULL;
	GtkWidget *window;
	GtkWidget *menubar;
	GtkStyleContext *context;
	GdkRGBA bg;
	GdkRGBA expected;

	if (!gtk_available)
	{
		g_test_message ("GTK display not available");
		return;
	}

	/* The chrome theme defines no menubar rules; without the backfill
	 * provider the menu bar would fall through to the base (OS) theme
	 * and follow the OS dark/light mode instead of the selected theme. */
	g_assert_true (theme_gtk3_apply ("chrome", THEME_GTK3_VARIANT_PREFER_LIGHT, &error));
	g_assert_no_error (error);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	menubar = gtk_menu_bar_new ();
	gtk_container_add (GTK_CONTAINER (window), menubar);
	gtk_widget_show_all (window);

	context = gtk_widget_get_style_context (menubar);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);
	G_GNUC_END_IGNORE_DEPRECATIONS
	g_assert_true (gdk_rgba_parse (&expected, "#e8f4e8"));
	g_assert_cmpfloat (ABS (bg.red - expected.red), <, 0.01);
	g_assert_cmpfloat (ABS (bg.green - expected.green), <, 0.01);
	g_assert_cmpfloat (ABS (bg.blue - expected.blue), <, 0.01);

	/* The window background must follow the theme too, or the space
	 * around and beside the menu bar keeps the OS theme's colors. */
	context = gtk_widget_get_style_context (window);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);
	G_GNUC_END_IGNORE_DEPRECATIONS
	g_assert_cmpfloat (ABS (bg.red - expected.red), <, 0.01);
	g_assert_cmpfloat (ABS (bg.green - expected.green), <, 0.01);
	g_assert_cmpfloat (ABS (bg.blue - expected.blue), <, 0.01);

	gtk_widget_destroy (window);
	theme_gtk3_disable ();
}

static void
test_desktop_color_sync_does_not_leak_into_chrome (void)
{
	GError *error = NULL;
	GtkWidget *window;
	GtkWidget *menubar;
	GtkStyleContext *context;
	GtkCssProvider *desktop_css;
	GdkScreen *screen;
	GdkRGBA bg;
	GdkRGBA expected;
	GdkRGBA injected;

	if (!gtk_available)
	{
		g_test_message ("GTK display not available");
		return;
	}

	/* KDE's GTK color-scheme sync (~/.config/gtk-3.0/gtk.css) redefines
	 * the standard named colors with the OS palette at USER priority.
	 * The switch theme defines no named colors of its own, so a naive
	 * @theme_bg_color reference in the chrome backfill would resolve to
	 * the injected OS dark palette; the backfill must use its neutral
	 * variant fallback instead. */
	screen = gdk_screen_get_default ();
	desktop_css = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (desktop_css,
		"@define-color theme_bg_color #1e1e1e;\n"
		"@define-color theme_fg_color #eeeeee;\n"
		"menubar { background-color: #1e1e1e; }\n",
		-1, NULL);
	gtk_style_context_add_provider_for_screen (screen,
		GTK_STYLE_PROVIDER (desktop_css), GTK_STYLE_PROVIDER_PRIORITY_USER);

	g_assert_true (theme_gtk3_apply ("switch", THEME_GTK3_VARIANT_PREFER_LIGHT, &error));
	g_assert_no_error (error);

	{
		GtkWidget *item;
		GdkRGBA item_bg;

		window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		menubar = gtk_menu_bar_new ();
		item = gtk_menu_item_new_with_label ("File");
		gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
		gtk_container_add (GTK_CONTAINER (window), menubar);
		gtk_widget_show_all (window);

		context = gtk_widget_get_style_context (menubar);
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);
		G_GNUC_END_IGNORE_DEPRECATIONS
		g_assert_true (gdk_rgba_parse (&expected, "#f6f5f4"));
		g_assert_true (gdk_rgba_parse (&injected, "#1e1e1e"));
		g_assert_cmpfloat (ABS (bg.red - injected.red), >, 0.1);
		g_assert_cmpfloat (ABS (bg.red - expected.red), <, 0.01);
		g_assert_cmpfloat (ABS (bg.green - expected.green), <, 0.01);
		g_assert_cmpfloat (ABS (bg.blue - expected.blue), <, 0.01);

		/* The menu bar body and the menu items must be one surface. */
		context = gtk_widget_get_style_context (item);
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &item_bg);
		G_GNUC_END_IGNORE_DEPRECATIONS
		g_assert_cmpfloat (ABS (item_bg.red - bg.red), <, 0.01);
		g_assert_cmpfloat (ABS (item_bg.green - bg.green), <, 0.01);
		g_assert_cmpfloat (ABS (item_bg.blue - bg.blue), <, 0.01);
	}

	gtk_widget_destroy (window);
	theme_gtk3_disable ();
	gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (desktop_css));
	g_object_unref (desktop_css);
}

static void
test_stale_theme_symlink_is_replaced (void)
{
	GError *error = NULL;
	char *themes_dir;
	char *link_path;
	char *target;
	char *active_theme_name = NULL;

	if (!gtk_available)
	{
		g_test_message ("GTK display not available");
		return;
	}
	if (!has_default_seat ())
	{
		g_test_message ("No default seat available");
		return;
	}

	/* A dangling symlink left behind by a removed theme must not block
	 * pinning gtk-theme-name to the newly applied theme. */
	themes_dir = g_build_filename (xdg_data_home, "themes", NULL);
	g_assert_cmpint (g_mkdir_with_parents (themes_dir, 0700), ==, 0);
	link_path = g_build_filename (themes_dir, "switch", NULL);
	g_unlink (link_path);
	{
		GFile *link_file = g_file_new_for_path (link_path);
		g_assert_true (g_file_make_symbolic_link (link_file, "/nonexistent/switch", NULL, NULL));
		g_object_unref (link_file);
	}

	g_assert_true (theme_gtk3_apply ("switch", THEME_GTK3_VARIANT_PREFER_LIGHT, &error));
	g_assert_no_error (error);

	target = g_file_read_link (link_path, NULL);
	g_assert_cmpstr (target, ==, theme_switch_root);
	g_object_get (gtk_settings_get_default (), "gtk-theme-name", &active_theme_name, NULL);
	g_assert_cmpstr (active_theme_name, ==, "switch");

	g_free (active_theme_name);
	g_free (target);
	g_free (link_path);
	g_free (themes_dir);
	theme_gtk3_disable ();
}

int
main (int argc, char **argv)
{
	int rc;

	g_test_init (&argc, &argv, NULL);
	setup_themes ();
	gtk_available = gtk_init_check (&argc, &argv);

	g_test_add_func ("/theme/gtk3/settings_layer_precedence", test_settings_layer_precedence);
	g_test_add_func ("/theme/gtk3/settings_restored_on_disable_and_switch", test_settings_restored_on_disable_and_switch);
	g_test_add_func ("/theme/gtk3/follow_system_variant_resolves_to_theme_variant",
			 test_follow_system_variant_resolves_to_theme_variant);
	g_test_add_func ("/theme/gtk3/menu_bar_follows_active_theme_colors",
			 test_menu_bar_follows_active_theme_colors);
	g_test_add_func ("/theme/gtk3/desktop_color_sync_does_not_leak_into_chrome",
			 test_desktop_color_sync_does_not_leak_into_chrome);
	g_test_add_func ("/theme/gtk3/stale_theme_symlink_is_replaced",
			 test_stale_theme_symlink_is_replaced);

	prefs.hex_gui_gtk3_variant = THEME_GTK3_VARIANT_PREFER_LIGHT;

	if (!gtk_available)
		g_test_message ("Skipping GTK3 settings tests because GTK initialization failed");

	rc = g_test_run ();
	theme_gtk3_disable ();
	teardown_themes ();
	return rc;
}
