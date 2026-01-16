#include "hct.h"

#include <string.h>

#include "cfgfiles.h"
#include "miniz.h"

#define HCT_ERROR (g_quark_from_static_string("zoitechat-hct"))

typedef enum
{
	HCT_ERROR_INVALID,
	HCT_ERROR_EXISTS,
	HCT_ERROR_EXTRACT
} hct_error;

static gboolean
hct_is_safe_component (const char *component)
{
	return component[0] != '\0' && strcmp (component, ".") != 0 && strcmp (component, "..") != 0;
}

static gboolean
hct_is_safe_path (const char *path)
{
	char **parts;
	guint i;
	gboolean ok = TRUE;

	if (!path || path[0] == '\0')
		return FALSE;

	if (g_path_is_absolute (path))
		return FALSE;

	parts = g_strsplit_set (path, "/\\", -1);
	for (i = 0; parts[i]; i++)
	{
		if (!hct_is_safe_component (parts[i]))
		{
			ok = FALSE;
			break;
		}
	}
	g_strfreev (parts);

	return ok;
}

gboolean
zoitechat_hct_is_file (const char *path)
{
	const char *ext;
	if (!path)
		return FALSE;

	if (!g_file_test (path, G_FILE_TEST_IS_REGULAR))
		return FALSE;

	ext = strrchr (path, '.');
	if (!ext)
		return FALSE;

	return g_ascii_strcasecmp (ext, ".hct") == 0;
}

static char *
zoitechat_hct_theme_name (const char *path)
{
	char *basename;
	char *name;
	size_t len;

	basename = g_path_get_basename (path);
	len = strlen (basename);
	if (len > 4 && g_ascii_strcasecmp (basename + len - 4, ".hct") == 0)
		basename[len - 4] = '\0';
	name = g_strdup (basename);
	g_free (basename);

	return name;
}

int
zoitechat_import_hct (const char *path, char **theme_name, GError **error)
{
	mz_zip_archive zip;
	mz_uint file_count;
	mz_uint i;
	char *name;
	char *themes_dir;
	char *dest_dir;
	int extracted = 0;

	if (!zoitechat_hct_is_file (path))
	{
		g_set_error (error, HCT_ERROR, HCT_ERROR_INVALID, "Invalid theme file.");
		return HCT_ERROR_INVALID;
	}

	name = zoitechat_hct_theme_name (path);
	themes_dir = g_build_filename (get_xdir (), "themes", NULL);
	g_mkdir_with_parents (themes_dir, 0700);
	dest_dir = g_build_filename (themes_dir, name, NULL);

	if (g_file_test (dest_dir, G_FILE_TEST_EXISTS))
	{
		g_set_error (error, HCT_ERROR, HCT_ERROR_EXISTS, "Theme already exists.");
		g_free (dest_dir);
		g_free (themes_dir);
		g_free (name);
		return HCT_ERROR_EXISTS;
	}

	memset (&zip, 0, sizeof (zip));
	if (!mz_zip_reader_init_file (&zip, path, 0))
	{
		g_set_error (error, HCT_ERROR, HCT_ERROR_INVALID, "Invalid theme file.");
		g_free (dest_dir);
		g_free (themes_dir);
		g_free (name);
		return HCT_ERROR_INVALID;
	}

	file_count = mz_zip_reader_get_num_files (&zip);
	g_mkdir_with_parents (dest_dir, 0700);

	for (i = 0; i < file_count; i++)
	{
		mz_zip_archive_file_stat stat;
		char *output_path;
		char *output_dir;

		if (!mz_zip_reader_file_stat (&zip, i, &stat))
			continue;

		if (stat.m_is_directory)
			continue;

		if (!hct_is_safe_path (stat.m_filename))
			continue;

		output_path = g_build_filename (dest_dir, stat.m_filename, NULL);
		output_dir = g_path_get_dirname (output_path);
		g_mkdir_with_parents (output_dir, 0700);

		if (!mz_zip_reader_extract_to_file (&zip, i, output_path, 0))
		{
			g_set_error (error, HCT_ERROR, HCT_ERROR_EXTRACT, "Failed to extract theme files.");
			g_free (output_dir);
			g_free (output_path);
			mz_zip_reader_end (&zip);
			g_free (dest_dir);
			g_free (themes_dir);
			g_free (name);
			return HCT_ERROR_EXTRACT;
		}

		extracted++;
		g_free (output_dir);
		g_free (output_path);
	}

	mz_zip_reader_end (&zip);

	if (extracted == 0)
	{
		g_set_error (error, HCT_ERROR, HCT_ERROR_INVALID, "Invalid theme file.");
		g_free (dest_dir);
		g_free (themes_dir);
		g_free (name);
		return HCT_ERROR_INVALID;
	}

	if (theme_name)
		*theme_name = g_strdup (name);

	g_free (dest_dir);
	g_free (themes_dir);
	g_free (name);

	return 0;
}
