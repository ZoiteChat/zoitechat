#include <errno.h>

#include "zoitechat.h"

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "cfgfiles.h"
#include "theme-service.h"

static zoitechat_theme_post_apply_callback zoitechat_theme_post_apply_cb;

static gboolean
zoitechat_theme_service_copy_file (const char *src, const char *dest, GError **error)
{
	char *data = NULL;
	gsize len = 0;

	if (!g_file_get_contents (src, &data, &len, error))
		return FALSE;

	if (!g_file_set_contents (dest, data, len, error))
	{
		g_free (data);
		return FALSE;
	}

	g_free (data);
	return TRUE;
}

char *
zoitechat_theme_service_get_themes_dir (void)
{
	return g_build_filename (get_xdir (), "themes", NULL);
}

static gboolean
zoitechat_theme_service_validate (const char *theme_name,
                                  char **colors_src,
                                  char **events_src,
                                  GError **error)
{
	char *themes_dir;
	char *theme_dir;

	if (!theme_name || !*theme_name)
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		             _("No theme name specified."));
		return FALSE;
	}

	themes_dir = zoitechat_theme_service_get_themes_dir ();
	theme_dir = g_build_filename (themes_dir, theme_name, NULL);
	g_free (themes_dir);

	*colors_src = g_build_filename (theme_dir, "colors.conf", NULL);
	*events_src = g_build_filename (theme_dir, "pevents.conf", NULL);

	if (!g_file_test (*colors_src, G_FILE_TEST_IS_REGULAR))
	{
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
		             _("This theme is missing a colors.conf file."));
		g_free (*events_src);
		g_free (*colors_src);
		*events_src = NULL;
		*colors_src = NULL;
		g_free (theme_dir);
		return FALSE;
	}

	g_free (theme_dir);
	return TRUE;
}

gboolean
zoitechat_theme_service_apply (const char *theme_name, GError **error)
{
	char *colors_src = NULL;
	char *colors_dest = NULL;
	char *events_src = NULL;
	char *events_dest = NULL;
	gboolean ok = FALSE;

	if (!zoitechat_theme_service_validate (theme_name, &colors_src, &events_src, error))
		return FALSE;

	colors_dest = g_build_filename (get_xdir (), "colors.conf", NULL);
	events_dest = g_build_filename (get_xdir (), "pevents.conf", NULL);

	if (!zoitechat_theme_service_copy_file (colors_src, colors_dest, error))
		goto cleanup;

	if (g_file_test (events_src, G_FILE_TEST_IS_REGULAR))
	{
		if (!zoitechat_theme_service_copy_file (events_src, events_dest, error))
			goto cleanup;
	}
	else if (g_file_test (events_dest, G_FILE_TEST_EXISTS))
	{
		if (g_unlink (events_dest) != 0)
		{
			g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
			             _("Failed to remove existing event settings."));
			goto cleanup;
		}
	}

	zoitechat_theme_service_run_post_apply_callback ();
	ok = TRUE;

cleanup:
	g_free (events_dest);
	g_free (events_src);
	g_free (colors_dest);
	g_free (colors_src);
	return ok;
}

GStrv
zoitechat_theme_service_discover_themes (void)
{
	char *themes_dir;
	GDir *dir;
	const char *name;
	GPtrArray *themes;
	GStrv result;

	themes_dir = zoitechat_theme_service_get_themes_dir ();
	if (!g_file_test (themes_dir, G_FILE_TEST_IS_DIR))
		g_mkdir_with_parents (themes_dir, 0700);

	themes = g_ptr_array_new_with_free_func (g_free);
	dir = g_dir_open (themes_dir, 0, NULL);
	if (dir)
	{
		while ((name = g_dir_read_name (dir)))
		{
			char *theme_dir = g_build_filename (themes_dir, name, NULL);
			char *colors_path;

			if (!g_file_test (theme_dir, G_FILE_TEST_IS_DIR))
			{
				g_free (theme_dir);
				continue;
			}

			colors_path = g_build_filename (theme_dir, "colors.conf", NULL);
			if (g_file_test (colors_path, G_FILE_TEST_IS_REGULAR))
				g_ptr_array_add (themes, g_strdup (name));

			g_free (colors_path);
			g_free (theme_dir);
		}
		g_dir_close (dir);
	}

	g_ptr_array_sort (themes, (GCompareFunc) g_strcmp0);
	g_ptr_array_add (themes, NULL);
	result = (GStrv) g_ptr_array_free (themes, FALSE);
	g_free (themes_dir);
	return result;
}

void
zoitechat_theme_service_set_post_apply_callback (zoitechat_theme_post_apply_callback callback)
{
	zoitechat_theme_post_apply_cb = callback;
}

void
zoitechat_theme_service_run_post_apply_callback (void)
{
	if (zoitechat_theme_post_apply_cb)
		zoitechat_theme_post_apply_cb ();
}
