#include "../zoitechat.h"
#include "../gtk3-theme-service.h"
#include "../cfgfiles.h"

#include <glib/gstdio.h>

char *xdir = NULL;

char *
get_xdir (void)
{
	return xdir;
}

static void
write_text_file (const char *path, const char *contents)
{
	g_file_set_contents (path, contents, -1, NULL);
}

static char *
make_theme_dir (const char *base, const char *name, gboolean dark, gboolean with_index)
{
	char *root = g_build_filename (base, name, NULL);
	char *gtk_dir = g_build_filename (root, "gtk-3.0", NULL);
	char *css = g_build_filename (gtk_dir, "gtk.css", NULL);

	g_mkdir_with_parents (gtk_dir, 0700);
	write_text_file (css, "button { background-image: url(\"../assets/a.png\"); }");
	if (dark)
	{
		char *dark_css = g_build_filename (gtk_dir, "gtk-dark.css", NULL);
		write_text_file (dark_css, "button { color: #eee; }");
		g_free (dark_css);
	}
	if (with_index)
	{
		char *index = g_build_filename (root, "index.theme", NULL);
		write_text_file (index, "[Desktop Entry]\nName=Indexed Theme\n");
		g_free (index);
	}
	g_free (css);
	g_free (gtk_dir);
	return root;
}

static void
setup_test_xdir (char **tmp_root)
{
	char *root = g_dir_make_tmp ("zoitechat-gtk3-service-test-XXXXXX", NULL);
	xdir = g_build_filename (root, "config", NULL);
	g_mkdir_with_parents (xdir, 0700);
	*tmp_root = root;
}

static void
teardown_test_xdir (char *tmp_root)
{
	char *cmd;
	cmd = g_strdup_printf ("rm -rf %s", tmp_root);
	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);
	g_free (cmd);
	g_free (xdir);
	xdir = NULL;
	g_free (tmp_root);
}


static guint
count_extract_temp_dirs (void)
{
	GDir *dir;
	const char *name;
	guint count = 0;
	const char *tmp_dir = g_get_tmp_dir ();

	dir = g_dir_open (tmp_dir, 0, NULL);
	if (!dir)
		return 0;

	while ((name = g_dir_read_name (dir)) != NULL)
	{
		if (g_str_has_prefix (name, "zoitechat-gtk3-theme-"))
			count++;
	}

	g_dir_close (dir);
	return count;
}


static char *
make_theme_dir_with_inherits (const char *base, const char *name, const char *inherits)
{
	char *root = make_theme_dir (base, name, FALSE, FALSE);
	char *index = g_build_filename (root, "index.theme", NULL);
	char *contents;

	if (inherits && inherits[0])
		contents = g_strdup_printf ("[Desktop Entry]\nName=%s\nInherits=%s\n", name, inherits);
	else
		contents = g_strdup_printf ("[Desktop Entry]\nName=%s\n", name);
	write_text_file (index, contents);
	g_free (contents);
	g_free (index);
	return root;
}

static void
test_inheritance_chain_single_parent (void)
{
	char *tmp_root;
	char *themes_root;
	char *adwaita;
	char *child;
	GPtrArray *chain;

	setup_test_xdir (&tmp_root);
	themes_root = g_build_filename (tmp_root, "themes", NULL);
	g_mkdir_with_parents (themes_root, 0700);
	adwaita = make_theme_dir_with_inherits (themes_root, "Adwaita", NULL);
	child = make_theme_dir_with_inherits (themes_root, "Child", "Adwaita");

	chain = zoitechat_gtk3_theme_build_inheritance_chain (child);
	g_assert_nonnull (chain);
	g_assert_cmpuint (chain->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (chain, 0), ==, adwaita);
	g_assert_cmpstr (g_ptr_array_index (chain, 1), ==, child);

	g_ptr_array_unref (chain);
	g_free (child);
	g_free (adwaita);
	g_free (themes_root);
	teardown_test_xdir (tmp_root);
}

static void
test_inheritance_chain_multi_level (void)
{
	char *tmp_root;
	char *themes_root;
	char *base;
	char *middle;
	char *child;
	GPtrArray *chain;

	setup_test_xdir (&tmp_root);
	themes_root = g_build_filename (tmp_root, "themes", NULL);
	g_mkdir_with_parents (themes_root, 0700);
	base = make_theme_dir_with_inherits (themes_root, "Base", NULL);
	middle = make_theme_dir_with_inherits (themes_root, "Middle", "Base");
	child = make_theme_dir_with_inherits (themes_root, "Child", "Middle");

	chain = zoitechat_gtk3_theme_build_inheritance_chain (child);
	g_assert_nonnull (chain);
	g_assert_cmpuint (chain->len, ==, 3);
	g_assert_cmpstr (g_ptr_array_index (chain, 0), ==, base);
	g_assert_cmpstr (g_ptr_array_index (chain, 1), ==, middle);
	g_assert_cmpstr (g_ptr_array_index (chain, 2), ==, child);

	g_ptr_array_unref (chain);
	g_free (child);
	g_free (middle);
	g_free (base);
	g_free (themes_root);
	teardown_test_xdir (tmp_root);
}

static void
test_inheritance_chain_missing_parent (void)
{
	char *tmp_root;
	char *themes_root;
	char *child;
	GPtrArray *chain;

	setup_test_xdir (&tmp_root);
	themes_root = g_build_filename (tmp_root, "themes", NULL);
	g_mkdir_with_parents (themes_root, 0700);
	child = make_theme_dir_with_inherits (themes_root, "Child", "MissingParent");

	chain = zoitechat_gtk3_theme_build_inheritance_chain (child);
	g_assert_nonnull (chain);
	g_assert_cmpuint (chain->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (chain, 0), ==, child);

	g_ptr_array_unref (chain);
	g_free (child);
	g_free (themes_root);
	teardown_test_xdir (tmp_root);
}
static void
test_inheritance_chain_parent_from_xdg_data_home (void)
{
	char *tmp_root;
	char *child_root;
	char *home_dir;
	char *user_data_dir;
	char *saved_home;
	char *saved_xdg_data_home;
	char *parent;
	char *child;
	GPtrArray *chain;

	setup_test_xdir (&tmp_root);
	child_root = g_build_filename (tmp_root, "themes", NULL);
	home_dir = g_build_filename (tmp_root, "home", NULL);
	user_data_dir = g_build_filename (tmp_root, "xdg-data-home", NULL);
	g_mkdir_with_parents (child_root, 0700);
	g_mkdir_with_parents (home_dir, 0700);
	g_mkdir_with_parents (user_data_dir, 0700);

	saved_home = g_strdup (g_getenv ("HOME"));
	saved_xdg_data_home = g_strdup (g_getenv ("XDG_DATA_HOME"));

	g_setenv ("HOME", home_dir, TRUE);
	g_setenv ("XDG_DATA_HOME", user_data_dir, TRUE);

	{
		char *user_themes = g_build_filename (user_data_dir, "themes", NULL);
		g_mkdir_with_parents (user_themes, 0700);
		parent = make_theme_dir_with_inherits (user_themes, "ParentFromDataHome", NULL);
		g_free (user_themes);
	}
	child = make_theme_dir_with_inherits (child_root, "Child", "ParentFromDataHome");

	chain = zoitechat_gtk3_theme_build_inheritance_chain (child);
	g_assert_nonnull (chain);
	g_assert_cmpuint (chain->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (chain, 0), ==, parent);
	g_assert_cmpstr (g_ptr_array_index (chain, 1), ==, child);
	g_ptr_array_unref (chain);

	if (saved_home)
		g_setenv ("HOME", saved_home, TRUE);
	else
		g_unsetenv ("HOME");
	if (saved_xdg_data_home)
		g_setenv ("XDG_DATA_HOME", saved_xdg_data_home, TRUE);
	else
		g_unsetenv ("XDG_DATA_HOME");

	g_free (child);
	g_free (parent);
	g_free (saved_xdg_data_home);
	g_free (saved_home);
	g_free (user_data_dir);
	g_free (home_dir);
	g_free (child_root);
	teardown_test_xdir (tmp_root);
}

static void
test_inheritance_chain_parent_from_xdg_data_dirs (void)
{
	char *tmp_root;
	char *child_root;
	char *home_dir;
	char *system_data_dir;
	char *system_data_dirs;
	char *saved_home;
	char *saved_xdg_data_dirs;
	char *parent;
	char *child;
	GPtrArray *chain;

	setup_test_xdir (&tmp_root);
	child_root = g_build_filename (tmp_root, "themes", NULL);
	home_dir = g_build_filename (tmp_root, "home", NULL);
	system_data_dir = g_build_filename (tmp_root, "xdg-data-system", NULL);
	system_data_dirs = g_strdup_printf ("%s:/usr/share", system_data_dir);
	g_mkdir_with_parents (child_root, 0700);
	g_mkdir_with_parents (home_dir, 0700);
	g_mkdir_with_parents (system_data_dir, 0700);

	saved_home = g_strdup (g_getenv ("HOME"));
	saved_xdg_data_dirs = g_strdup (g_getenv ("XDG_DATA_DIRS"));

	g_setenv ("HOME", home_dir, TRUE);
	g_setenv ("XDG_DATA_DIRS", system_data_dirs, TRUE);

	{
		char *system_themes = g_build_filename (system_data_dir, "themes", NULL);
		g_mkdir_with_parents (system_themes, 0700);
		parent = make_theme_dir_with_inherits (system_themes, "ParentFromDataDirs", NULL);
		g_free (system_themes);
	}
	child = make_theme_dir_with_inherits (child_root, "Child", "ParentFromDataDirs");

	chain = zoitechat_gtk3_theme_build_inheritance_chain (child);
	g_assert_nonnull (chain);
	g_assert_cmpuint (chain->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (chain, 0), ==, parent);
	g_assert_cmpstr (g_ptr_array_index (chain, 1), ==, child);
	g_ptr_array_unref (chain);

	if (saved_home)
		g_setenv ("HOME", saved_home, TRUE);
	else
		g_unsetenv ("HOME");
	if (saved_xdg_data_dirs)
		g_setenv ("XDG_DATA_DIRS", saved_xdg_data_dirs, TRUE);
	else
		g_unsetenv ("XDG_DATA_DIRS");

	g_free (child);
	g_free (parent);
	g_free (saved_xdg_data_dirs);
	g_free (saved_home);
	g_free (system_data_dirs);
	g_free (system_data_dir);
	g_free (home_dir);
	g_free (child_root);
	teardown_test_xdir (tmp_root);
}

static void
test_invalid_archive_reports_extract_error (void)
{
	char *tmp_root;
	char *bad_archive;
	char *imported_id = NULL;
	GError *error = NULL;
	guint before_count;
	guint after_count;

	setup_test_xdir (&tmp_root);
	bad_archive = g_build_filename (tmp_root, "bad-theme.tar.xz", NULL);
	write_text_file (bad_archive, "this is not a real archive");
	before_count = count_extract_temp_dirs ();

	g_assert_false (zoitechat_gtk3_theme_service_import (bad_archive, &imported_id, &error));
	g_assert_null (imported_id);
	g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED);
	g_assert_cmpstr (error->message, ==, "Failed to extract theme archive.");
	g_error_free (error);
	after_count = count_extract_temp_dirs ();
	g_assert_cmpuint (after_count, ==, before_count);

	g_free (bad_archive);
	teardown_test_xdir (tmp_root);
}

static void
test_archive_without_theme_reports_css_error (void)
{
	char *tmp_root;
	char *archive_root;
	char *archive_path;
	char *command;
	char *imported_id = NULL;
	GError *error = NULL;

	setup_test_xdir (&tmp_root);
	archive_root = g_build_filename (tmp_root, "invalid-theme-root", NULL);
	g_mkdir_with_parents (archive_root, 0700);
	{
		char *readme = g_build_filename (archive_root, "README.txt", NULL);
		write_text_file (readme, "not a gtk theme");
		g_free (readme);
	}
	archive_path = g_build_filename (tmp_root, "invalid-theme.zip", NULL);

	command = g_strdup_printf ("cd %s && zip -qr %s .", archive_root, archive_path);
	g_assert_true (g_spawn_command_line_sync (command, NULL, NULL, NULL, NULL));
	g_free (command);

	g_assert_false (zoitechat_gtk3_theme_service_import (archive_path, &imported_id, &error));
	g_assert_null (imported_id);
	g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
	g_assert_cmpstr (error->message, ==, "No GTK3 gtk.css file found in the selected theme.");
	g_error_free (error);

	g_free (archive_path);
	g_free (archive_root);
	teardown_test_xdir (tmp_root);
}

static void
test_import_rejects_theme_missing_index_theme (void)
{
	char *tmp_root;
	char *src_root;
	char *theme_root;
	char *imported_id = NULL;
	GError *error = NULL;

	setup_test_xdir (&tmp_root);
	src_root = g_build_filename (tmp_root, "src", NULL);
	g_mkdir_with_parents (src_root, 0700);
	theme_root = make_theme_dir (src_root, "NoIndex", FALSE, FALSE);

	g_assert_false (zoitechat_gtk3_theme_service_import (theme_root, &imported_id, &error));
	g_assert_null (imported_id);
	g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
	g_assert_nonnull (g_strstr_len (error->message, -1, "missing required index.theme"));
	g_assert_nonnull (g_strstr_len (error->message, -1, "NoIndex"));
	g_error_free (error);

	g_free (theme_root);
	g_free (src_root);
	teardown_test_xdir (tmp_root);
}

static void
test_import_rejects_index_without_desktop_entry (void)
{
	char *tmp_root;
	char *src_root;
	char *theme_root;
	char *index_path;
	char *imported_id = NULL;
	GError *error = NULL;

	setup_test_xdir (&tmp_root);
	src_root = g_build_filename (tmp_root, "src", NULL);
	g_mkdir_with_parents (src_root, 0700);
	theme_root = make_theme_dir (src_root, "NoDesktopEntry", FALSE, FALSE);
	index_path = g_build_filename (theme_root, "index.theme", NULL);
	write_text_file (index_path, "[X-GNOME-Metatheme]\nName=Broken\n");

	g_assert_false (zoitechat_gtk3_theme_service_import (theme_root, &imported_id, &error));
	g_assert_null (imported_id);
	g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
	g_assert_nonnull (g_strstr_len (error->message, -1, "missing the [Desktop Entry] section"));
	g_assert_nonnull (g_strstr_len (error->message, -1, "index.theme"));
	g_error_free (error);

	g_free (index_path);
	g_free (theme_root);
	g_free (src_root);
	teardown_test_xdir (tmp_root);
}

static void
test_import_rejects_unresolved_inherits (void)
{
	char *tmp_root;
	char *src_root;
	char *theme_root;
	char *imported_id = NULL;
	GError *error = NULL;

	setup_test_xdir (&tmp_root);
	src_root = g_build_filename (tmp_root, "src", NULL);
	g_mkdir_with_parents (src_root, 0700);
	theme_root = make_theme_dir_with_inherits (src_root, "ChildTheme", "MissingParent");

	g_assert_false (zoitechat_gtk3_theme_service_import (theme_root, &imported_id, &error));
	g_assert_null (imported_id);
	g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
	g_assert_nonnull (g_strstr_len (error->message, -1, "MissingParent"));
	g_assert_nonnull (g_strstr_len (error->message, -1, "could not be resolved"));
	g_error_free (error);

	g_free (theme_root);
	g_free (src_root);
	teardown_test_xdir (tmp_root);
}

static void
test_import_collision_and_dark_detection (void)
{
	char *tmp_root;
	char *src_root;
	char *theme_one;
	char *id_one = NULL;
	char *id_two = NULL;
	ZoitechatGtk3Theme *found;

	setup_test_xdir (&tmp_root);
	src_root = g_build_filename (tmp_root, "src", NULL);
	g_mkdir_with_parents (src_root, 0700);
	theme_one = make_theme_dir (src_root, "Ocean", TRUE, FALSE);

	g_assert_true (zoitechat_gtk3_theme_service_import (theme_one, &id_one, NULL));
	g_assert_true (zoitechat_gtk3_theme_service_import (theme_one, &id_two, NULL));
	g_assert_nonnull (id_one);
	g_assert_nonnull (id_two);
	g_assert_cmpstr (id_one, !=, id_two);

	found = zoitechat_gtk3_theme_find_by_id (id_two);
	g_assert_nonnull (found);
	g_assert_true (found->has_dark_variant);
	g_assert_true (g_str_has_suffix (found->path, "Ocean-1"));

	zoitechat_gtk3_theme_free (found);
	g_free (id_one);
	g_free (id_two);
	g_free (theme_one);
	g_free (src_root);
	teardown_test_xdir (tmp_root);
}

static void
test_discover_includes_user_and_system_data_dirs (void)
{
	char *tmp_root;
	char *home_dir;
	char *user_data_dir;
	char *system_data_dir;
	char *system_data_dirs;
	char *saved_home;
	char *saved_xdg_data_home;
	char *saved_xdg_data_dirs;
	char *user_themes_dir;
	char *system_themes_dir;
	char *user_theme;
	char *system_theme;
	GPtrArray *themes;
	guint i;
	gboolean found_user = FALSE;
	gboolean found_system = FALSE;

	setup_test_xdir (&tmp_root);
	home_dir = g_build_filename (tmp_root, "home", NULL);
	user_data_dir = g_build_filename (tmp_root, "xdg-data-home", NULL);
	system_data_dir = g_build_filename (tmp_root, "xdg-data-system", NULL);
	system_data_dirs = g_strdup_printf ("%s:/usr/share", system_data_dir);
	user_themes_dir = g_build_filename (user_data_dir, "themes", NULL);
	system_themes_dir = g_build_filename (system_data_dir, "themes", NULL);

	g_mkdir_with_parents (home_dir, 0700);
	g_mkdir_with_parents (user_themes_dir, 0700);
	g_mkdir_with_parents (system_themes_dir, 0700);
	user_theme = make_theme_dir (user_themes_dir, "UserDataTheme", FALSE, FALSE);
	system_theme = make_theme_dir (system_themes_dir, "SystemDataTheme", FALSE, FALSE);

	saved_home = g_strdup (g_getenv ("HOME"));
	saved_xdg_data_home = g_strdup (g_getenv ("XDG_DATA_HOME"));
	saved_xdg_data_dirs = g_strdup (g_getenv ("XDG_DATA_DIRS"));

	g_setenv ("HOME", home_dir, TRUE);
	g_setenv ("XDG_DATA_HOME", user_data_dir, TRUE);
	g_setenv ("XDG_DATA_DIRS", system_data_dirs, TRUE);

	themes = zoitechat_gtk3_theme_service_discover ();
	g_assert_nonnull (themes);

	for (i = 0; i < themes->len; i++)
	{
		ZoitechatGtk3Theme *theme = g_ptr_array_index (themes, i);
		if (g_strcmp0 (theme->path, user_theme) == 0)
		{
			found_user = TRUE;
			g_assert_cmpint (theme->source, ==, ZOITECHAT_GTK3_THEME_SOURCE_USER);
		}
		if (g_strcmp0 (theme->path, system_theme) == 0)
		{
			found_system = TRUE;
			g_assert_cmpint (theme->source, ==, ZOITECHAT_GTK3_THEME_SOURCE_SYSTEM);
		}
	}

	g_assert_true (found_user);
	g_assert_true (found_system);
	g_ptr_array_unref (themes);

	if (saved_home)
		g_setenv ("HOME", saved_home, TRUE);
	else
		g_unsetenv ("HOME");
	if (saved_xdg_data_home)
		g_setenv ("XDG_DATA_HOME", saved_xdg_data_home, TRUE);
	else
		g_unsetenv ("XDG_DATA_HOME");
	if (saved_xdg_data_dirs)
		g_setenv ("XDG_DATA_DIRS", saved_xdg_data_dirs, TRUE);
	else
		g_unsetenv ("XDG_DATA_DIRS");

	g_free (saved_xdg_data_dirs);
	g_free (saved_xdg_data_home);
	g_free (saved_home);
	g_free (system_theme);
	g_free (user_theme);
	g_free (system_themes_dir);
	g_free (user_themes_dir);
	g_free (system_data_dirs);
	g_free (system_data_dir);
	g_free (user_data_dir);
	g_free (home_dir);
	teardown_test_xdir (tmp_root);
}

static void
test_archive_root_detection_prefers_index (void)
{
	char *tmp_root;
	char *archive_root;
	char *theme_a;
	char *theme_b_parent;
	char *theme_b;
	char *archive_path;
	char *command;
	char *imported_id = NULL;
	ZoitechatGtk3Theme *found;

	setup_test_xdir (&tmp_root);
	archive_root = g_build_filename (tmp_root, "archive-root", NULL);
	g_mkdir_with_parents (archive_root, 0700);
	theme_a = make_theme_dir (archive_root, "Flat", FALSE, FALSE);
	theme_b_parent = g_build_filename (archive_root, "nested", NULL);
	g_mkdir_with_parents (theme_b_parent, 0700);
	theme_b = make_theme_dir (theme_b_parent, "Indexed", FALSE, TRUE);
	archive_path = g_build_filename (tmp_root, "themes.tar.xz", NULL);

	command = g_strdup_printf ("tar -cJf %s -C %s .", archive_path, archive_root);
	g_assert_true (g_spawn_command_line_sync (command, NULL, NULL, NULL, NULL));
	g_free (command);

	g_assert_true (zoitechat_gtk3_theme_service_import (archive_path, &imported_id, NULL));
	found = zoitechat_gtk3_theme_find_by_id (imported_id);
	g_assert_nonnull (found);
	g_assert_true (g_str_has_suffix (found->path, "Indexed"));

	zoitechat_gtk3_theme_free (found);
	g_free (imported_id);
	g_free (archive_path);
	g_free (theme_b);
	g_free (theme_b_parent);
	g_free (theme_a);
	g_free (archive_root);
	teardown_test_xdir (tmp_root);
}

static void
test_zip_import_nested_root (void)
{
	char *tmp_root;
	char *zip_root;
	char *nested;
	char *theme;
	char *archive_path;
	char *command;
	char *imported_id = NULL;
	ZoitechatGtk3Theme *found;

	setup_test_xdir (&tmp_root);
	zip_root = g_build_filename (tmp_root, "zip-root", NULL);
	nested = g_build_filename (zip_root, "bundle", "themes", NULL);
	g_mkdir_with_parents (nested, 0700);
	theme = make_theme_dir (nested, "Juno-ocean", TRUE, FALSE);
	archive_path = g_build_filename (tmp_root, "themes.zip", NULL);

	command = g_strdup_printf ("cd %s && zip -qr %s .", zip_root, archive_path);
	g_assert_true (g_spawn_command_line_sync (command, NULL, NULL, NULL, NULL));
	g_free (command);

	g_assert_true (zoitechat_gtk3_theme_service_import (archive_path, &imported_id, NULL));
	found = zoitechat_gtk3_theme_find_by_id (imported_id);
	g_assert_nonnull (found);
	g_assert_true (found->has_dark_variant);
	g_assert_true (g_str_has_suffix (found->path, "Juno-ocean"));

	zoitechat_gtk3_theme_free (found);
	g_free (imported_id);
	g_free (archive_path);
	g_free (theme);
	g_free (nested);
	g_free (zip_root);
	teardown_test_xdir (tmp_root);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/gtk3-theme-service/inheritance-single-parent", test_inheritance_chain_single_parent);
	g_test_add_func ("/gtk3-theme-service/inheritance-multi-level", test_inheritance_chain_multi_level);
	g_test_add_func ("/gtk3-theme-service/inheritance-missing-parent", test_inheritance_chain_missing_parent);
	g_test_add_func ("/gtk3-theme-service/inheritance-parent-from-xdg-data-home", test_inheritance_chain_parent_from_xdg_data_home);
	g_test_add_func ("/gtk3-theme-service/inheritance-parent-from-xdg-data-dirs", test_inheritance_chain_parent_from_xdg_data_dirs);
	g_test_add_func ("/gtk3-theme-service/import-collision-dark", test_import_collision_and_dark_detection);
	g_test_add_func ("/gtk3-theme-service/discover-user-and-system-data-dirs", test_discover_includes_user_and_system_data_dirs);
	g_test_add_func ("/gtk3-theme-service/archive-root-detection", test_archive_root_detection_prefers_index);
	g_test_add_func ("/gtk3-theme-service/zip-import-nested-root", test_zip_import_nested_root);
	g_test_add_func ("/gtk3-theme-service/invalid-archive-extract-error", test_invalid_archive_reports_extract_error);
	g_test_add_func ("/gtk3-theme-service/archive-without-theme-css-error", test_archive_without_theme_reports_css_error);
	g_test_add_func ("/gtk3-theme-service/import-missing-index-theme", test_import_rejects_theme_missing_index_theme);
	g_test_add_func ("/gtk3-theme-service/import-missing-desktop-entry", test_import_rejects_index_without_desktop_entry);
	g_test_add_func ("/gtk3-theme-service/import-unresolved-inherits", test_import_rejects_unresolved_inherits);
	return g_test_run ();
}
