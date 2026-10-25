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

/* Regression coverage for saving multiple Preferences editors together. */
#include "../preferences-persistence.h"
#include "../../common/cfgfiles.h"
#include "../theme/theme-runtime.h"
#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

static char *test_directory;
static gboolean fail_theme_prepare;
static int rename_count, fail_install, fail_restore;

char *get_xdir (void) { return test_directory; }

static gboolean
prepare (const char *name, const char *contents, char **path)
{
        *path = g_build_filename (test_directory, name, NULL);
        return g_file_set_contents (*path, contents, -1, NULL);
}

int save_config_prepare (char **path) { return prepare ("config.new", "new config", path); }
gboolean theme_runtime_save_prepare (char **path)
{
        if (fail_theme_prepare) return FALSE;
        return prepare ("theme.new", "new theme", path);
}

static int
checked_rename (const char *source, const char *destination)
{
        rename_count++;
        if (rename_count == fail_install || rename_count == fail_restore)
        {
                errno = EACCES;
                return -1;
        }
        return g_rename (source, destination);
}

/* Exercise replacement failures without relying on filesystem permissions. */
#undef g_rename
#define g_rename checked_rename
#include "../preferences-persistence.c"
#undef g_rename

static void
put (const char *name, const char *contents)
{
        char *path = g_build_filename (test_directory, name, NULL);
        g_assert_true (g_file_set_contents (path, contents, -1, NULL));
        g_free (path);
}

static void
assert_contents (const char *name, const char *expected)
{
        char *path = g_build_filename (test_directory, name, NULL), *contents = NULL;
        g_assert_true (g_file_get_contents (path, &contents, NULL, NULL));
        g_assert_cmpstr (contents, ==, expected);
        g_free (contents);
        g_free (path);
}

static void
begin (void)
{
        test_directory = g_dir_make_tmp ("zoitechat-preferences-XXXXXX", NULL);
        g_assert_nonnull (test_directory);
        rename_count = fail_install = fail_restore = 0;
        fail_theme_prepare = FALSE;
        put ("zoitechat.conf", "old config");
        put ("colors.conf", "old theme");
        put ("commands.conf", "old commands");
        preferences_persistence_stage_file ("commands.conf", "new commands");
        preferences_persistence_stage_file ("replace.conf", "new replacements");
}

static int
temporary_count (void)
{
        GDir *directory = g_dir_open (test_directory, 0, NULL);
        const char *name;
        int count = 0;
        while ((name = g_dir_read_name (directory)))
                if (g_str_has_prefix (name, ".preferences-")) count++;
        g_dir_close (directory);
        return count;
}

static void
end (void)
{
        GDir *directory = g_dir_open (test_directory, 0, NULL);
        const char *name;
        preferences_persistence_clear_staged_files ();
        while ((name = g_dir_read_name (directory)))
        {
                char *path = g_build_filename (test_directory, name, NULL);
                if (g_file_test (path, G_FILE_TEST_IS_DIR)) g_rmdir (path);
                else g_unlink (path);
                g_free (path);
        }
        g_dir_close (directory);
        g_rmdir (test_directory);
        g_free (test_directory);
}

static void
test_success (void)
{
        PreferencesPersistenceResult result;
        begin ();
        result = preferences_persistence_save_all ();
        g_assert_true (result.success);
        assert_contents ("zoitechat.conf", "new config");
        assert_contents ("colors.conf", "new theme");
        assert_contents ("commands.conf", "new commands");
        assert_contents ("replace.conf", "new replacements");
        g_assert_cmpint (temporary_count (), ==, 0);
        end ();
}

static void
test_prepare_failure (void)
{
        PreferencesPersistenceResult result;
        begin ();
        fail_theme_prepare = TRUE;
        result = preferences_persistence_save_all ();
        g_assert_false (result.success);
        g_assert_false (result.partial_failure);
        g_assert_true (result.theme_failed);
        assert_contents ("zoitechat.conf", "old config");
        assert_contents ("commands.conf", "old commands");
        g_assert_cmpint (rename_count, ==, 0);
        g_assert_cmpint (temporary_count (), ==, 0);
        end ();
}

static void
test_backup_failure (void)
{
        PreferencesPersistenceResult result;
        char *path;
        begin ();
        path = g_build_filename (test_directory, "replace.conf", NULL);
        g_assert_cmpint (g_mkdir (path, 0700), ==, 0);
        g_free (path);
        result = preferences_persistence_save_all ();
        g_assert_false (result.success);
        g_assert_cmpstr (result.failed_file, ==, "replace.conf");
        assert_contents ("zoitechat.conf", "old config");
        assert_contents ("commands.conf", "old commands");
        g_assert_cmpint (rename_count, ==, 0);
        g_assert_cmpint (temporary_count (), ==, 0);
        end ();
}

static void
test_rollback_and_retry (void)
{
        PreferencesPersistenceResult result;
        begin ();
        fail_install = 4;
        result = preferences_persistence_save_all ();
        g_assert_false (result.success);
        g_assert_false (result.partial_failure);
        g_assert_true (result.retry_possible);
        assert_contents ("zoitechat.conf", "old config");
        assert_contents ("colors.conf", "old theme");
        assert_contents ("commands.conf", "old commands");
        g_assert_cmpint (temporary_count (), ==, 0);
        fail_install = 0;
        result = preferences_persistence_save_all ();
        g_assert_true (result.success);
        assert_contents ("commands.conf", "new commands");
        end ();
}

static void
test_recovery_copy (void)
{
        PreferencesPersistenceResult result;
        begin ();
        fail_install = 4;
        fail_restore = 5;
        result = preferences_persistence_save_all ();
        g_assert_false (result.success);
        g_assert_true (result.partial_failure);
        g_assert_cmpint (temporary_count (), ==, 1);
        assert_contents ("zoitechat.conf", "old config");
        assert_contents ("colors.conf", "old theme");
        end ();
}

int main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);
        g_test_add_func ("/preferences/save/success", test_success);
        g_test_add_func ("/preferences/save/prepare-failure", test_prepare_failure);
        g_test_add_func ("/preferences/save/backup-failure", test_backup_failure);
        g_test_add_func ("/preferences/save/rollback-retry", test_rollback_and_retry);
        g_test_add_func ("/preferences/save/recovery-copy", test_recovery_copy);
        return g_test_run ();
}
