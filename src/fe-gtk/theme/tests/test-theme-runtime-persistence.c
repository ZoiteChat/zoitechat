#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"

#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "../theme-runtime.h"

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

static void
test_gtk_map_colors_blend_with_palette_without_transparency (void)
{
	ThemeGtkPaletteMap map = { 0 };
	ThemeWidgetStyleValues base_values;
	ThemeWidgetStyleValues values;
	GdkRGBA mapped_bg;
	double alpha;
	double expected_red;
	double expected_green;
	double expected_blue;

	setup_temp_home ();
	theme_runtime_load ();
	theme_runtime_get_widget_style_values (&base_values);

	map.enabled = TRUE;
	g_assert_true (gdk_rgba_parse (&map.text_foreground, "rgba(10, 20, 30, 0.25)"));
	g_assert_true (gdk_rgba_parse (&map.text_background, "rgba(40, 50, 60, 0.30)"));
	g_assert_true (gdk_rgba_parse (&map.selection_foreground, "rgba(70, 80, 90, 0.35)"));
	g_assert_true (gdk_rgba_parse (&map.selection_background, "rgba(100, 110, 120, 0.40)"));
	g_assert_true (gdk_rgba_parse (&map.accent, "rgba(130, 140, 150, 0.45)"));

	theme_runtime_get_widget_style_values_mapped (&map, &values);
	g_assert_cmpfloat (values.foreground.alpha, ==, 1.0);
	g_assert_cmpfloat (values.background.alpha, ==, 1.0);
	g_assert_cmpfloat (values.selection_foreground.alpha, ==, 1.0);
	g_assert_cmpfloat (values.selection_background.alpha, ==, 1.0);

	mapped_bg = map.text_background;
	alpha = mapped_bg.alpha;
	expected_red = (mapped_bg.red * alpha) + (base_values.background.red * (1.0 - alpha));
	expected_green = (mapped_bg.green * alpha) + (base_values.background.green * (1.0 - alpha));
	expected_blue = (mapped_bg.blue * alpha) + (base_values.background.blue * (1.0 - alpha));
	g_assert_true (fabs (values.background.red - expected_red) < 0.0001);
	g_assert_true (fabs (values.background.green - expected_green) < 0.0001);
	g_assert_true (fabs (values.background.blue - expected_blue) < 0.0001);
}


static void
test_gtk_map_uses_theme_defaults_until_custom_token_is_set (void)
{
	ThemeGtkPaletteMap map = { 0 };
	ThemeWidgetStyleValues values;
	GdkRGBA custom;

	setup_temp_home ();
	theme_runtime_load ();

	map.enabled = TRUE;
	g_assert_true (gdk_rgba_parse (&map.text_foreground, "#010203"));
	g_assert_true (gdk_rgba_parse (&map.text_background, "#111213"));
	g_assert_true (gdk_rgba_parse (&map.selection_foreground, "#212223"));
	g_assert_true (gdk_rgba_parse (&map.selection_background, "#313233"));
	g_assert_true (gdk_rgba_parse (&map.accent, "#414243"));

	theme_runtime_get_widget_style_values_mapped (&map, &values);
	g_assert_true (colors_equal (&values.foreground, &map.text_foreground));

	g_assert_true (gdk_rgba_parse (&custom, "#a1b2c3"));
	theme_runtime_user_set_color (THEME_TOKEN_TEXT_FOREGROUND, &custom);
	theme_runtime_apply_mode (ZOITECHAT_DARK_MODE_LIGHT, NULL);
	theme_runtime_get_widget_style_values_mapped (&map, &values);
	g_assert_true (colors_equal (&values.foreground, &custom));
}

static void
test_save_writes_only_custom_token_keys (void)
{
	GdkRGBA custom;
	char *cfg;

	setup_temp_home ();
	theme_runtime_load ();
	g_assert_true (gdk_rgba_parse (&custom, "#445566"));
	theme_runtime_user_set_color (THEME_TOKEN_TEXT_FOREGROUND, &custom);
	theme_runtime_save ();

	cfg = read_colors_conf ();
	g_assert_nonnull (g_strstr_len (cfg, -1, "theme.mode.light.token.text_foreground"));
	g_assert_null (g_strstr_len (cfg, -1, "theme.mode.light.token.text_background"));
	g_free (cfg);
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
	g_test_add_func ("/theme/runtime/gtk_map_colors_blend_with_palette_without_transparency",
			 test_gtk_map_colors_blend_with_palette_without_transparency);
	g_test_add_func ("/theme/runtime/gtk_map_uses_theme_defaults_until_custom_token_is_set",
			 test_gtk_map_uses_theme_defaults_until_custom_token_is_set);
	g_test_add_func ("/theme/runtime/save_writes_only_custom_token_keys",
			 test_save_writes_only_custom_token_keys);
	return g_test_run ();
}
