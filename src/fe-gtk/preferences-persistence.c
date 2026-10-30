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

#include "preferences-persistence.h"

#include "../common/cfgfiles.h"
#include "theme/theme-runtime.h"

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

typedef struct
{
        char *name;
        char *contents;
        char *temporary;
        char *backup;
        gboolean existed;
        gboolean installed;
} PreferencesFile;

static GPtrArray *staged_files;

static void
preferences_file_free (gpointer data)
{
        PreferencesFile *file = data;
        if (file->temporary) g_unlink (file->temporary);
        if (file->backup) g_unlink (file->backup);
        g_free (file->name);
        g_free (file->contents);
        g_free (file->temporary);
        g_free (file->backup);
        g_free (file);
}

void
preferences_persistence_clear_staged_files (void)
{
        if (staged_files) g_ptr_array_free (staged_files, TRUE);
        staged_files = NULL;
}

void
preferences_persistence_stage_file (const char *name, const char *contents)
{
        PreferencesFile *file;
        g_return_if_fail (name && contents && !strchr (name, '/') && !strchr (name, '\\'));
        if (!staged_files)
                staged_files = g_ptr_array_new_with_free_func (preferences_file_free);
        file = g_new0 (PreferencesFile, 1);
        file->name = g_strdup (name);
        file->contents = g_strdup (contents);
        g_ptr_array_add (staged_files, file);
}

/* A temporary in the configuration directory stays on the same filesystem.
 * Restrictive permissions also protect command text and identity settings. */
static gboolean
preferences_write_temporary (const char *contents, gsize length, char **path)
{
        int fd;
        gsize written = 0;
        char *temporary = g_build_filename (get_xdir (), ".preferences-XXXXXX", NULL);
        fd = g_mkstemp (temporary);
        if (fd < 0) { g_free (temporary); return FALSE; }
        while (written < length)
        {
                gssize count = write (fd, contents + written, length - written);
                if (count < 0 && errno == EINTR) continue;
                if (count <= 0)
                {
                        close (fd);
                        g_unlink (temporary);
                        g_free (temporary);
                        return FALSE;
                }
                written += count;
        }
        if (close (fd) != 0)
        {
                g_unlink (temporary);
                g_free (temporary);
                return FALSE;
        }
        *path = temporary;
        return TRUE;
}

static gboolean
preferences_backup (PreferencesFile *file)
{
        char *path = g_build_filename (get_xdir (), file->name, NULL);
        char *contents = NULL;
        gsize length;
        GError *error = NULL;
        gboolean success;
        if (!g_file_get_contents (path, &contents, &length, &error))
        {
                success = g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
                g_clear_error (&error);
                g_free (path);
                return success;
        }
        file->existed = TRUE;
        success = preferences_write_temporary (contents, length, &file->backup);
        g_free (contents);
        g_free (path);
        return success;
}

static gboolean
preferences_install (PreferencesFile *file, const char *temporary)
{
        char *path = g_build_filename (get_xdir (), file->name, NULL);
        gboolean success;
#ifdef WIN32
        /* Match the existing config/theme save convention on Windows. The
         * backup remains available if replacement fails after unlinking. */
        if (file->existed && g_unlink (path) != 0)
        {
                g_free (path);
                return FALSE;
        }
        file->installed = TRUE;
#endif
        success = g_rename (temporary, path) == 0;
        if (success) file->installed = TRUE;
        g_free (path);
        return success;
}

static gboolean
preferences_restore (PreferencesFile *file)
{
        char *path;
        gboolean success;
        if (!file->installed) return TRUE;
        path = g_build_filename (get_xdir (), file->name, NULL);
        if (file->existed)
        {
#ifdef WIN32
                g_unlink (path);
#endif
                success = g_rename (file->backup, path) == 0;
                /* Retain the recovery copy if restoration itself fails. */
                if (!success) g_clear_pointer (&file->backup, g_free);
        }
        else success = g_unlink (path) == 0 || errno == ENOENT;
        g_free (path);
        return success;
}

PreferencesPersistenceResult
preferences_persistence_save_all (void)
{
        PreferencesPersistenceResult result = {0};
        GPtrArray *files = g_ptr_array_new_with_free_func (preferences_file_free);
        PreferencesFile *config = g_new0 (PreferencesFile, 1);
        PreferencesFile *theme = g_new0 (PreferencesFile, 1);
        guint i;
        result.retry_possible = TRUE;
        config->name = g_strdup ("zoitechat.conf");
        theme->name = g_strdup ("colors.conf");
        g_ptr_array_add (files, config);
        g_ptr_array_add (files, theme);
        if (!save_config_prepare (&config->temporary))
        {
                result.config_failed = TRUE;
                result.failed_file = "zoitechat.conf";
                goto done;
        }
        if (!theme_runtime_save_prepare (&theme->temporary))
        {
                result.theme_failed = TRUE;
                result.failed_file = "colors.conf";
                goto done;
        }
        for (i = 0; staged_files && i < staged_files->len; i++)
        {
                PreferencesFile *queued = g_ptr_array_index (staged_files, i);
                PreferencesFile *file = g_new0 (PreferencesFile, 1);
                file->name = g_strdup (queued->name);
                g_ptr_array_add (files, file);
                if (!preferences_write_temporary (queued->contents, strlen (queued->contents), &file->temporary))
                {
                        result.failed_file = queued->name;
                        goto done;
                }
        }
        /* Prepare every file and recovery copy before replacing anything. */
        for (i = 0; i < files->len; i++)
        {
                PreferencesFile *file = g_ptr_array_index (files, i);
                if (!preferences_backup (file))
                {
                        result.failed_file = i < 2 ? (i ? "colors.conf" : "zoitechat.conf")
                                : ((PreferencesFile *)g_ptr_array_index (staged_files, i - 2))->name;
                        result.config_failed = i == 0;
                        result.theme_failed = i == 1;
                        goto done;
                }
        }
        for (i = 0; i < files->len; i++)
        {
                PreferencesFile *file = g_ptr_array_index (files, i);
                if (!preferences_install (file, file->temporary))
                {
                        result.failed_file = i < 2 ? (i ? "colors.conf" : "zoitechat.conf")
                                : ((PreferencesFile *)g_ptr_array_index (staged_files, i - 2))->name;
                        result.config_failed = i == 0;
                        result.theme_failed = i == 1;
                        goto rollback;
                }
        }
        result.success = TRUE;
        goto done;
 rollback:
        for (i = files->len; i > 0; i--)
                if (!preferences_restore (g_ptr_array_index (files, i - 1)))
                        result.partial_failure = TRUE;
 done:
        g_ptr_array_free (files, TRUE);
        return result;
}
