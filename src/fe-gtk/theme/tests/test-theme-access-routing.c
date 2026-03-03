#include <math.h>
#include <gtk/gtk.h>

#include "../theme-access.h"
#include "../theme-manager.h"
#include "../theme-runtime.h"
#include "../../xtext-color.h"
#include "../../../common/zoitechat.h"

struct session *current_sess;
struct session *current_tab;
struct session *lastact_sess;
struct zoitechatprefs prefs;

static gboolean stub_dark_active;
static ThemeSemanticToken stub_last_color_token;
static int stub_runtime_get_color_calls;
static int stub_runtime_widget_calls;
static int stub_runtime_xtext_calls;
static size_t stub_runtime_xtext_last_len;

static GdkRGBA stub_light_colors[THEME_TOKEN_COUNT];
static GdkRGBA stub_dark_colors[THEME_TOKEN_COUNT];

void
setup_apply_real (const ThemeChangedEvent *event)
{
	(void) event;
}

gboolean
fe_dark_mode_is_enabled_for (unsigned int mode)
{
	return mode == ZOITECHAT_DARK_MODE_DARK;
}

gboolean
theme_runtime_apply_mode (unsigned int mode, gboolean *dark_active)
{
	if (mode == ZOITECHAT_DARK_MODE_DARK)
		stub_dark_active = TRUE;
	if (mode == ZOITECHAT_DARK_MODE_LIGHT)
		stub_dark_active = FALSE;
	if (dark_active)
		*dark_active = stub_dark_active;
	return TRUE;
}

void
theme_runtime_load (void)
{
}

void
theme_runtime_save (void)
{
}

void
theme_runtime_user_set_color (ThemeSemanticToken token, const GdkRGBA *col)
{
	(void) token;
	(void) col;
}

void
theme_runtime_dark_set_color (ThemeSemanticToken token, const GdkRGBA *col)
{
	(void) token;
	(void) col;
}

gboolean
theme_runtime_get_color (ThemeSemanticToken token, GdkRGBA *out_rgba)
{
	g_assert_nonnull (out_rgba);
	stub_runtime_get_color_calls++;
	stub_last_color_token = token;
	*out_rgba = stub_dark_active ? stub_dark_colors[token] : stub_light_colors[token];
	return TRUE;
}

void
theme_runtime_get_widget_style_values (ThemeWidgetStyleValues *out_values)
{
	stub_runtime_widget_calls++;
	gdk_rgba_parse (&out_values->background, "#010203");
	gdk_rgba_parse (&out_values->foreground, "#fdfcfa");
}

void
theme_runtime_get_xtext_colors (XTextColor *palette, size_t palette_len)
{
	size_t i;

	stub_runtime_xtext_calls++;
	stub_runtime_xtext_last_len = palette_len;
	for (i = 0; i < palette_len; i++)
	{
		palette[i].red = (unsigned short) (i + 1);
		palette[i].green = (unsigned short) (i + 2);
		palette[i].blue = (unsigned short) (i + 3);
	}
}

gboolean
theme_runtime_is_dark_active (void)
{
	return stub_dark_active;
}

static gboolean
rgba_equal (const GdkRGBA *a, const GdkRGBA *b)
{
	return a->red == b->red && a->green == b->green && a->blue == b->blue && a->alpha == b->alpha;
}

static void
reset_stubs (void)
{
	size_t i;
	char light[32];
	char dark[32];

	stub_dark_active = FALSE;
	stub_last_color_token = THEME_TOKEN_MIRC_0;
	stub_runtime_get_color_calls = 0;
	stub_runtime_widget_calls = 0;
	stub_runtime_xtext_calls = 0;
	stub_runtime_xtext_last_len = 0;
	for (i = 0; i < THEME_TOKEN_COUNT; i++)
	{
		g_snprintf (light, sizeof (light), "#%02x%02x%02x", (unsigned int) (i + 1), 0x11, 0x22);
		g_snprintf (dark, sizeof (dark), "#%02x%02x%02x", (unsigned int) (i + 1), 0xaa, 0xbb);
		g_assert_true (gdk_rgba_parse (&stub_light_colors[i], light));
		g_assert_true (gdk_rgba_parse (&stub_dark_colors[i], dark));
	}
}

static void
test_access_semantic_token_routes_directly (void)
{
	ThemeSemanticToken token;
	GdkRGBA color;
	size_t i;

	reset_stubs ();
	for (i = 0; i < theme_palette_token_def_count (); i++)
	{
		const ThemePaletteTokenDef *def = theme_palette_token_def_at (i);

		g_assert_nonnull (def);
		token = def->token;
		g_assert_true (theme_get_color (token, &color));
		g_assert_cmpint (stub_last_color_token, ==, token);
		g_assert_true (rgba_equal (&color, &stub_light_colors[token]));
	}
}

static void
test_access_token_routes_without_legacy_accessor (void)
{
	ThemeSemanticToken token = THEME_TOKEN_MIRC_0;
	GdkRGBA color;
	size_t i;

	reset_stubs ();
	for (i = 0; i < theme_palette_token_def_count (); i++)
	{
		const ThemePaletteTokenDef *def = theme_palette_token_def_at (i);

		g_assert_nonnull (def);
		g_assert_true (theme_palette_legacy_index_to_token (def->legacy_index, &token));
		g_assert_true (theme_get_color (token, &color));
		g_assert_cmpint (stub_last_color_token, ==, token);
		g_assert_true (rgba_equal (&color, &stub_light_colors[token]));
	}
}

static void
test_access_xtext_palette_forwarding (void)
{
	XTextColor palette[4] = { 0 };

	reset_stubs ();
	theme_get_xtext_colors (palette, G_N_ELEMENTS (palette));
	g_assert_cmpint (stub_runtime_xtext_calls, ==, 1);
	g_assert_cmpuint (stub_runtime_xtext_last_len, ==, G_N_ELEMENTS (palette));
	g_assert_cmpuint (palette[0].red, ==, 1);
	g_assert_cmpuint (palette[1].green, ==, 3);
	g_assert_cmpuint (palette[3].blue, ==, 6);
}


static void
test_access_widget_style_forwarding (void)
{
	ThemeWidgetStyleValues values;

	reset_stubs ();
	theme_get_widget_style_values (&values);
	g_assert_cmpint (stub_runtime_widget_calls, ==, 1);
	g_assert_true (fabs (values.background.red - (0x01 / 255.0)) < 0.0001);
	g_assert_true (fabs (values.foreground.green - (0xfc / 255.0)) < 0.0001);
}

static void
test_access_dark_light_switch_affects_token_consumers (void)
{
	ThemeSemanticToken token;
	GdkRGBA light;
	GdkRGBA dark;
	reset_stubs ();
	token = THEME_TOKEN_TEXT_FOREGROUND;
	g_assert_true (theme_runtime_apply_mode (ZOITECHAT_DARK_MODE_LIGHT, NULL));
	g_assert_true (theme_get_color (token, &light));
	g_assert_true (rgba_equal (&light, &stub_light_colors[token]));

	g_assert_true (theme_runtime_apply_mode (ZOITECHAT_DARK_MODE_DARK, NULL));
	g_assert_true (theme_get_color (token, &dark));
	g_assert_true (rgba_equal (&dark, &stub_dark_colors[token]));
	g_assert_false (rgba_equal (&light, &dark));
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/theme/access/semantic_token_routes_directly", test_access_semantic_token_routes_directly);
	g_test_add_func ("/theme/access/token_routes_without_legacy_accessor", test_access_token_routes_without_legacy_accessor);
	g_test_add_func ("/theme/access/xtext_palette_forwarding", test_access_xtext_palette_forwarding);
	g_test_add_func ("/theme/access/widget_style_forwarding", test_access_widget_style_forwarding);
	g_test_add_func ("/theme/access/dark_light_switch_affects_token_consumers",
			 test_access_dark_light_switch_affects_token_consumers);
	return g_test_run ();
}
