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

#include "gtk3-theme-service.h"

#ifndef G_OS_WIN32
#if defined(__has_include)
#if __has_include(<archive.h>)
#include <archive.h>
#include <archive_entry.h>
#elif __has_include(<libarchive/archive.h>)
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>
#elif __has_include(<archive/archive.h>)
#include <archive/archive.h>
#include <archive/archive_entry.h>
#else
#error "libarchive headers not found"
#endif
#else
#include <archive.h>
#include <archive_entry.h>
#endif
#endif
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifndef G_OS_WIN32
extern char *realpath (const char *path, char *resolved_path);
#endif

#include "util.h"
#include "cfgfiles.h"

static void
remove_tree (const char *path)
{
	GDir *dir;
	const char *name;

	if (!g_file_test (path, G_FILE_TEST_EXISTS))
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

#ifdef G_OS_WIN32
static gboolean
path_tree_has_entries (const char *path)
{
	GDir *dir;
	const char *name;

	if (!path || !g_file_test (path, G_FILE_TEST_EXISTS))
		return FALSE;

	if (!g_file_test (path, G_FILE_TEST_IS_DIR))
		return TRUE;

	dir = g_dir_open (path, 0, NULL);
	if (!dir)
		return FALSE;

	while ((name = g_dir_read_name (dir)) != NULL)
	{
		char *child = g_build_filename (path, name, NULL);
		gboolean has_entries = path_tree_has_entries (child);
		g_free (child);
		if (has_entries)
		{
			g_dir_close (dir);
			return TRUE;
		}
	}

	g_dir_close (dir);
	return FALSE;
}
#endif

static gboolean
gtk3_css_dir_parse_minor (const char *name, gint *minor)
{
	gint parsed_minor = 0;

	if (!g_str_has_prefix (name, "gtk-3"))
		return FALSE;

	if (name[5] == '\0')
	{
		*minor = 0;
		return TRUE;
	}

	if (name[5] != '.')
		return FALSE;

	if (!name[6])
		return FALSE;

	for (const char *p = name + 6; *p; p++)
	{
		if (!g_ascii_isdigit (*p))
			return FALSE;
		parsed_minor = (parsed_minor * 10) + (*p - '0');
	}

	*minor = parsed_minor;
	return TRUE;
}

char *
zoitechat_gtk3_theme_pick_css_dir_for_minor (const char *theme_root, int preferred_minor)
{
	GDir *dir;
	const char *name;
	char *best_supported = NULL;
	char *best_fallback = NULL;
	gint best_supported_minor = G_MININT;
	gint best_fallback_minor = G_MININT;

	if (!theme_root || !g_file_test (theme_root, G_FILE_TEST_IS_DIR))
		return NULL;

	dir = g_dir_open (theme_root, 0, NULL);
	if (!dir)
		return NULL;

	while ((name = g_dir_read_name (dir)) != NULL)
	{
		char *css_path;
		gint minor = 0;

		if (!gtk3_css_dir_parse_minor (name, &minor))
			continue;

		css_path = g_build_filename (theme_root, name, "gtk.css", NULL);
		if (!g_file_test (css_path, G_FILE_TEST_IS_REGULAR))
		{
			g_free (css_path);
			continue;
		}
		g_free (css_path);

		if (preferred_minor >= 0 && minor <= preferred_minor)
		{
			if (minor > best_supported_minor)
			{
				g_free (best_supported);
				best_supported = g_strdup (name);
				best_supported_minor = minor;
			}
		}
		if (minor > best_fallback_minor)
		{
			g_free (best_fallback);
			best_fallback = g_strdup (name);
			best_fallback_minor = minor;
		}
	}

	g_dir_close (dir);

	if (best_supported)
	{
		g_free (best_fallback);
		return best_supported;
	}

	return best_fallback;
}

char *
zoitechat_gtk3_theme_pick_css_dir (const char *theme_root)
{
	return zoitechat_gtk3_theme_pick_css_dir_for_minor (theme_root, -1);
}

static gboolean
path_has_gtk3_css (const char *root)
{
	char *css_dir = zoitechat_gtk3_theme_pick_css_dir (root);
	gboolean ok = css_dir != NULL;
	g_free (css_dir);
	return ok;
}

static char **
path_read_inherits (const char *theme_root)
{
	char *index_theme;
	GKeyFile *keyfile;
	char *raw;
	char **tokens;
	GPtrArray *parents;
	guint i;

	if (!theme_root)
		return NULL;

	index_theme = g_build_filename (theme_root, "index.theme", NULL);
	if (!g_file_test (index_theme, G_FILE_TEST_IS_REGULAR))
	{
		g_free (index_theme);
		return NULL;
	}

	keyfile = g_key_file_new ();
	if (!g_key_file_load_from_file (keyfile, index_theme, G_KEY_FILE_NONE, NULL))
	{
		g_key_file_unref (keyfile);
		g_free (index_theme);
		return NULL;
	}

	raw = g_key_file_get_string (keyfile, "Desktop Entry", "Inherits", NULL);
	g_key_file_unref (keyfile);
	g_free (index_theme);
	if (!raw)
		return NULL;

	tokens = g_strsplit_set (raw, ",;", -1);
	g_free (raw);
	parents = g_ptr_array_new_with_free_func (g_free);

	for (i = 0; tokens && tokens[i]; i++)
	{
		char *name = g_strstrip (tokens[i]);
		if (name[0] == '\0')
			continue;
		g_ptr_array_add (parents, g_strdup (name));
	}

	g_strfreev (tokens);
	g_ptr_array_add (parents, NULL);
	return (char **) g_ptr_array_free (parents, FALSE);
}

static gboolean
path_exists_as_dir (const char *path)
{
	return path && g_file_test (path, G_FILE_TEST_IS_DIR);
}

static char *
resolve_parent_theme_root (const char *child_theme_root, const char *parent_name)
{
	char *candidate;
	char *child_parent;
	char *user_dir;
	char *home_themes;
	char *home_local;

	if (!parent_name || !parent_name[0])
		return NULL;

	if (g_path_is_absolute (parent_name) && path_exists_as_dir (parent_name))
		return g_strdup (parent_name);

	child_parent = g_path_get_dirname (child_theme_root);
	candidate = g_build_filename (child_parent, parent_name, NULL);
	g_free (child_parent);
	if (path_exists_as_dir (candidate))
		return candidate;
	g_free (candidate);

	candidate = g_build_filename ("/usr/share/themes", parent_name, NULL);
	if (path_exists_as_dir (candidate))
		return candidate;
	g_free (candidate);

	home_themes = g_build_filename (g_get_home_dir (), ".themes", parent_name, NULL);
	if (path_exists_as_dir (home_themes))
		return home_themes;
	g_free (home_themes);

	home_local = g_build_filename (g_get_home_dir (), ".local", "share", "themes", parent_name, NULL);
	if (path_exists_as_dir (home_local))
		return home_local;
	g_free (home_local);

	user_dir = zoitechat_gtk3_theme_service_get_user_themes_dir ();
	candidate = g_build_filename (user_dir, parent_name, NULL);
	g_free (user_dir);
	if (path_exists_as_dir (candidate))
		return candidate;
	g_free (candidate);

	return NULL;
}

static void
build_inheritance_chain_recursive (const char *theme_root,
								   GPtrArray *ordered_roots,
								   GHashTable *visited)
{
	char **parents;
	guint i;

	if (!theme_root || g_hash_table_contains (visited, theme_root))
		return;

	g_hash_table_add (visited, g_strdup (theme_root));
	parents = path_read_inherits (theme_root);
	for (i = 0; parents && parents[i]; i++)
	{
		char *parent_root = resolve_parent_theme_root (theme_root, parents[i]);
		if (!parent_root)
			continue;
		build_inheritance_chain_recursive (parent_root, ordered_roots, visited);
		g_free (parent_root);
	}
	g_strfreev (parents);

	if (path_has_gtk3_css (theme_root))
		g_ptr_array_add (ordered_roots, g_strdup (theme_root));
}

GPtrArray *
zoitechat_gtk3_theme_build_inheritance_chain (const char *theme_root)
{
	GPtrArray *ordered_roots;
	GHashTable *visited;

	if (!theme_root || !path_exists_as_dir (theme_root))
		return NULL;

	ordered_roots = g_ptr_array_new_with_free_func (g_free);
	visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	build_inheritance_chain_recursive (theme_root, ordered_roots, visited);
	g_hash_table_destroy (visited);

	if (ordered_roots->len == 0)
	{
		g_ptr_array_unref (ordered_roots);
		return NULL;
	}

	return ordered_roots;
}

static char *
path_build_id (const char *path, ZoitechatGtk3ThemeSource source)
{
	char *digest = g_compute_checksum_for_string (G_CHECKSUM_SHA1, path, -1);
	char *id = g_strdup_printf ("%s:%s", source == ZOITECHAT_GTK3_THEME_SOURCE_USER ? "user" : "system", digest);
	g_free (digest);
	return id;
}

static char *
path_pick_thumbnail (const char *root)
{
	static const char *const names[] = {
		"thumbnail.png",
		"preview.png",
		"screenshot.png",
		NULL
	};
	int i;

	for (i = 0; names[i] != NULL; i++)
	{
		char *candidate = g_build_filename (root, names[i], NULL);
		if (g_file_test (candidate, G_FILE_TEST_IS_REGULAR))
			return candidate;
		g_free (candidate);

		char *css_dir = zoitechat_gtk3_theme_pick_css_dir (root);
		if (css_dir)
		{
			candidate = g_build_filename (root, css_dir, names[i], NULL);
			g_free (css_dir);
			if (g_file_test (candidate, G_FILE_TEST_IS_REGULAR))
				return candidate;
			g_free (candidate);
		}
	}

	return NULL;
}

static char *
path_read_display_name (const char *root)
{
	char *index_theme = g_build_filename (root, "index.theme", NULL);
	GKeyFile *keyfile = g_key_file_new ();
	char *name = NULL;

	if (g_file_test (index_theme, G_FILE_TEST_IS_REGULAR) &&
		g_key_file_load_from_file (keyfile, index_theme, G_KEY_FILE_NONE, NULL))
	{
		name = g_key_file_get_string (keyfile, "Desktop Entry", "Name", NULL);
		if (!name)
			name = g_key_file_get_string (keyfile, "X-GNOME-Metatheme", "Name", NULL);
	}

	if (!name)
		name = g_path_get_basename (root);

	g_key_file_unref (keyfile);
	g_free (index_theme);
	return name;
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

char *
zoitechat_gtk3_theme_service_get_user_themes_dir (void)
{
	return g_build_filename (get_xdir (), "gtk3-themes", NULL);
}

static char *path_normalize_theme_root (const char *path);

static void
discover_dir (GPtrArray *themes, GHashTable *seen_theme_roots, const char *base_dir, ZoitechatGtk3ThemeSource source)
{
	GDir *dir;
	const char *name;

	if (!g_file_test (base_dir, G_FILE_TEST_IS_DIR))
		return;

	dir = g_dir_open (base_dir, 0, NULL);
	if (!dir)
		return;

	while ((name = g_dir_read_name (dir)) != NULL)
	{
		ZoitechatGtk3Theme *theme;
		char *root;
		char *dark;
		char *css_dir;

		if (name[0] == '.')
			continue;

		root = g_build_filename (base_dir, name, NULL);
		if (!g_file_test (root, G_FILE_TEST_IS_DIR) || !path_has_gtk3_css (root))
		{
			g_free (root);
			continue;
		}

		if (seen_theme_roots)
		{
			char *canonical_root = path_normalize_theme_root (root);
			if (g_hash_table_contains (seen_theme_roots, canonical_root))
			{
				g_free (canonical_root);
				g_free (root);
				continue;
			}
			g_hash_table_add (seen_theme_roots, canonical_root);
		}

		theme = g_new0 (ZoitechatGtk3Theme, 1);
		theme->path = root;
		theme->source = source;
		theme->id = path_build_id (root, source);
		theme->display_name = path_read_display_name (root);
		theme->thumbnail_path = path_pick_thumbnail (root);
		css_dir = zoitechat_gtk3_theme_pick_css_dir (root);
		dark = css_dir ? g_build_filename (root, css_dir, "gtk-dark.css", NULL) : NULL;
		theme->has_dark_variant = g_file_test (dark, G_FILE_TEST_IS_REGULAR);
		g_free (css_dir);
		g_free (dark);
		g_ptr_array_add (themes, theme);
	}

	g_dir_close (dir);
}



static char *
path_canonicalize_compat (const char *path)
{
	char *absolute_path;
	char *cwd;
	char *resolved;

	if (!path || path[0] == '\0')
		return NULL;

	if (g_path_is_absolute (path))
		absolute_path = g_strdup (path);
	else
	{
		cwd = g_get_current_dir ();
		absolute_path = g_build_filename (cwd, path, NULL);
		g_free (cwd);
	}

#ifdef G_OS_WIN32
	resolved = _fullpath (NULL, absolute_path, 0);
	if (resolved)
	{
		char *copy = g_strdup (resolved);
		free (resolved);
		g_free (absolute_path);
		return copy;
	}
#else
	resolved = realpath (absolute_path, NULL);
	if (resolved)
	{
		char *copy = g_strdup (resolved);
		free (resolved);
		g_free (absolute_path);
		return copy;
	}
#endif

	return absolute_path;
}

static char *
path_normalize_theme_root (const char *path)
{
	char *canonical;
	char *target;

	if (!path || path[0] == '\0')
		return NULL;

	canonical = path_canonicalize_compat (path);
	target = g_file_read_link (canonical, NULL);
	if (target && target[0])
	{
		char *base = g_path_get_dirname (canonical);
		char *resolved = g_path_is_absolute (target)
		? g_strdup (target)
		: g_build_filename (base, target, NULL);
		g_free (canonical);
		canonical = path_canonicalize_compat (resolved);
		g_free (resolved);
		g_free (base);
	}
	g_free (target);
	return canonical;
}

static gint
theme_cmp (gconstpointer a, gconstpointer b)
{
	const ZoitechatGtk3Theme *ta = *(const ZoitechatGtk3Theme **) a;
	const ZoitechatGtk3Theme *tb = *(const ZoitechatGtk3Theme **) b;
	return g_ascii_strcasecmp (ta->display_name, tb->display_name);
}

static void
add_theme_root (GPtrArray *roots, GHashTable *seen, const char *path)
{
	char *normalized;

	if (!path || path[0] == '\0')
		return;

	normalized = path_canonicalize_compat (path);
	if (g_hash_table_contains (seen, normalized))
	{
		g_free (normalized);
		return;
	}

	g_hash_table_add (seen, normalized);
	g_ptr_array_add (roots, g_strdup (path));
}

GPtrArray *
zoitechat_gtk3_theme_service_discover (void)
{
	GPtrArray *themes = g_ptr_array_new_with_free_func ((GDestroyNotify) zoitechat_gtk3_theme_free);
	GPtrArray *system_roots = g_ptr_array_new_with_free_func (g_free);
	GHashTable *seen_system_roots = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	GPtrArray *user_roots = g_ptr_array_new_with_free_func (g_free);
	GHashTable *seen_user_roots = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	GHashTable *seen_theme_roots = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	const gchar *const *system_data_dirs;
	guint i;
	char *user_data_themes;
	char *user_dir = zoitechat_gtk3_theme_service_get_user_themes_dir ();
	char *home_themes = g_build_filename (g_get_home_dir (), ".themes", NULL);

	g_mkdir_with_parents (user_dir, 0700);

	user_data_themes = g_build_filename (g_get_user_data_dir (), "themes", NULL);
	add_theme_root (user_roots, seen_user_roots, user_data_themes);
	g_free (user_data_themes);
	add_theme_root (user_roots, seen_user_roots, home_themes);
	add_theme_root (user_roots, seen_user_roots, user_dir);

	system_data_dirs = g_get_system_data_dirs ();
	for (i = 0; system_data_dirs && system_data_dirs[i]; i++)
	{
		char *system_themes = g_build_filename (system_data_dirs[i], "themes", NULL);
		add_theme_root (system_roots, seen_system_roots, system_themes);
		g_free (system_themes);
	}

	for (i = 0; i < system_roots->len; i++)
		discover_dir (themes, seen_theme_roots, g_ptr_array_index (system_roots, i), ZOITECHAT_GTK3_THEME_SOURCE_SYSTEM);
	for (i = 0; i < user_roots->len; i++)
		discover_dir (themes, seen_theme_roots, g_ptr_array_index (user_roots, i), ZOITECHAT_GTK3_THEME_SOURCE_USER);
	g_ptr_array_sort (themes, theme_cmp);

	g_hash_table_destroy (seen_theme_roots);
	g_hash_table_destroy (seen_user_roots);
	g_ptr_array_unref (user_roots);
	g_hash_table_destroy (seen_system_roots);
	g_ptr_array_unref (system_roots);
	g_free (home_themes);
	g_free (user_dir);
	return themes;
}

ZoitechatGtk3Theme *
zoitechat_gtk3_theme_find_by_id (const char *theme_id)
{
	GPtrArray *themes;
	ZoitechatGtk3Theme *result = NULL;
	guint i;

	if (!theme_id || !*theme_id)
		return NULL;

	themes = zoitechat_gtk3_theme_service_discover ();
	for (i = 0; i < themes->len; i++)
	{
		ZoitechatGtk3Theme *theme = g_ptr_array_index (themes, i);
		if (g_strcmp0 (theme->id, theme_id) == 0)
		{
			result = g_new0 (ZoitechatGtk3Theme, 1);
			result->id = g_strdup (theme->id);
			result->display_name = g_strdup (theme->display_name);
			result->path = g_strdup (theme->path);
			result->thumbnail_path = g_strdup (theme->thumbnail_path);
			result->has_dark_variant = theme->has_dark_variant;
			result->source = theme->source;
			break;
		}
	}
	g_ptr_array_unref (themes);
	return result;
}

static void
collect_theme_roots (const char *root, GPtrArray *found, int depth)
{
	GDir *dir;
	const char *name;

	if (depth > 4)
		return;

	if (path_has_gtk3_css (root))
	{
		g_ptr_array_add (found, g_strdup (root));
		return;
	}

	dir = g_dir_open (root, 0, NULL);
	if (!dir)
		return;

	while ((name = g_dir_read_name (dir)) != NULL)
	{
		char *child;
		if (name[0] == '.')
			continue;
		child = g_build_filename (root, name, NULL);
		if (g_file_test (child, G_FILE_TEST_IS_DIR))
			collect_theme_roots (child, found, depth + 1);
		g_free (child);
	}

	g_dir_close (dir);
}



typedef struct
{
	char *path;
	int depth;
	gboolean has_index_theme;
} ThemeRootCandidate;

static void
theme_root_candidate_free (ThemeRootCandidate *candidate)
{
	if (!candidate)
		return;
	g_free (candidate->path);
	g_free (candidate);
}

static int
path_depth_from_root (const char *base, const char *path)
{
	int depth = 0;
	const char *cursor;
	size_t base_len;

	if (!base || !path)
		return 0;

	base_len = strlen (base);
	cursor = path + base_len;
	while (*cursor)
	{
		if (*cursor == G_DIR_SEPARATOR)
			depth++;
		cursor++;
	}

	return depth;
}

static gint
theme_root_candidate_compare (gconstpointer a, gconstpointer b)
{
	const ThemeRootCandidate *ca = a;
	const ThemeRootCandidate *cb = b;
	if (ca->has_index_theme != cb->has_index_theme)
		return ca->has_index_theme ? -1 : 1;
	if (ca->depth != cb->depth)
		return ca->depth - cb->depth;
	return g_ascii_strcasecmp (ca->path, cb->path);
}

static char *
select_theme_root (GPtrArray *roots, const char *input_root)
{
	GPtrArray *candidates;
	guint i;
	char *selected;

	if (!roots || roots->len == 0)
		return NULL;
	if (roots->len == 1)
		return g_strdup (g_ptr_array_index (roots, 0));

	candidates = g_ptr_array_new_with_free_func ((GDestroyNotify) theme_root_candidate_free);
	for (i = 0; i < roots->len; i++)
	{
		ThemeRootCandidate *candidate = g_new0 (ThemeRootCandidate, 1);
		char *index_theme;

		candidate->path = g_strdup (g_ptr_array_index (roots, i));
		candidate->depth = path_depth_from_root (input_root, candidate->path);
		index_theme = g_build_filename (candidate->path, "index.theme", NULL);
		candidate->has_index_theme = g_file_test (index_theme, G_FILE_TEST_IS_REGULAR);
		g_free (index_theme);
		g_ptr_array_add (candidates, candidate);
	}

	g_ptr_array_sort (candidates, theme_root_candidate_compare);
	selected = g_strdup (((ThemeRootCandidate *) g_ptr_array_index (candidates, 0))->path);
	g_ptr_array_unref (candidates);
	return selected;
}

static gboolean
copy_css_file (const char *src, const char *dest, GError **error)
{
	char *contents = NULL;
	char *normalized;
	gsize len = 0;
	GRegex *regex;

	if (!g_file_get_contents (src, &contents, &len, error))
		return FALSE;

	if (!g_strstr_len (contents, len, ":insensitive"))
	{
		gboolean ok = g_file_set_contents (dest, contents, len, error);
		g_free (contents);
		return ok;
	}

	regex = g_regex_new (":insensitive", 0, 0, error);
	if (!regex)
	{
		g_free (contents);
		return FALSE;
	}

	normalized = g_regex_replace_literal (regex, contents, -1, 0, ":disabled", 0, error);
	g_regex_unref (regex);
	if (!normalized)
	{
		g_free (contents);
		return FALSE;
	}

	if (!g_file_set_contents (dest, normalized, -1, error))
	{
		g_free (normalized);
		g_free (contents);
		return FALSE;
	}

	g_free (normalized);
	g_free (contents);
	return TRUE;
}

static gboolean
copy_tree (const char *src, const char *dest, GError **error)
{
	GDir *dir;
	const char *name;

	if (g_mkdir_with_parents (dest, 0700) != 0)
		return g_set_error_literal (error, G_FILE_ERROR, g_file_error_from_errno (errno), "Failed to create theme directory."), FALSE;

	dir = g_dir_open (src, 0, error);
	if (!dir)
		return FALSE;

	while ((name = g_dir_read_name (dir)) != NULL)
	{
		char *s = g_build_filename (src, name, NULL);
		char *d = g_build_filename (dest, name, NULL);
		if (g_file_test (s, G_FILE_TEST_IS_DIR))
		{
			if (!copy_tree (s, d, error))
			{
				g_free (s);
				g_free (d);
				g_dir_close (dir);
				return FALSE;
			}
		}
		else
		{
			if (g_str_has_suffix (name, ".css"))
			{
				if (!copy_css_file (s, d, error))
				{
					g_free (s);
					g_free (d);
					g_dir_close (dir);
					return FALSE;
				}
			}
			else
			{
				GFile *sf = g_file_new_for_path (s);
				GFile *df = g_file_new_for_path (d);
				if (!g_file_copy (sf, df, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error))
				{
					g_object_unref (sf);
					g_object_unref (df);
					g_free (s);
					g_free (d);
					g_dir_close (dir);
					return FALSE;
				}
				g_object_unref (sf);
				g_object_unref (df);
			}
		}
		g_free (s);
		g_free (d);
	}

	g_dir_close (dir);
	return TRUE;
}

static gboolean
validate_theme_root_for_import (const char *theme_root, GError **error)
{
	char *index_theme;
	GKeyFile *keyfile;
	char *css_dir;
	char *css_path;
	char *raw_inherits;
	char **inherits;
	guint i;
	GError *load_error = NULL;

	index_theme = g_build_filename (theme_root, "index.theme", NULL);
	if (!g_file_test (index_theme, G_FILE_TEST_IS_REGULAR))
	{
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
					 "Invalid GTK3 theme at '%s': missing required index.theme at '%s'.",
			   theme_root, index_theme);
		g_free (index_theme);
		return FALSE;
	}

	keyfile = g_key_file_new ();
	if (!g_key_file_load_from_file (keyfile, index_theme, G_KEY_FILE_NONE, &load_error))
	{
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
					 "Invalid GTK3 theme at '%s': failed to parse index.theme '%s': %s.",
			   theme_root, index_theme, load_error->message);
		g_error_free (load_error);
		g_key_file_unref (keyfile);
		g_free (index_theme);
		return FALSE;
	}

	if (!g_key_file_has_group (keyfile, "Desktop Entry"))
	{
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
					 "Invalid GTK3 theme at '%s': index.theme '%s' is missing the [Desktop Entry] section.",
			   theme_root, index_theme);
		g_key_file_unref (keyfile);
		g_free (index_theme);
		return FALSE;
	}

	css_dir = zoitechat_gtk3_theme_pick_css_dir (theme_root);
	if (!css_dir)
	{
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
					 "Invalid GTK3 theme at '%s': could not resolve a GTK CSS directory (expected gtk-3.x/gtk.css).",
					 theme_root);
		g_key_file_unref (keyfile);
		g_free (index_theme);
		return FALSE;
	}

	css_path = g_build_filename (theme_root, css_dir, "gtk.css", NULL);
	if (!g_file_test (css_path, G_FILE_TEST_IS_REGULAR))
	{
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
					 "Invalid GTK3 theme at '%s': missing primary gtk.css at '%s'.",
			   theme_root, css_path);
		g_free (css_path);
		g_free (css_dir);
		g_key_file_unref (keyfile);
		g_free (index_theme);
		return FALSE;
	}
	g_free (css_path);
	g_free (css_dir);

	raw_inherits = g_key_file_get_string (keyfile, "Desktop Entry", "Inherits", NULL);
	g_key_file_unref (keyfile);
	g_free (index_theme);
	if (!raw_inherits)
		return TRUE;

	inherits = g_strsplit_set (raw_inherits, ",;", -1);
	g_free (raw_inherits);

	for (i = 0; inherits && inherits[i]; i++)
	{
		char *parent_name = g_strstrip (inherits[i]);
		char *parent_root;

		if (parent_name[0] == '\0')
			continue;

		parent_root = resolve_parent_theme_root (theme_root, parent_name);
		if (!parent_root)
		{
			g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
						 "Invalid GTK3 theme at '%s': parent theme '%s' from Inherits could not be resolved.",
				theme_root, parent_name);
			g_strfreev (inherits);
			return FALSE;
		}
		g_free (parent_root);
	}

	g_strfreev (inherits);
	return TRUE;
}

static char *
extract_archive (const char *source, GError **error)
{
	char *tmp = g_dir_make_tmp ("zoitechat-gtk3-theme-XXXXXX", error);
	#ifdef G_OS_WIN32
	char *stdout_text = NULL;
	char *stderr_text = NULL;
	char *system_tar = NULL;
	char *system_root = NULL;
	char *tar_program = NULL;
	int status = 0;
	gboolean extracted = FALSE;
	const char *ext;

	if (!tmp)
		return NULL;

	ext = strrchr (source, '.');
	if (ext && g_ascii_strcasecmp (ext, ".zip") == 0)
	{
		char *argv[] = {
			"powershell",
			"-NoProfile",
			"-NonInteractive",
			"-ExecutionPolicy",
			"Bypass",
			"-Command",
			"Expand-Archive -LiteralPath $args[0] -DestinationPath $args[1] -Force",
			(char *)source,
			tmp,
			NULL
		};

		extracted = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
								  &stdout_text, &stderr_text, &status, NULL);
	}
	else
	{
		tar_program = g_find_program_in_path ("tar.exe");
		if (!tar_program)
		{
			system_root = g_strdup (g_getenv ("SystemRoot"));
			if (system_root)
			{
				system_tar = g_build_filename (system_root, "System32", "tar.exe", NULL);
				if (g_file_test (system_tar, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE))
					tar_program = g_strdup (system_tar);
			}
		}

		if (!tar_program)
		{
			char *argv[] = {
				"powershell",
				"-NoProfile",
				"-NonInteractive",
				"-ExecutionPolicy",
				"Bypass",
				"-Command",
				"tar -xf $args[0] -C $args[1]",
				(char *)source,
				tmp,
				NULL
			};

			extracted = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
									  &stdout_text, &stderr_text, &status, NULL);
		}
		else
		{
			char *argv[] = {
				tar_program,
				"-xf",
				(char *)source,
				"-C",
				tmp,
				NULL
			};

			extracted = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
									  &stdout_text, &stderr_text, &status, NULL);
		}
	}

	if (!extracted || (status != 0 && !path_tree_has_entries (tmp)))
	{
		g_free (tar_program);
		g_free (system_tar);
		g_free (system_root);
		g_free (stdout_text);
		g_free (stderr_text);
		remove_tree (tmp);
		g_free (tmp);
		g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to extract theme archive.");
		return NULL;
	}

	/*
	 * Windows archive tools often return a non-zero exit status for Unix-style
	 * symlink entries that cannot be materialized without extra privileges.
	 * If regular theme files were extracted, continue and let theme validation
	 * decide whether the imported theme is usable.
	 */
	g_free (tar_program);
	g_free (system_tar);
	g_free (system_root);
	g_free (stdout_text);
	g_free (stderr_text);

	return tmp;
	#else
	struct archive *archive = NULL;
	struct archive *disk = NULL;
	struct archive_entry *entry;
	int r;

	if (!tmp)
		return NULL;

	archive = archive_read_new ();
	disk = archive_write_disk_new ();
	archive_read_support_filter_all (archive);
	archive_read_support_format_all (archive);
	archive_write_disk_set_options (disk, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);
	archive_write_disk_set_standard_lookup (disk);

	r = archive_read_open_filename (archive, source, 10240);
	if (r != ARCHIVE_OK)
	{
		archive_read_free (archive);
		archive_write_free (disk);
		remove_tree (tmp);
		g_free (tmp);
		g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to extract theme archive.");
		return NULL;
	}

	while ((r = archive_read_next_header (archive, &entry)) == ARCHIVE_OK)
	{
		const char *entry_path = archive_entry_pathname (entry);
		char *dest;

		if (!entry_path)
		{
			r = ARCHIVE_FAILED;
			break;
		}

		dest = g_build_filename (tmp, entry_path, NULL);
		archive_entry_set_pathname (entry, dest);
		g_free (dest);

		r = archive_write_header (disk, entry);
		if (r < ARCHIVE_OK)
			break;

		if (archive_entry_size (entry) > 0)
		{
			const void *buff;
			size_t size;
			la_int64_t offset;

			for (;;)
			{
				r = archive_read_data_block (archive, &buff, &size, &offset);
				if (r == ARCHIVE_EOF)
					break;
				if (r != ARCHIVE_OK)
					break;
				r = archive_write_data_block (disk, buff, size, offset);
				if (r != ARCHIVE_OK)
					break;
			}
			if (r != ARCHIVE_EOF && r != ARCHIVE_OK)
				break;
		}

		r = archive_write_finish_entry (disk);
		if (r != ARCHIVE_OK)
			break;
	}

	if (r == ARCHIVE_EOF)
		r = ARCHIVE_OK;

	archive_read_free (archive);
	archive_write_free (disk);

	if (r != ARCHIVE_OK)
	{
		remove_tree (tmp);
		g_free (tmp);
		g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to extract theme archive.");
		return NULL;
	}

	return tmp;
	#endif
}

gboolean
zoitechat_gtk3_theme_service_import (const char *source_path, char **imported_id, GError **error)
{
	char *input_root = NULL;
	gboolean cleanup_input = FALSE;
	GPtrArray *roots;
	char *selected = NULL;
	char *base;
	char *dest_root;
	char *user_dir;
	char *candidate;
	int suffix = 0;
	gboolean ok;

	if (!source_path || !*source_path)
		return g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "No theme path provided."), FALSE;

	if (g_file_test (source_path, G_FILE_TEST_IS_DIR))
		input_root = g_strdup (source_path);
	else
	{
		input_root = extract_archive (source_path, error);
		cleanup_input = TRUE;
		if (!input_root)
			return FALSE;
	}

	roots = g_ptr_array_new_with_free_func (g_free);
	collect_theme_roots (input_root, roots, 0);
	if (roots->len == 0)
	{
		if (cleanup_input)
			remove_tree (input_root);
		g_free (input_root);
		g_ptr_array_unref (roots);
		return g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "No GTK3 gtk.css file found in the selected theme."), FALSE;
	}

	selected = select_theme_root (roots, input_root);
	if (!validate_theme_root_for_import (selected, error))
	{
		g_free (selected);
		g_ptr_array_unref (roots);
		if (cleanup_input)
			remove_tree (input_root);
		g_free (input_root);
		return FALSE;
	}

	base = g_path_get_basename (selected);
	user_dir = zoitechat_gtk3_theme_service_get_user_themes_dir ();
	g_mkdir_with_parents (user_dir, 0700);

	candidate = g_strdup (base);
	dest_root = g_build_filename (user_dir, candidate, NULL);
	while (g_file_test (dest_root, G_FILE_TEST_EXISTS))
	{
		suffix++;
		g_free (candidate);
		g_free (dest_root);
		candidate = g_strdup_printf ("%s-%d", base, suffix);
		dest_root = g_build_filename (user_dir, candidate, NULL);
	}

	ok = copy_tree (selected, dest_root, error);
	if (ok && imported_id)
		*imported_id = path_build_id (dest_root, ZOITECHAT_GTK3_THEME_SOURCE_USER);

	g_free (dest_root);
	g_free (candidate);
	g_free (user_dir);
	g_free (base);
	g_free (selected);
	g_ptr_array_unref (roots);
	if (cleanup_input)
		remove_tree (input_root);
	g_free (input_root);
	return ok;
}

gboolean
zoitechat_gtk3_theme_service_remove_user_theme (const char *theme_id, GError **error)
{
	ZoitechatGtk3Theme *theme = zoitechat_gtk3_theme_find_by_id (theme_id);
	gboolean ok;

	if (!theme)
		return g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "Theme not found."), FALSE;
	if (theme->source != ZOITECHAT_GTK3_THEME_SOURCE_USER)
	{
		zoitechat_gtk3_theme_free (theme);
		return g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_PERM, "Only user-imported themes can be removed."), FALSE;
	}

	remove_tree (theme->path);
	ok = !g_file_test (theme->path, G_FILE_TEST_EXISTS);
	zoitechat_gtk3_theme_free (theme);
	if (!ok)
		return g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to remove theme."), FALSE;
	return TRUE;
}
