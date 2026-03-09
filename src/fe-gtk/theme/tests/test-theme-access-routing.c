#include "../../fe-gtk.h"

#include <math.h>

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
static gboolean stub_gtk3_active;
static ThemeSemanticToken stub_last_color_token;
static int stub_runtime_get_color_calls;
static int stub_runtime_widget_calls;
static int stub_runtime_xtext_calls;
static int stub_runtime_xtext_mapped_calls;
static size_t stub_runtime_xtext_last_len;
static ThemeGtkPaletteMap stub_last_gtk_map;
static gboolean stub_last_gtk_map_valid;
static gboolean gtk_available;

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
theme_runtime_mode_has_user_colors (gboolean dark_mode)
{
	(void) dark_mode;
	return FALSE;
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

void
theme_runtime_get_widget_style_values_mapped (const ThemeGtkPaletteMap *gtk_map, ThemeWidgetStyleValues *out_values)
{
	(void) gtk_map;
	theme_runtime_get_widget_style_values (out_values);
}

void
theme_runtime_get_xtext_colors_mapped (const ThemeGtkPaletteMap *gtk_map, XTextColor *palette, size_t palette_len)
{
	size_t i;

	stub_runtime_xtext_mapped_calls++;
	stub_last_gtk_map = *gtk_map;
	stub_last_gtk_map_valid = TRUE;
	stub_runtime_xtext_last_len = palette_len;
	for (i = 0; i < palette_len; i++)
	{
		palette[i].red = (unsigned short) (100 + i);
		palette[i].green = (unsigned short) (200 + i);
		palette[i].blue = (unsigned short) (300 + i);
	}
}

gboolean
theme_gtk3_is_active (void)
{
	return stub_gtk3_active;
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
	stub_runtime_xtext_mapped_calls = 0;
	stub_runtime_xtext_last_len = 0;
	stub_last_gtk_map_valid = FALSE;
	stub_gtk3_active = FALSE;
	for (i = 0; i < THEME_TOKEN_COUNT; i++)
	{
		g_snprintf (light, sizeof (light), "#%02x%02x%02x", (unsigned int) (i + 1), 0x11, 0x22);
		g_snprintf (dark, sizeof (dark), "#%02x%02x%02x", (unsigned int) (i + 1), 0xaa, 0xbb);
		g_assert_true (gdk_rgba_parse (&stub_light_colors[i], light));
		g_assert_true (gdk_rgba_parse (&stub_dark_colors[i], dark));
	}
}

static gboolean
rgba_close (const GdkRGBA *a, const GdkRGBA *b)
{
	return fabs (a->red - b->red) < 0.0001 &&
		fabs (a->green - b->green) < 0.0001 &&
		fabs (a->blue - b->blue) < 0.0001 &&
		fabs (a->alpha - b->alpha) < 0.0001;
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
test_access_xtext_palette_widget_mapping_when_gtk3_active (void)
{
	GtkWidget *window;
	GtkWidget *label;
	GtkStyleContext *context;
	GtkCssProvider *provider;
	XTextColor palette[2] = { 0 };
	GdkRGBA expected;

	if (!gtk_available)
	{
		g_test_skip ("GTK display not available");
		return;
	}

	reset_stubs ();
	stub_gtk3_active = TRUE;
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	label = gtk_label_new ("mapped");
	gtk_container_add (GTK_CONTAINER (window), label);
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (provider,
		"label { color: #112233; background-color: #445566; }"
		"label:selected { color: #778899; background-color: #aabbcc; }"
		"label:link { color: #123456; }",
		-1,
		NULL);
	context = gtk_widget_get_style_context (label);
	gtk_style_context_add_provider (context,
		GTK_STYLE_PROVIDER (provider),
		GTK_STYLE_PROVIDER_PRIORITY_USER);
	gtk_widget_realize (window);

	theme_get_xtext_colors_for_widget (label, palette, G_N_ELEMENTS (palette));

	g_assert_cmpint (stub_runtime_xtext_mapped_calls, ==, 1);
	g_assert_cmpint (stub_runtime_xtext_calls, ==, 0);
	g_assert_true (stub_last_gtk_map_valid);
	g_assert_true (gdk_rgba_parse (&expected, "#112233"));
	g_assert_true (rgba_close (&stub_last_gtk_map.text_foreground, &expected));
	g_assert_true (gdk_rgba_parse (&expected, "#445566"));
	g_assert_true (rgba_close (&stub_last_gtk_map.text_background, &expected));
	g_assert_true (gdk_rgba_parse (&expected, "#778899"));
	g_assert_true (rgba_close (&stub_last_gtk_map.selection_foreground, &expected));
	g_assert_true (gdk_rgba_parse (&expected, "#aabbcc"));
	g_assert_true (rgba_close (&stub_last_gtk_map.selection_background, &expected));
	g_assert_true (gdk_rgba_parse (&expected, "#123456"));
	g_assert_true (rgba_close (&stub_last_gtk_map.accent, &expected));
	g_assert_cmpuint (palette[0].red, ==, 100);
	g_assert_cmpuint (palette[1].green, ==, 201);

	gtk_widget_destroy (window);
	g_object_unref (provider);
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
	g_test_add_func ("/theme/access/xtext_palette_widget_mapping_when_gtk3_active",
			 test_access_xtext_palette_widget_mapping_when_gtk3_active);
	g_test_add_func ("/theme/access/widget_style_forwarding", test_access_widget_style_forwarding);
	g_test_add_func ("/theme/access/dark_light_switch_affects_token_consumers",
			 test_access_dark_light_switch_affects_token_consumers);
	gtk_available = gtk_init_check (&argc, &argv);
	return g_test_run ();
}
