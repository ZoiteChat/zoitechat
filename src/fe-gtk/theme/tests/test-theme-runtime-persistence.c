#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "../theme-runtime.h"
#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"

struct session *current_sess;
struct session *current_tab;
struct session *lastact_sess;
struct zoitechatprefs prefs;

static char *test_home_dir;

static gboolean
read_line_value (const char *cfg, const char *key, char *out, gsize out_len)
{
	char *pattern;
	char *pos;
	char *line_end;
	gsize value_len;

	pattern = g_strdup_printf ("%s = ", key);
	pos = g_strstr_len (cfg, -1, pattern);
	g_free (pattern);
	if (!pos)
		return FALSE;

	pos = strchr (pos, '=');
	if (!pos)
		return FALSE;
	pos++;
	while (*pos == ' ')
		pos++;

	line_end = strchr (pos, '\n');
	if (!line_end)
		line_end = pos + strlen (pos);
	value_len = (gsize) (line_end - pos);
	if (value_len + 1 > out_len)
		return FALSE;
	memcpy (out, pos, value_len);
	out[value_len] = '\0';
	return TRUE;
}

int
cfg_get_color (char *cfg, char *var, guint16 *r, guint16 *g, guint16 *b)
{
	char value[128];

	if (!read_line_value (cfg, var, value, sizeof (value)))
		return 0;
	if (sscanf (value, "%04hx %04hx %04hx", r, g, b) != 3)
		return 0;
	return 1;
}

int
cfg_get_int (char *cfg, char *var)
{
	char value[128];

	if (!read_line_value (cfg, var, value, sizeof (value)))
		return 0;
	return atoi (value);
}

int
cfg_put_color (int fh, guint16 r, guint16 g, guint16 b, char *var)
{
	char line[256];
	int len;

	len = g_snprintf (line, sizeof line, "%s = %04hx %04hx %04hx\n", var, r, g, b);
	if (len < 0)
		return 0;
	return write (fh, line, (size_t) len) == len;
}

int
cfg_put_int (int fh, int value, char *var)
{
	char line[128];
	int len;

	len = g_snprintf (line, sizeof line, "%s = %d\n", var, value);
	if (len < 0)
		return 0;
	return write (fh, line, (size_t) len) == len;
}

int
zoitechat_open_file (const char *file, int flags, int mode, int xof_flags)
{
	char *path;
	int fd;

	(void) xof_flags;
	path = g_build_filename (test_home_dir, file, NULL);
	fd = g_open (path, flags, mode);
	g_free (path);
	return fd;
}

gboolean
fe_dark_mode_is_enabled_for (unsigned int mode)
{
	return mode == ZOITECHAT_DARK_MODE_DARK;
}

gboolean
theme_policy_system_prefers_dark (void)
{
	return FALSE;
}

gboolean
theme_policy_is_dark_mode_active (unsigned int mode)
{
	return mode == ZOITECHAT_DARK_MODE_DARK;
}

static void
setup_temp_home (void)
{
	if (test_home_dir)
		return;
	test_home_dir = g_dir_make_tmp ("zoitechat-theme-tests-XXXXXX", NULL);
	g_assert_nonnull (test_home_dir);
}

static char *
read_colors_conf (void)
{
	char *path;
	char *content = NULL;
	gsize length = 0;
	gboolean ok;

	path = g_build_filename (test_home_dir, "colors.conf", NULL);
	ok = g_file_get_contents (path, &content, &length, NULL);
	g_free (path);
	g_assert_true (ok);
	g_assert_cmpuint (length, >, 0);
	return content;
}

static gboolean
colors_equal (const GdkRGBA *a, const GdkRGBA *b)
{
	return a->red == b->red && a->green == b->green && a->blue == b->blue;
}

static void
apply_ui_color_edit (unsigned int mode, ThemeSemanticToken token, const char *hex)
{
	GdkRGBA color;

	g_assert_true (gdk_rgba_parse (&color, hex));
	if (theme_policy_is_dark_mode_active (mode))
		theme_runtime_dark_set_color (token, &color);
	else
		theme_runtime_user_set_color (token, &color);
	theme_runtime_apply_mode (mode, NULL);
}

static void
test_persistence_roundtrip_light_and_dark (void)
{
	GdkRGBA light_color;
	GdkRGBA dark_color;
	GdkRGBA loaded;
	char *cfg;

	setup_temp_home ();
	theme_runtime_load ();

	gdk_rgba_parse (&light_color, "#123456");
	theme_runtime_user_set_color (THEME_TOKEN_MIRC_0, &light_color);
	theme_runtime_apply_dark_mode (FALSE);

	theme_runtime_apply_dark_mode (TRUE);
	gdk_rgba_parse (&dark_color, "#abcdef");
	theme_runtime_dark_set_color (THEME_TOKEN_MIRC_0, &dark_color);

	theme_runtime_save ();
	cfg = read_colors_conf ();
	g_assert_nonnull (g_strstr_len (cfg, -1, "theme.mode.light.token.mirc_0"));
	g_assert_nonnull (g_strstr_len (cfg, -1, "theme.mode.dark.token.mirc_0"));
	g_assert_null (g_strstr_len (cfg, -1, "color_0 = "));
	g_assert_null (g_strstr_len (cfg, -1, "dark_color_0 = "));
	g_free (cfg);

	theme_runtime_load ();
	theme_runtime_apply_dark_mode (FALSE);
	g_assert_true (theme_runtime_get_color (THEME_TOKEN_MIRC_0, &loaded));
	g_assert_true (colors_equal (&light_color, &loaded));

	theme_runtime_apply_dark_mode (TRUE);
	g_assert_true (theme_runtime_get_color (THEME_TOKEN_MIRC_0, &loaded));
	g_assert_true (colors_equal (&dark_color, &loaded));
}

static void
test_loads_legacy_color_keys_via_migration_loader (void)
{
	char *path;
	const char *legacy_cfg =
		"color_0 = 1111 2222 3333\n"
		"dark_color_0 = aaaa bbbb cccc\n";
	GdkRGBA loaded;
	GdkRGBA light_expected;
	GdkRGBA dark_expected;
	gboolean ok;

	setup_temp_home ();
	path = g_build_filename (test_home_dir, "colors.conf", NULL);
	ok = g_file_set_contents (path, legacy_cfg, -1, NULL);
	g_free (path);
	g_assert_true (ok);

	theme_runtime_load ();

	gdk_rgba_parse (&light_expected, "#111122223333");
	gdk_rgba_parse (&dark_expected, "#aaaabbbbcccc");

	theme_runtime_apply_dark_mode (FALSE);
	g_assert_true (theme_runtime_get_color (THEME_TOKEN_MIRC_0, &loaded));
	g_assert_true (colors_equal (&loaded, &light_expected));

	theme_runtime_apply_dark_mode (TRUE);
	g_assert_true (theme_runtime_get_color (THEME_TOKEN_MIRC_0, &loaded));
	g_assert_true (colors_equal (&loaded, &dark_expected));
}


static void
test_ui_edits_persist_without_legacy_array_mutation (void)
{
	GdkRGBA light_loaded;
	GdkRGBA dark_loaded;
	GdkRGBA light_expected;
	GdkRGBA dark_expected;

	setup_temp_home ();
	theme_runtime_load ();

	apply_ui_color_edit (ZOITECHAT_DARK_MODE_LIGHT, THEME_TOKEN_SELECTION_FOREGROUND, "#224466");
	apply_ui_color_edit (ZOITECHAT_DARK_MODE_DARK, THEME_TOKEN_SELECTION_FOREGROUND, "#88aacc");
	theme_runtime_save ();

	theme_runtime_load ();
	theme_runtime_apply_mode (ZOITECHAT_DARK_MODE_LIGHT, NULL);
	g_assert_true (theme_runtime_get_color (THEME_TOKEN_SELECTION_FOREGROUND, &light_loaded));
	g_assert_true (gdk_rgba_parse (&light_expected, "#224466"));
	g_assert_true (colors_equal (&light_loaded, &light_expected));

	theme_runtime_apply_mode (ZOITECHAT_DARK_MODE_DARK, NULL);
	g_assert_true (theme_runtime_get_color (THEME_TOKEN_SELECTION_FOREGROUND, &dark_loaded));
	g_assert_true (gdk_rgba_parse (&dark_expected, "#88aacc"));
	g_assert_true (colors_equal (&dark_loaded, &dark_expected));
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/theme/runtime/persistence_roundtrip_light_and_dark",
			 test_persistence_roundtrip_light_and_dark);
	g_test_add_func ("/theme/runtime/loads_legacy_color_keys_via_migration_loader",
			 test_loads_legacy_color_keys_via_migration_loader);
	g_test_add_func ("/theme/runtime/ui_edits_persist_without_legacy_array_mutation",
			 test_ui_edits_persist_without_legacy_array_mutation);
	return g_test_run ();
}
