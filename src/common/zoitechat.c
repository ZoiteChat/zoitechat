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
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define WANTSOCKET
#include "inet.h"

#ifdef WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#endif

#include "zoitechat.h"
#include "fe.h"
#include "util.h"
#include "cfgfiles.h"
#include "chanopt.h"
#include "ignore.h"
#include "zoitechat-plugin.h"
#include "inbound.h"
#include "plugin.h"
#include "plugin-identd.h"
#include "plugin-timer.h"
#include "notify.h"
#include "server.h"
#include "servlist.h"
#include "sts.h"
#include "outbound.h"
#include "text.h"
#include "url.h"
#include "zoitechatc.h"

#if ! GLIB_CHECK_VERSION (2, 36, 0)
#include <glib-object.h>			/* for g_type_init() */
#endif

GSList *popup_list = 0;
GSList *button_list = 0;
GSList *dlgbutton_list = 0;
GSList *command_list = 0;
GSList *ctcp_list = 0;
GSList *replace_list = 0;
GSList *sess_list = 0;
GSList *dcc_list = 0;
GSList *ignore_list = 0;
GSList *usermenu_list = 0;
GSList *urlhandler_list = 0;
GSList *tabmenu_list = 0;

/*
 * This array contains 5 double linked lists, one for each priority in the
 * "interesting session" queue ("channel" stands for everything but
 * SESS_DIALOG):
 *
 * [0] queries with hilight
 * [1] queries
 * [2] channels with hilight
 * [3] channels with dialogue
 * [4] channels with other data
 *
 * Each time activity happens the corresponding session is put at the
 * beginning of one of the lists.  The aim is to be able to switch to the
 * session with the most important/recent activity.
 */
GList *sess_list_by_lastact[5] = {NULL, NULL, NULL, NULL, NULL};


static int in_zoitechat_exit = FALSE;
int zoitechat_is_quitting = FALSE;
/* command-line args */
int arg_dont_autoconnect = FALSE;
int arg_skip_plugins = FALSE;
char *arg_url = NULL;
char **arg_urls = NULL;
char *arg_command = NULL;
gint arg_existing = FALSE;

#ifdef USE_DBUS
#include "dbus/dbus-client.h"
#include "dbus/dbus-plugin.h"
#endif /* USE_DBUS */

struct session *current_tab;
struct session *current_sess = 0;
struct zoitechatprefs prefs;

gboolean
zoitechat_theme_path_from_arg (const char *arg, char **path_out)
{
	char *path = NULL;
	gboolean valid_ext = FALSE;

	if (!arg)
		return FALSE;

	if (g_str_has_prefix (arg, "file://"))
		path = g_filename_from_uri (arg, NULL, NULL);
	else
		path = g_strdup (arg);

	if (!path)
		return FALSE;

	if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
	{
		char *path_lower = g_ascii_strdown (path, -1);

		valid_ext = g_str_has_suffix (path_lower, ".zip")
			|| g_str_has_suffix (path_lower, ".tar")
			|| g_str_has_suffix (path_lower, ".tar.gz")
			|| g_str_has_suffix (path_lower, ".tgz")
			|| g_str_has_suffix (path_lower, ".tar.xz")
			|| g_str_has_suffix (path_lower, ".txz");

		g_free (path_lower);
	}

	if (!valid_ext)
	{
		g_free (path);
		return FALSE;
	}

	if (path_out)
		*path_out = path;
	else
		g_free (path);

	return TRUE;
}

#ifdef WIN32
static gboolean
zoitechat_has_theme_argument (void)
{
	char *theme_path = NULL;
	guint i;

	if (arg_url && zoitechat_theme_path_from_arg (arg_url, &theme_path))
	{
		g_free (theme_path);
		return TRUE;
	}

	if (arg_urls)
	{
		for (i = 0; i < g_strv_length (arg_urls); i++)
		{
			if (zoitechat_theme_path_from_arg (arg_urls[i], &theme_path))
			{
				g_free (theme_path);
				return TRUE;
			}
		}
	}

	return FALSE;
}

static HWND
zoitechat_find_running_window (void)
{
	HWND hwnd = FindWindowA ("ZoiteChat", NULL);

	if (!hwnd)
		hwnd = FindWindowA ("zoitechat", NULL);
	if (!hwnd)
		hwnd = FindWindowA (NULL, "ZoiteChat");

	return hwnd;
}

static gboolean
zoitechat_send_command_to_existing (HWND hwnd, const char *command)
{
	COPYDATASTRUCT copy_data;
	DWORD_PTR send_result = 0;

	if (!hwnd || !command || !*command)
		return FALSE;

	copy_data.dwData = 0;
	copy_data.cbData = (DWORD)strlen (command) + 1;
	copy_data.lpData = (void *)command;

	return SendMessageTimeoutA (hwnd, WM_COPYDATA, (WPARAM)NULL,
	                            (LPARAM)&copy_data,
	                            SMTO_ABORTIFHUNG | SMTO_BLOCK,
	                            5000, &send_result) != 0;
}

static gboolean
zoitechat_remote_win32 (void)
{
	HWND hwnd;
	gboolean allow_remote;
	gboolean sent = FALSE;

	allow_remote = arg_existing || zoitechat_has_theme_argument ();
	if (!allow_remote)
		return FALSE;

	hwnd = zoitechat_find_running_window ();
	if (!hwnd)
		return FALSE;

	if (arg_url)
	{
		char *command = g_strdup_printf ("url %s", arg_url);
		sent = zoitechat_send_command_to_existing (hwnd, command) || sent;
		g_free (command);
	}
	else if (arg_command)
	{
		sent = zoitechat_send_command_to_existing (hwnd, arg_command) || sent;
	}
	else if (arg_existing)
	{
		sent = zoitechat_send_command_to_existing (hwnd, "__WIN32_TASKBAR_TOGGLE__") || sent;
	}

	if (arg_urls)
	{
		guint i;
		for (i = 0; i < g_strv_length (arg_urls); i++)
		{
			char *command = g_strdup_printf ("url %s", arg_urls[i]);
			sent = zoitechat_send_command_to_existing (hwnd, command) || sent;
			g_free (command);
		}
		g_strfreev (arg_urls);
		arg_urls = NULL;
	}

	return sent;
}
#endif


static gboolean
zoitechat_is_safe_archive_entry (const char *entry)
{
	char **parts;
	gboolean safe = TRUE;
	guint i;

	if (!entry || !*entry)
		return FALSE;

	if (g_path_is_absolute (entry) || entry[0] == '/' || entry[0] == '\\')
		return FALSE;

	if (g_ascii_isalpha (entry[0]) && entry[1] == ':')
		return FALSE;

	parts = g_strsplit_set (entry, "/\\", -1);
	for (i = 0; parts[i] != NULL; i++)
	{
		if (strcmp (parts[i], "..") == 0)
		{
			safe = FALSE;
			break;
		}
	}

	g_strfreev (parts);
	return safe;
}

static gboolean
zoitechat_remove_tree (const char *path, GError **error)
{
	GDir *dir;
	const char *name;

	if (!g_file_test (path, G_FILE_TEST_EXISTS))
		return TRUE;

	if (!g_file_test (path, G_FILE_TEST_IS_DIR))
		return g_remove (path) == 0;

	dir = g_dir_open (path, 0, error);
	if (!dir)
		return FALSE;

	while ((name = g_dir_read_name (dir)))
	{
		char *child = g_build_filename (path, name, NULL);

		if (!zoitechat_remove_tree (child, error))
		{
			g_free (child);
			g_dir_close (dir);
			return FALSE;
		}

		g_free (child);
	}

	g_dir_close (dir);
	if (g_rmdir (path) != 0)
	{
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
		             _("Failed to remove temporary directory."));
		return FALSE;
	}

	return TRUE;
}

static gboolean
zoitechat_copy_directory_recursive (const char *src_dir, const char *dest_dir, GError **error)
{
	GDir *dir;
	const char *name;

	if (g_mkdir_with_parents (dest_dir, 0700) != 0)
	{
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
		             _("Failed to create destination theme directory."));
		return FALSE;
	}

	dir = g_dir_open (src_dir, 0, error);
	if (!dir)
		return FALSE;

	while ((name = g_dir_read_name (dir)))
	{
		char *src_path = g_build_filename (src_dir, name, NULL);
		char *dest_path = g_build_filename (dest_dir, name, NULL);

		if (g_file_test (src_path, G_FILE_TEST_IS_DIR))
		{
			if (!zoitechat_copy_directory_recursive (src_path, dest_path, error))
			{
				g_free (dest_path);
				g_free (src_path);
				g_dir_close (dir);
				return FALSE;
			}
		}
		else if (g_file_test (src_path, G_FILE_TEST_IS_REGULAR))
		{
			GFile *src_file = g_file_new_for_path (src_path);
			GFile *dest_file = g_file_new_for_path (dest_path);
			gboolean copied = g_file_copy (src_file, dest_file,
			                               G_FILE_COPY_OVERWRITE,
			                               NULL, NULL, NULL, error);

			g_object_unref (dest_file);
			g_object_unref (src_file);

			if (!copied)
			{
				g_free (dest_path);
				g_free (src_path);
				g_dir_close (dir);
				return FALSE;
			}
		}

		g_free (dest_path);
		g_free (src_path);
	}

	g_dir_close (dir);
	return TRUE;
}

static gboolean
zoitechat_find_gtk3_theme_root (const char *search_dir,
	                            char **theme_root_out,
	                            gboolean *has_dark_css_out,
	                            gboolean *missing_gtk_css_out,
	                            GError **error)
{
	GDir *dir;
	const char *name;

	dir = g_dir_open (search_dir, 0, error);
	if (!dir)
		return FALSE;

	while ((name = g_dir_read_name (dir)))
	{
		char *child = g_build_filename (search_dir, name, NULL);

		if (g_file_test (child, G_FILE_TEST_IS_DIR))
		{
			if (g_str_has_prefix (name, "gtk-3."))
			{
				char *gtk_css = g_build_filename (child, "gtk.css", NULL);

				if (g_file_test (gtk_css, G_FILE_TEST_IS_REGULAR))
				{
					char *dark_css = g_build_filename (child, "gtk-dark.css", NULL);

					if (theme_root_out)
						*theme_root_out = g_strdup (search_dir);
					if (has_dark_css_out)
						*has_dark_css_out = g_file_test (dark_css, G_FILE_TEST_IS_REGULAR);

					g_free (dark_css);
					g_free (gtk_css);
					g_free (child);
					g_dir_close (dir);
					return TRUE;
				}

				g_free (gtk_css);
				if (missing_gtk_css_out)
					*missing_gtk_css_out = TRUE;
			}
			else if (zoitechat_find_gtk3_theme_root (child,
			                                       theme_root_out,
			                                       has_dark_css_out,
			                                       missing_gtk_css_out,
			                                       error))
			{
				g_free (child);
				g_dir_close (dir);
				return TRUE;
			}

			if (error && *error)
			{
				g_free (child);
				g_dir_close (dir);
				return FALSE;
			}
		}

		g_free (child);
	}

	g_dir_close (dir);
	return FALSE;
}


typedef enum
{
	ZOITECHAT_GTK3_ARCHIVE_UNKNOWN = 0,
	ZOITECHAT_GTK3_ARCHIVE_ZIP,
	ZOITECHAT_GTK3_ARCHIVE_TAR
} ZoiteChatGtk3ArchiveType;

static ZoiteChatGtk3ArchiveType
zoitechat_detect_gtk3_archive_type (const char *archive_path)
{
	if (!archive_path)
		return ZOITECHAT_GTK3_ARCHIVE_UNKNOWN;

	if (g_str_has_suffix (archive_path, ".zip") || g_str_has_suffix (archive_path, ".ZIP"))
		return ZOITECHAT_GTK3_ARCHIVE_ZIP;

	if (g_str_has_suffix (archive_path, ".tar")
	 || g_str_has_suffix (archive_path, ".tar.gz")
	 || g_str_has_suffix (archive_path, ".tgz")
	 || g_str_has_suffix (archive_path, ".tar.xz")
	 || g_str_has_suffix (archive_path, ".txz"))
		return ZOITECHAT_GTK3_ARCHIVE_TAR;

	return ZOITECHAT_GTK3_ARCHIVE_UNKNOWN;
}

#ifndef WIN32
static gboolean
zoitechat_validate_zip_entries_unix (const char *archive_path, char **archive_root_out, GError **error)
{
	char *stdout_buf = NULL;
	char *stderr_buf = NULL;
	char *argv[] = {"unzip", "-Z1", (char *)archive_path, NULL};
	char *archive_root = NULL;
	char **lines;
	gboolean ok;
	int status = 0;
	guint i;

	ok = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
	                   NULL, NULL, &stdout_buf, &stderr_buf, &status, error);
	if (!ok)
		goto cleanup;

	if (!g_spawn_check_exit_status (status, error))
	{
		ok = FALSE;
		goto cleanup;
	}

	lines = g_strsplit (stdout_buf ? stdout_buf : "", "\n", -1);
	for (i = 0; lines[i] != NULL; i++)
	{
		const char *entry = lines[i];
		char **parts;

		if (!entry[0])
			continue;

		if (!zoitechat_is_safe_archive_entry (entry))
		{
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			             _("Archive contains unsafe path: %s"), entry);
			ok = FALSE;
			break;
		}

		parts = g_strsplit (entry, "/", 2);
		if (parts[0] && *parts[0])
		{
			if (!archive_root)
				archive_root = g_strdup (parts[0]);
			else if (strcmp (archive_root, parts[0]) != 0)
				g_clear_pointer (&archive_root, g_free);
		}
		g_strfreev (parts);
	}
	g_strfreev (lines);

	if (ok && archive_root_out)
		*archive_root_out = g_strdup (archive_root);

cleanup:
	g_free (archive_root);
	g_free (stderr_buf);
	g_free (stdout_buf);
	return ok;
}

#endif

static gboolean
zoitechat_validate_tar_entries_unix (const char *archive_path, char **archive_root_out, GError **error)
{
	char *stdout_buf = NULL;
	char *stderr_buf = NULL;
	char *argv[] = {"tar", "-tf", (char *)archive_path, NULL};
	char *archive_root = NULL;
	char **lines;
	gboolean ok;
	int status = 0;
	guint i;

	ok = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
	                   NULL, NULL, &stdout_buf, &stderr_buf, &status, error);
	if (!ok)
		goto cleanup;

	if (!g_spawn_check_exit_status (status, error))
	{
		ok = FALSE;
		goto cleanup;
	}

	lines = g_strsplit (stdout_buf ? stdout_buf : "", "\n", -1);
	for (i = 0; lines[i] != NULL; i++)
	{
		const char *entry = lines[i];
		char **parts;

		if (!entry[0])
			continue;

		if (!zoitechat_is_safe_archive_entry (entry))
		{
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			             _("Archive contains unsafe path: %s"), entry);
			ok = FALSE;
			break;
		}

		parts = g_strsplit_set (entry, "/\\", 2);
		if (parts[0] && *parts[0])
		{
			if (!archive_root)
				archive_root = g_strdup (parts[0]);
			else if (strcmp (archive_root, parts[0]) != 0)
				g_clear_pointer (&archive_root, g_free);
		}
		g_strfreev (parts);
	}
	g_strfreev (lines);

	if (ok && archive_root_out)
		*archive_root_out = g_strdup (archive_root);

cleanup:
	g_free (archive_root);
	g_free (stderr_buf);
	g_free (stdout_buf);
	return ok;
}

gboolean
zoitechat_import_gtk3_theme_archive (const char *archive_path,
                                  char **theme_name_out,
                                  GError **error)
{
	ZoiteChatGtk3ArchiveType archive_type;
	char *temp_dir = NULL;
	char *archive_root = NULL;
	char *theme_root = NULL;
	char *theme_name = NULL;
	char *store_dir = NULL;
	char *dest_dir = NULL;
	int status = 0;
	gboolean ok = FALSE;
	gboolean has_dark_css = FALSE;
	gboolean missing_gtk_css = FALSE;

	if (theme_name_out)
		*theme_name_out = NULL;

	if (!archive_path)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		             _("No GTK3 theme archive specified."));
		return FALSE;
	}

	archive_type = zoitechat_detect_gtk3_archive_type (archive_path);
	if (archive_type == ZOITECHAT_GTK3_ARCHIVE_UNKNOWN)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		             _("Unsupported archive format. Use .zip, .tar, .tar.gz, .tgz, .tar.xz, or .txz."));
		return FALSE;
	}

	{
		char *basename = g_path_get_basename (archive_path);
		char *dot;

		if (basename && *basename)
		{
			dot = strrchr (basename, '.');
			if (dot)
				*dot = '\0';
			if (g_str_has_suffix (basename, ".tar"))
				basename[strlen (basename) - 4] = '\0';
			if (*basename)
				archive_root = g_strdup (basename);
		}
		g_free (basename);
	}

	temp_dir = g_dir_make_tmp ("zoitechat-gtk3-theme-XXXXXX", error);
	if (!temp_dir)
		return FALSE;

#ifdef WIN32
	if (archive_type == ZOITECHAT_GTK3_ARCHIVE_ZIP)
	{
		char *powershell = NULL;
		char *command = NULL;
		GString *escaped_path = g_string_new ("'");
		GString *escaped_dir = g_string_new ("'");
		const char *cursor;

		powershell = g_find_program_in_path ("powershell.exe");
		if (!powershell)
			powershell = g_find_program_in_path ("powershell");
		if (!powershell)
		{
			g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT,
			             _("No archive extractor was found."));
			g_string_free (escaped_path, TRUE);
			g_string_free (escaped_dir, TRUE);
			goto cleanup;
		}

		for (cursor = archive_path; *cursor != '\0'; cursor++)
		{
			if (*cursor == '\'')
				g_string_append (escaped_path, "''");
			else
				g_string_append_c (escaped_path, *cursor);
		}
		g_string_append_c (escaped_path, '\'');

		for (cursor = temp_dir; *cursor != '\0'; cursor++)
		{
			if (*cursor == '\'')
				g_string_append (escaped_dir, "''");
			else
				g_string_append_c (escaped_dir, *cursor);
		}
		g_string_append_c (escaped_dir, '\'');

		command = g_strdup_printf (
			"Add-Type -AssemblyName System.IO.Compression.FileSystem; "
			"$ErrorActionPreference='Stop'; "
			"$archive=[System.IO.Compression.ZipFile]::OpenRead(%s); "
			"try { "
			"foreach ($entry in $archive.Entries) { "
			"$name=$entry.FullName; "
			"if ([string]::IsNullOrEmpty($name)) { continue }; "
			"if ([System.IO.Path]::IsPathRooted($name) -or $name.StartsWith('/') -or $name.StartsWith('\\') -or $name.Contains('..\\') -or $name.Contains('../')) { throw 'Archive contains unsafe path: ' + $name } "
			"}; "
			"[System.IO.Compression.ZipFile]::ExtractToDirectory(%s,%s,$true); "
			"} finally { $archive.Dispose(); }",
			escaped_path->str,
			escaped_path->str,
			escaped_dir->str);
		g_string_free (escaped_path, TRUE);
		g_string_free (escaped_dir, TRUE);

		{
			char *ps_argv[] = {powershell, "-NoProfile", "-NonInteractive", "-Command", command, NULL};
			ok = g_spawn_sync (NULL, ps_argv, NULL, 0, NULL, NULL,
			                   NULL, NULL, &status, error);
		}
		if (ok)
			ok = g_spawn_check_exit_status (status, error);

		g_free (command);
		g_free (powershell);
		if (!ok)
			goto cleanup;
	}
	else
	{
		char *argv[] = {"tar", "-xf", (char *)archive_path, "-C", temp_dir, NULL};
		char *extracted_root = NULL;

		if (!zoitechat_validate_tar_entries_unix (archive_path, &extracted_root, error))
			goto cleanup;
		if (extracted_root)
		{
			g_free (archive_root);
			archive_root = extracted_root;
		}

		ok = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
		                   NULL, NULL, &status, error);
		if (!ok)
			goto cleanup;
		if (!g_spawn_check_exit_status (status, error))
			goto cleanup;
	}
#else
	if (archive_type == ZOITECHAT_GTK3_ARCHIVE_ZIP)
	{
		char *argv[] = {"unzip", "-o", (char *)archive_path, "-d", temp_dir, NULL};
		char *extracted_root = NULL;

		if (!zoitechat_validate_zip_entries_unix (archive_path, &extracted_root, error))
			goto cleanup;
		if (extracted_root)
		{
			g_free (archive_root);
			archive_root = extracted_root;
		}

		ok = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
		                   NULL, NULL, &status, error);
		if (!ok)
			goto cleanup;
		if (!g_spawn_check_exit_status (status, error))
			goto cleanup;
	}
	else
	{
		char *argv[] = {"tar", "-xf", (char *)archive_path, "-C", temp_dir, NULL};
		char *extracted_root = NULL;

		if (!zoitechat_validate_tar_entries_unix (archive_path, &extracted_root, error))
			goto cleanup;
		if (extracted_root)
		{
			g_free (archive_root);
			archive_root = extracted_root;
		}

		ok = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
		                   NULL, NULL, &status, error);
		if (!ok)
			goto cleanup;
		if (!g_spawn_check_exit_status (status, error))
			goto cleanup;
	}
#endif

	if (!zoitechat_find_gtk3_theme_root (temp_dir, &theme_root, &has_dark_css, &missing_gtk_css, error))
	{
		if (error && *error)
			goto cleanup;

		if (missing_gtk_css)
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			             _("Archive contains a gtk-3.x directory, but gtk.css is missing."));
		else
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			             _("Archive is not a GTK3 theme. Expected layout: <theme-name>/gtk-3.x/gtk.css."));
		goto cleanup;
	}

	theme_name = g_path_get_basename (theme_root);
	if (!theme_name || !*theme_name)
	{
		g_clear_pointer (&theme_name, g_free);
		if (archive_root)
			theme_name = g_strdup (archive_root);
	}

	if (!theme_name || !*theme_name)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
		             _("Unable to determine GTK3 theme directory name from archive."));
		goto cleanup;
	}

	store_dir = g_build_filename (get_xdir (), "gtk3-themes", NULL);
	dest_dir = g_build_filename (store_dir, theme_name, NULL);

	if (g_file_test (dest_dir, G_FILE_TEST_EXISTS) && !zoitechat_remove_tree (dest_dir, error))
		goto cleanup;

	if (!zoitechat_copy_directory_recursive (theme_root, dest_dir, error))
		goto cleanup;

	if (has_dark_css)
		fe_message (_("Imported GTK3 theme includes gtk-dark.css."), FE_MSG_INFO);

	if (theme_name_out)
		*theme_name_out = g_strdup (theme_name);

	ok = TRUE;

cleanup:
	if (temp_dir)
	{
		GError *cleanup_error = NULL;
		if (!zoitechat_remove_tree (temp_dir, &cleanup_error))
			g_clear_error (&cleanup_error);
	}
	g_free (dest_dir);
	g_free (store_dir);
	g_free (theme_name);
	g_free (theme_root);
	g_free (archive_root);
	g_free (temp_dir);
	return ok;
}



/*
 * Update the priority queue of the "interesting sessions"
 * (sess_list_by_lastact).
 */
void
lastact_update(session *sess)
{
	int oldidx = sess->lastact_idx;
	int newidx = LACT_NONE;
	int dia = (sess->type == SESS_DIALOG);

	if (sess->tab_state & TAB_STATE_NEW_HILIGHT)
		newidx = dia? LACT_QUERY_HI: LACT_CHAN_HI;
	else if (sess->tab_state & TAB_STATE_NEW_MSG)
		newidx = dia? LACT_QUERY: LACT_CHAN;
	else if (sess->tab_state & TAB_STATE_NEW_DATA)
		newidx = dia? LACT_QUERY: LACT_CHAN_DATA;

	if (oldidx == newidx &&
		 (newidx == LACT_NONE || g_list_index(sess_list_by_lastact[newidx], sess) == 0))
		return;

	if (oldidx != LACT_NONE)
		sess_list_by_lastact[oldidx] = g_list_remove(sess_list_by_lastact[oldidx], sess);

	sess->lastact_idx = newidx;
	if (newidx != LACT_NONE)
		sess_list_by_lastact[newidx] = g_list_prepend(sess_list_by_lastact[newidx], sess);
	return;
}

/*
 * Extract the first session from the priority queue of sessions with recent
 * activity. Return NULL if no such session can be found.
 *
 * If filter is specified, skip a session if filter(session) returns 0. This
 * can be used for UI-specific needs, e.g. in fe-gtk we want to filter out
 * detached sessions.
 */
session *
lastact_getfirst(int (*filter) (session *sess))
{
	int i;
	session *sess = NULL;
	GList *curitem;

	for (i = 0; i < 5 && !sess; i++)
	{
		curitem = sess_list_by_lastact[i];
		while (curitem && !sess)
		{
			sess = g_list_nth_data(curitem, 0);
			if (!sess || (filter && !filter(sess)))
			{
				sess = NULL;
				curitem = g_list_next(curitem);
			}
		}

		if (sess)
		{
			sess_list_by_lastact[i] = g_list_remove(sess_list_by_lastact[i], sess);
			sess->lastact_idx = LACT_NONE;
		}
	}
	
	return sess;
}

int
is_session (session * sess)
{
	return g_slist_find (sess_list, sess) ? 1 : 0;
}

session *
find_dialog (server *serv, char *nick)
{
	GSList *list = sess_list;
	session *sess;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv && sess->type == SESS_DIALOG)
		{
			if (!serv->p_cmp (nick, sess->channel))
				return (sess);
		}
		list = list->next;
	}
	return NULL;
}

session *
find_channel (server *serv, char *chan)
{
	session *sess;
	GSList *list = sess_list;
	while (list)
	{
		sess = list->data;
		if ((serv == sess->server) && sess->type == SESS_CHANNEL)
		{
			if (!serv->p_cmp (chan, sess->channel))
				return sess;
		}
		list = list->next;
	}
	return NULL;
}

static void
lagcheck_update (void)
{
	server *serv;
	GSList *list = serv_list;
	
	if (!prefs.hex_gui_lagometer)
		return;

	while (list)
	{
		serv = list->data;
		if (serv->lag_sent)
			fe_set_lag (serv, -1);

		list = list->next;
	}
}

void
lag_check (void)
{
	server *serv;
	GSList *list = serv_list;
	unsigned long tim;
	char tbuf[128];
	time_t now = time (0);
	time_t lag;

	tim = make_ping_time ();

	while (list)
	{
		serv = list->data;
		if (serv->connected && serv->end_of_motd)
		{
			lag = now - serv->ping_recv;
			if (prefs.hex_net_ping_timeout != 0 && lag > prefs.hex_net_ping_timeout && lag > 0)
			{
				sprintf (tbuf, "%" G_GINT64_FORMAT, (gint64) lag);
				EMIT_SIGNAL (XP_TE_PINGTIMEOUT, serv->server_session, tbuf, NULL,
								 NULL, NULL, 0);
				if (prefs.hex_net_auto_reconnect)
					serv->auto_reconnect (serv, FALSE, -1);
			}
			else
			{
				g_snprintf (tbuf, sizeof (tbuf), "LAG%lu", tim);
				serv->p_ping (serv, "", tbuf);
				
				if (!serv->lag_sent)
				{
					serv->lag_sent = tim;
					fe_set_lag (serv, -1);
				}
			}
		}
		list = list->next;
	}
}

static int
away_check (void)
{
	session *sess;
	GSList *list;
	int full, sent, loop = 0;

	if (!prefs.hex_away_track)
		return 1;

doover:
	full = TRUE;
	sent = 0;
	list = sess_list;
	while (list)
	{
		sess = list->data;

		if (sess->server->connected &&
			 sess->type == SESS_CHANNEL &&
			 sess->channel[0] &&
			 (sess->total <= prefs.hex_away_size_max || !prefs.hex_away_size_max))
		{
			if (!sess->done_away_check)
			{
				full = FALSE;

				if (sent < 31 && !sess->doing_who)
				{
					sess->done_away_check = TRUE;
					sess->doing_who = TRUE;
					sess->server->p_away_status (sess->server, sess->channel);
					sent += sess->total;
				}
			}
		}

		list = list->next;
	}

	if (full)
	{
		list = sess_list;
		while (list)
		{
			sess = list->data;
			if (!sess->server->have_awaynotify)
				sess->done_away_check = FALSE;
			list = list->next;
		}
		loop++;
		if (loop < 2)
			goto doover;
	}

	return 1;
}

static int
zoitechat_lag_check (void)
{
	lag_check ();
	return 1;
}

static int
zoitechat_lag_check_update (void)
{
	lagcheck_update ();
	return 1;
}

void
zoitechat_reinit_timers (void)
{
	static int lag_check_update_tag = 0;
	static int lag_check_tag = 0;
	static int away_tag = 0;

	if (prefs.hex_notify_timeout && notify_tag == 0)
	{
		notify_tag = fe_timeout_add_seconds (prefs.hex_notify_timeout,
						     notify_checklist, NULL);
	}
	else if (!prefs.hex_notify_timeout && notify_tag != 0)
	{
		fe_timeout_remove (notify_tag);
		notify_tag = 0;
	}

	if (prefs.hex_away_track && away_tag == 0)
	{
		away_tag = fe_timeout_add_seconds (prefs.hex_away_timeout, away_check, NULL);
	}
	else if (!prefs.hex_away_track && away_tag != 0)
	{
		fe_timeout_remove (away_tag);
		away_tag = 0;
	}

	if (prefs.hex_gui_lagometer && lag_check_update_tag == 0)
	{
		lag_check_update_tag = fe_timeout_add (500, zoitechat_lag_check_update, NULL);
	}
	else if (!prefs.hex_gui_lagometer && lag_check_update_tag != 0)
	{
		fe_timeout_remove (lag_check_update_tag);
		lag_check_update_tag = 0;
	}

	if ((prefs.hex_net_ping_timeout != 0 || prefs.hex_gui_lagometer)
	    && lag_check_tag == 0)
	{
		lag_check_tag = fe_timeout_add_seconds (30, zoitechat_lag_check, NULL);
	}
	else if ((!prefs.hex_net_ping_timeout && !prefs.hex_gui_lagometer)
					 && lag_check_tag != 0)
	{
		fe_timeout_remove (lag_check_tag);
		lag_check_tag = 0;
	}
}

static void
irc_init (session *sess)
{
	static int done_init = FALSE;
	char *buf;
	char *theme_path;

	if (done_init)
		return;

	done_init = TRUE;

	plugin_add (sess, NULL, NULL, timer_plugin_init, NULL, NULL, FALSE);
	plugin_add (sess, NULL, NULL, identd_plugin_init, identd_plugin_deinit, NULL, FALSE);

#ifdef USE_PLUGIN
	if (!arg_skip_plugins)
		plugin_auto_load (sess);	/* autoload ~/.xchat *.so */
#endif

#ifdef USE_DBUS
	plugin_add (sess, NULL, NULL, dbus_plugin_init, NULL, NULL, FALSE);
#endif

	zoitechat_reinit_timers ();

	if (arg_url != NULL)
	{
		theme_path = NULL;
		if (zoitechat_theme_path_from_arg (arg_url, &theme_path))
		{
			GError *error = NULL;
			char *theme_name = NULL;

			if (zoitechat_import_gtk3_theme_archive (theme_path, &theme_name, &error))
			{
				if (theme_name)
				{
					char *message = g_strdup_printf (_("GTK3 theme \"%s\" imported. Use Theme settings to apply it."), theme_name);
					fe_message (message, FE_MSG_INFO);
					g_free (message);
				}
				else
				{
					fe_message (_("GTK3 theme imported. Use Theme settings to apply it."), FE_MSG_INFO);
				}
			}
			else
			{
				fe_message (error ? error->message : _("Failed to import GTK3 theme archive."),
				            FE_MSG_ERROR);
				g_clear_error (&error);
			}

			g_free (theme_name);
		}
		else
		{
			buf = g_strdup_printf ("server %s", arg_url);
			handle_command (sess, buf, FALSE);
			g_free (buf);
		}

		g_free (theme_path);
		g_free (arg_url);	/* from GOption */
	}
	
	if (arg_urls != NULL)
	{
		guint i;
		for (i = 0; i < g_strv_length (arg_urls); i++)
		{
			theme_path = NULL;
			if (zoitechat_theme_path_from_arg (arg_urls[i], &theme_path))
			{
				GError *error = NULL;
				char *theme_name = NULL;

				if (zoitechat_import_gtk3_theme_archive (theme_path, &theme_name, &error))
				{
					if (theme_name)
					{
						char *message = g_strdup_printf (_("GTK3 theme \"%s\" imported. Use Theme settings to apply it."), theme_name);
						fe_message (message, FE_MSG_INFO);
						g_free (message);
					}
					else
					{
						fe_message (_("GTK3 theme imported. Use Theme settings to apply it."), FE_MSG_INFO);
					}
				}
				else
				{
					fe_message (error ? error->message : _("Failed to import GTK3 theme archive."),
					            FE_MSG_ERROR);
					g_clear_error (&error);
				}

				g_free (theme_name);
			}
			else
			{
				buf = g_strdup_printf ("%s %s", i==0? "server" : "newserver", arg_urls[i]);
				handle_command (sess, buf, FALSE);
				g_free (buf);
			}

			g_free (theme_path);
		}
		g_strfreev (arg_urls);
	}

	if (arg_command != NULL)
	{
		handle_command (sess, arg_command, FALSE);
		g_free (arg_command);
	}

	/* load -e <xdir>/startup.txt */
	load_perform_file (sess, "startup.txt");
}

static session *
session_new (server *serv, char *from, int type, int focus)
{
	session *sess;

	sess = g_new0 (struct session, 1);

	sess->server = serv;
	sess->logfd = -1;
	sess->type = type;

	sess->alert_balloon = SET_DEFAULT;
	sess->alert_beep = SET_DEFAULT;
	sess->alert_taskbar = SET_DEFAULT;
	sess->alert_tray = SET_DEFAULT;

	sess->text_hidejoinpart = SET_DEFAULT;
	sess->text_logging = SET_DEFAULT;
	sess->text_scrollback = SET_DEFAULT;
	sess->text_strip = SET_DEFAULT;

	sess->lastact_idx = LACT_NONE;

	if (from != NULL)
	{
		safe_strcpy(sess->channel, from, CHANLEN);
		safe_strcpy(sess->session_name, from, CHANLEN);
	}

	sess_list = g_slist_prepend (sess_list, sess);

	fe_new_window (sess, focus);

	return sess;
}

session *
new_ircwindow (server *serv, char *name, int type, int focus)
{
	session *sess;

	switch (type)
	{
	case SESS_SERVER:
		serv = server_new ();
		if (prefs.hex_gui_tab_server)
			sess = session_new (serv, name, SESS_SERVER, focus);
		else
			sess = session_new (serv, name, SESS_CHANNEL, focus);
		serv->server_session = sess;
		serv->front_session = sess;
		break;
	case SESS_DIALOG:
		sess = session_new (serv, name, type, focus);
		break;
	default:
/*	case SESS_CHANNEL:
	case SESS_NOTICES:
	case SESS_SNOTICES:*/
		sess = session_new (serv, name, type, focus);
		break;
	}

	irc_init (sess);
	chanopt_load (sess);
	scrollback_load (sess);
	if (sess->scrollwritten && sess->scrollback_replay_marklast)
		sess->scrollback_replay_marklast (sess);
	if (type == SESS_DIALOG)
	{
		struct User *user;

		log_open_or_close (sess);

		user = userlist_find_global (serv, name);
		if (user && user->hostname)
			set_topic (sess, user->hostname, user->hostname);
	}
	plugin_emit_dummy_print (sess, "Open Context");

	return sess;
}

static void
exec_notify_kill (session * sess)
{
#ifndef WIN32
	struct nbexec *re;
	if (sess->running_exec != NULL)
	{
		re = sess->running_exec;
		sess->running_exec = NULL;
		kill (re->childpid, SIGKILL);
		waitpid (re->childpid, NULL, WNOHANG);
		fe_input_remove (re->iotag);
		close (re->myfd);
		g_free (re->linebuf);
		g_free (re);
	}
#endif
}

static void
send_quit_or_part (session * killsess)
{
	int willquit = TRUE;
	GSList *list;
	session *sess;
	server *killserv = killsess->server;

	/* check if this is the last session using this server */
	list = sess_list;
	while (list)
	{
		sess = (session *) list->data;
		if (sess->server == killserv && sess != killsess)
		{
			willquit = FALSE;
			list = 0;
		} else
			list = list->next;
	}

	if (zoitechat_is_quitting)
		willquit = TRUE;

	if (killserv->connected)
	{
		if (willquit)
		{
			if (!killserv->sent_quit)
			{
				killserv->flush_queue (killserv);
				server_sendquit (killsess);
				killserv->sent_quit = TRUE;
			}
		} else
		{
			if (killsess->type == SESS_CHANNEL && killsess->channel[0] &&
				 !killserv->sent_quit)
			{
				server_sendpart (killserv, killsess->channel, 0);
			}
		}
	}
}

void
session_free (session *killsess)
{
	server *killserv = killsess->server;
	session *sess;
	GSList *list;
	int oldidx;

	plugin_emit_dummy_print (killsess, "Close Context");

	if (current_tab == killsess)
		current_tab = NULL;

	if (killserv->server_session == killsess)
		killserv->server_session = NULL;

	if (killserv->front_session == killsess)
	{
		/* front_session is closed, find a valid replacement */
		killserv->front_session = NULL;
		list = sess_list;
		while (list)
		{
			sess = (session *) list->data;
			if (sess != killsess && sess->server == killserv)
			{
				killserv->front_session = sess;
				if (!killserv->server_session)
					killserv->server_session = sess;
				break;
			}
			list = list->next;
		}
	}

	if (!killserv->server_session)
		killserv->server_session = killserv->front_session;

	sess_list = g_slist_remove (sess_list, killsess);

	if (killsess->type == SESS_CHANNEL)
		userlist_free (killsess);

	oldidx = killsess->lastact_idx;
	if (oldidx != LACT_NONE)
		sess_list_by_lastact[oldidx] = g_list_remove(sess_list_by_lastact[oldidx], killsess);

	exec_notify_kill (killsess);

	log_close (killsess);
	scrollback_close (killsess);
	chanopt_save (killsess);

	send_quit_or_part (killsess);

	history_free (&killsess->history);
	g_free (killsess->topic);
	g_free (killsess->current_modes);

	fe_session_callback (killsess);

	if (current_sess == killsess)
	{
		current_sess = NULL;
		if (sess_list)
			current_sess = sess_list->data;
	}

	g_free (killsess);

	if (!sess_list && !in_zoitechat_exit)
		zoitechat_exit ();						/* sess_list is empty, quit! */

	list = sess_list;
	while (list)
	{
		sess = (session *) list->data;
		if (sess->server == killserv)
			return;					  /* this server is still being used! */
		list = list->next;
	}

	server_free (killserv);
}

static void
free_sessions (void)
{
	GSList *list = sess_list;

	while (list)
	{
		fe_close_window (list->data);
		list = sess_list;
	}
}


static char defaultconf_ctcp[] =
	"NAME TIME\n"				"CMD nctcp %s TIME %t\n\n"\
	"NAME PING\n"				"CMD nctcp %s PING %d\n\n";

static char defaultconf_replace[] =
	"NAME teh\n"				"CMD the\n\n";
/*	"NAME r\n"					"CMD are\n\n"\
	"NAME u\n"					"CMD you\n\n"*/

static char defaultconf_commands[] =
	"NAME ACTION\n"		"CMD me &2\n\n"\
	"NAME AME\n"			"CMD allchan me &2\n\n"\
	"NAME ANICK\n"			"CMD allserv nick &2\n\n"\
	"NAME AMSG\n"			"CMD allchan say &2\n\n"\
	"NAME BANLIST\n"		"CMD quote MODE %c +b\n\n"\
	"NAME CHAT\n"			"CMD dcc chat %2\n\n"\
	"NAME DIALOG\n"		"CMD query %2\n\n"\
	"NAME DMSG\n"			"CMD msg =%2 &3\n\n"\
	"NAME EXIT\n"			"CMD quit\n\n"\
	"NAME GREP\n"			"CMD lastlog -r -- &2\n\n"\
	"NAME IGNALL\n"			"CMD ignore %2!*@* ALL\n\n"\
	"NAME J\n"				"CMD join &2\n\n"\
	"NAME KILL\n"			"CMD quote KILL %2 :&3\n\n"\
	"NAME LEAVE\n"			"CMD part &2\n\n"\
	"NAME M\n"				"CMD msg &2\n\n"\
	"NAME OMSG\n"			"CMD msg @%c &2\n\n"\
	"NAME ONOTICE\n"		"CMD notice @%c &2\n\n"\
	"NAME RAW\n"			"CMD quote &2\n\n"\
	"NAME SERVHELP\n"		"CMD quote HELP\n\n"\
	"NAME SPING\n"			"CMD ping\n\n"\
	"NAME SQUERY\n"		"CMD quote SQUERY %2 :&3\n\n"\
	"NAME SSLSERVER\n"	"CMD server -ssl &2\n\n"\
	"NAME SV\n"				"CMD echo ZoiteChat %v %m\n\n"\
	"NAME UMODE\n"			"CMD mode %n &2\n\n"\
	"NAME UPTIME\n"		"CMD quote STATS u\n\n"\
	"NAME VER\n"			"CMD ctcp %2 VERSION\n\n"\
	"NAME VERSION\n"		"CMD ctcp %2 VERSION\n\n"\
	"NAME WALLOPS\n"		"CMD quote WALLOPS :&2\n\n"\
        "NAME WI\n"                     "CMD quote WHOIS %2\n\n"\
	"NAME WII\n"			"CMD quote WHOIS %2 %2\n\n";

static char defaultconf_urlhandlers[] =
		"NAME Open Link in a new Firefox Window\n"		"CMD !firefox -new-window %s\n\n";

#ifdef USE_SIGACTION
/* Close and open log files on SIGUSR1. Usefull for log rotating */

static void 
sigusr1_handler (int signal, siginfo_t *si, void *un)
{
	GSList *list = sess_list;
	session *sess;

	while (list)
	{
		sess = list->data;
		log_open_or_close (sess);
		list = list->next;
	}
}

/* Execute /SIGUSR2 when SIGUSR2 received */

static void
sigusr2_handler (int signal, siginfo_t *si, void *un)
{
	session *sess = current_sess;

	if (sess)
		handle_command (sess, "SIGUSR2", FALSE);
}
#endif

static gint
xchat_auto_connect (gpointer userdata)
{
	servlist_auto_connect (NULL);
	return 0;
}

static void
xchat_init (void)
{
	char buf[3068];

#ifdef WIN32
	WSADATA wsadata;

	if (WSAStartup(0x0202, &wsadata) != 0)
	{
		MessageBox (NULL, "Cannot find winsock 2.2+", "Error", MB_OK);
		exit (0);
	}
#endif	/* !WIN32 */

#ifdef USE_SIGACTION
	struct sigaction act;

	/* ignore SIGPIPE's */
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigemptyset (&act.sa_mask);
	sigaction (SIGPIPE, &act, NULL);

	/* Deal with SIGUSR1's & SIGUSR2's */
	act.sa_sigaction = sigusr1_handler;
	act.sa_flags = 0;
	sigemptyset (&act.sa_mask);
	sigaction (SIGUSR1, &act, NULL);

	act.sa_sigaction = sigusr2_handler;
	act.sa_flags = 0;
	sigemptyset (&act.sa_mask);
	sigaction (SIGUSR2, &act, NULL);
#else
#ifndef WIN32
	/* good enough for these old systems */
	signal (SIGPIPE, SIG_IGN);
#endif
#endif

	load_text_events ();
	sound_load ();
	notify_load ();
	ignore_load ();
	sts_init ();

	g_snprintf (buf, sizeof (buf),
		"NAME %s~%s~\n"				"CMD query %%s\n\n"\
		"NAME %s~%s~\n"				"CMD send %%s\n\n"\
		"NAME %s~%s~\n"				"CMD whois %%s %%s\n\n"\
		"NAME %s~%s~\n"				"CMD notify -n ASK %%s\n\n"\
		"NAME %s~%s~\n"				"CMD ignore %%s!*@* ALL\n\n"\

		"NAME SUB\n"					"CMD %s\n\n"\
			"NAME %s\n"					"CMD op %%a\n\n"\
			"NAME %s\n"					"CMD deop %%a\n\n"\
			"NAME SEP\n"				"CMD \n\n"\
			"NAME %s\n"					"CMD voice %%a\n\n"\
			"NAME %s\n"					"CMD devoice %%a\n"\
			"NAME SEP\n"				"CMD \n\n"\
			"NAME SUB\n"				"CMD %s\n\n"\
				"NAME %s\n"				"CMD kick %%s\n\n"\
				"NAME %s\n"				"CMD ban %%s\n\n"\
				"NAME SEP\n"			"CMD \n\n"\
				"NAME %s *!*@*.host\n""CMD ban %%s 0\n\n"\
				"NAME %s *!*@domain\n""CMD ban %%s 1\n\n"\
				"NAME %s *!*user@*.host\n""CMD ban %%s 2\n\n"\
				"NAME %s *!*user@domain\n""CMD ban %%s 3\n\n"\
				"NAME SEP\n"			"CMD \n\n"\
				"NAME %s *!*@*.host\n""CMD kickban %%s 0\n\n"\
				"NAME %s *!*@domain\n""CMD kickban %%s 1\n\n"\
				"NAME %s *!*user@*.host\n""CMD kickban %%s 2\n\n"\
				"NAME %s *!*user@domain\n""CMD kickban %%s 3\n\n"\
			"NAME ENDSUB\n"			"CMD \n\n"\
		"NAME ENDSUB\n"				"CMD \n\n",

		_("_Open Dialog Window"), "zc-menu-new",
		_("_Send a File" ELLIPSIS), "zc-menu-save",
		_("_User Info (WhoIs)"), "zc-menu-about",
		_("_Add to Friends List" ELLIPSIS), "zc-menu-add",
		_("_Ignore"), "zc-menu-remove",
		_("O_perator Actions"),

		_("Give Ops"),
		_("Take Ops"),
		_("Give Voice"),
		_("Take Voice"),

		_("Kick/Ban"),
		_("Kick"),
		_("Ban"),
		_("Ban"),
		_("Ban"),
		_("Ban"),
		_("Ban"),
		_("KickBan"),
		_("KickBan"),
		_("KickBan"),
		_("KickBan"));

	list_loadconf ("popup.conf", &popup_list, buf);

	g_snprintf (buf, sizeof (buf),
		"NAME %s\n"				"CMD part\n\n"
		"NAME %s\n"				"CMD getstr # join \"%s\"\n\n"
		"NAME %s\n"				"CMD quote LINKS\n\n"
		"NAME %s\n"				"CMD ping\n\n"
		"NAME TOGGLE %s\n"	"CMD irc_hide_version\n\n",
				_("Leave Channel"),
				_("Join Channel..."),
				_("Enter Channel to Join:"),
				_("Server Links"),
				_("Ping Server"),
				_("Hide Version"));
	list_loadconf ("usermenu.conf", &usermenu_list, buf);

	g_snprintf (buf, sizeof (buf),
		"NAME %s\n"		"CMD op %%a\n\n"
		"NAME %s\n"		"CMD deop %%a\n\n"
		"NAME %s\n"		"CMD ban %%s\n\n"
		"NAME %s\n"		"CMD getstr \"%s\" \"kick %%s\" \"%s\"\n\n"
		"NAME %s\n"		"CMD send %%s\n\n"
		"NAME %s\n"		"CMD query %%s\n\n",
				_("Op"),
				_("DeOp"),
				_("Ban"),
				_("Kick"),
				_("bye"),
				_("Enter reason to kick %s:"),
				_("Send File"),
				_("Dialog"));
	list_loadconf ("buttons.conf", &button_list, buf);

	g_snprintf (buf, sizeof (buf),
		"NAME %s\n"				"CMD whois %%s %%s\n\n"
		"NAME %s\n"				"CMD send %%s\n\n"
		"NAME %s\n"				"CMD dcc chat %%s\n\n"
		"NAME %s\n"				"CMD clear\n\n"
		"NAME %s\n"				"CMD ping %%s\n\n",
				_("WhoIs"),
				_("Send"),
				_("Chat"),
				_("Clear"),
				_("Ping"));
	list_loadconf ("dlgbuttons.conf", &dlgbutton_list, buf);

	list_loadconf ("tabmenu.conf", &tabmenu_list, NULL);
	list_loadconf ("ctcpreply.conf", &ctcp_list, defaultconf_ctcp);
	list_loadconf ("commands.conf", &command_list, defaultconf_commands);
	list_loadconf ("replace.conf", &replace_list, defaultconf_replace);
	list_loadconf ("urlhandlers.conf", &urlhandler_list,
						defaultconf_urlhandlers);

	servlist_init ();							/* load server list */

	/* if we got a URL, don't open the server list GUI */
	if (!prefs.hex_gui_slist_skip && !arg_url && !arg_urls)
		fe_serverlist_open (NULL);

	/* turned OFF via -a arg or by passing urls */
	if (!arg_dont_autoconnect && !arg_urls && !arg_url)
	{
		/* do any auto connects */
		if (!servlist_have_auto ())	/* if no new windows open .. */
		{
			/* and no serverlist gui ... */
			if (prefs.hex_gui_slist_skip || arg_url || arg_urls)
				/* we'll have to open one. */
				new_ircwindow (NULL, NULL, SESS_SERVER, 0);
		} else
		{
			fe_idle_add (xchat_auto_connect, NULL);
		}
	} else
	{
		if (prefs.hex_gui_slist_skip || arg_url || arg_urls)
			new_ircwindow (NULL, NULL, SESS_SERVER, 0);
	}
}

void
zoitechat_exit (void)
{
	zoitechat_is_quitting = TRUE;
	in_zoitechat_exit = TRUE;
	plugin_kill_all ();
	fe_cleanup ();

	save_config ();
	if (prefs.save_pevents)
	{
		pevent_save (NULL);
	}

	sound_save ();
	notify_save ();
	ignore_save ();
	sts_cleanup ();
	free_sessions ();
	chanopt_save_all (TRUE);
	servlist_cleanup ();
	fe_exit ();
}

void
zoitechat_exec (const char *cmd)
{
	util_exec (cmd);
}


static void
set_locale (void)
{
#ifdef WIN32
	char zoitechat_lang[13];	/* LC_ALL= plus 5 chars of hex_gui_lang and trailing \0 */

	strcpy (zoitechat_lang, "LC_ALL=");

	if (0 <= prefs.hex_gui_lang && prefs.hex_gui_lang < LANGUAGES_LENGTH)
		strcat (zoitechat_lang, languages[prefs.hex_gui_lang]);
	else
		strcat (zoitechat_lang, "en");

	putenv (zoitechat_lang);
#endif
}

int
main (int argc, char *argv[])
{
	int i;
	int ret;

#ifdef WIN32
	HRESULT coinit_result;
#endif

	srand ((unsigned int) time (NULL)); /* CL: do this only once! */

	/* We must check for the config dir parameter, otherwise load_config() will behave incorrectly.
	 * load_config() must come before fe_args() because fe_args() calls gtk_init() which needs to
	 * know the language which is set in the config. The code below is copy-pasted from fe_args()
	 * for the most part. */
	if (argc >= 2)
	{
		for (i = 1; i < argc; i++)
		{
			if ((strcmp (argv[i], "-d") == 0 || strcmp (argv[i], "--cfgdir") == 0)
				&& i + 1 < argc)
			{
				xdir = g_strdup (argv[i + 1]);
			}
			else if (strncmp (argv[i], "--cfgdir=", 9) == 0)
			{
				xdir = g_strdup (argv[i] + 9);
			}

			if (xdir != NULL)
			{
				if (xdir[strlen (xdir) - 1] == G_DIR_SEPARATOR)
				{
					xdir[strlen (xdir) - 1] = 0;
				}
				break;
			}
		}
	}

#if ! GLIB_CHECK_VERSION (2, 36, 0)
	g_type_init ();
#endif

	if (check_config_dir () == 0)
	{
		if (load_config () != 0)
			load_default_config ();
	} else
	{
		/* this is probably the first run */
		load_default_config ();
		make_config_dirs ();
		make_dcc_dirs ();
	}

	/* we MUST do this after load_config () AND before fe_init (thus gtk_init) otherwise it will fail */
	set_locale ();

	ret = fe_args (argc, argv);
	if (ret != -1)
		return ret;

#ifdef WIN32
	if (zoitechat_remote_win32 ())
		return 0;
#endif

#ifdef USE_DBUS
	zoitechat_remote ();
#endif

#ifdef WIN32
	coinit_result = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
	if (SUCCEEDED (coinit_result))
	{
		CoInitializeSecurity (NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
	}
#endif

	fe_init ();

	/* This is done here because cfgfiles.c is too early in
	* the startup process to use gtk functions. */
	if (g_access (get_xdir (), W_OK) != 0)
	{
		char buf[2048];

		g_snprintf (buf, sizeof(buf),
			_("You do not have write access to %s. Nothing from this session can be saved."),
			get_xdir ());
		fe_message (buf, FE_MSG_ERROR);
	}

#ifndef WIN32
#ifndef __EMX__
	/* OS/2 uses UID 0 all the time */
	if (getuid () == 0)
		fe_message (_("* Running IRC as root is stupid! You should\n"
			      "  create a User Account and use that to login.\n"), FE_MSG_WARN|FE_MSG_WAIT);
#endif
#endif /* !WIN32 */

	xchat_init ();

	fe_main ();

#ifdef WIN32
	if (SUCCEEDED (coinit_result))
	{
		CoUninitialize ();
	}
#endif

#ifdef WIN32
	WSACleanup ();
#endif

	return 0;
}
