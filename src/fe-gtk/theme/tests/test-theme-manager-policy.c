#include <gtk/gtk.h>

#include "../theme-palette.h"
#include "../theme-manager.h"
#include "../../fe-gtk.h"
#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"

struct session *current_sess;
struct session *current_tab;
struct session *lastact_sess;
struct zoitechatprefs prefs;

static gboolean stub_policy_dark;
static unsigned int stub_policy_mode;
static gboolean stub_apply_mode_result;
static gboolean stub_apply_mode_palette_changed;
static int stub_dark_set_calls;
static int stub_user_set_calls;
static int stub_apply_mode_calls;
static int stub_reload_style_calls;
static ThemeSemanticToken stub_last_dark_token;
static ThemeSemanticToken stub_last_user_token;

static int listener_a_calls;
static int listener_b_calls;
static ThemeChangedEvent listener_last_event;

void setup_apply_real (const ThemeChangedEvent *event)
{
	(void) event;
}

gboolean fe_dark_mode_is_enabled_for (unsigned int mode)
{
	return mode == ZOITECHAT_DARK_MODE_DARK;
}

void fe_set_auto_dark_mode_state (gboolean enabled)
{
	(void) enabled;
}

gboolean fe_win32_high_contrast_is_enabled (void)
{
	return FALSE;
}

gboolean fe_win32_try_get_system_dark (gboolean *enabled)
{
	(void) enabled;
	return FALSE;
}

void zoitechat_set_theme_post_apply_callback (zoitechat_theme_post_apply_callback callback)
{
	(void) callback;
}

gboolean theme_policy_is_dark_mode_active (unsigned int mode)
{
	stub_policy_mode = mode;
	return stub_policy_dark;
}

gboolean theme_application_apply_mode (unsigned int mode, gboolean *palette_changed)
{
	(void) mode;
	if (palette_changed)
		*palette_changed = stub_apply_mode_palette_changed;
	return stub_apply_mode_result;
}

void theme_runtime_dark_set_color (ThemeSemanticToken token, const GdkRGBA *color)
{
	(void) color;
	stub_dark_set_calls++;
	stub_last_dark_token = token;
}

void theme_runtime_user_set_color (ThemeSemanticToken token, const GdkRGBA *color)
{
	(void) color;
	stub_user_set_calls++;
	stub_last_user_token = token;
}

void theme_runtime_reset_mode_colors (gboolean dark_mode)
{
	(void) dark_mode;
}

gboolean theme_runtime_apply_mode (unsigned int mode, gboolean *dark_active)
{
	(void) mode;
	(void) dark_active;
	stub_apply_mode_calls++;
	return TRUE;
}

void theme_application_reload_input_style (void)
{
	stub_reload_style_calls++;
}


void theme_css_reload_input_style (gboolean enabled, const PangoFontDescription *font_desc)
{
	(void) enabled;
	(void) font_desc;
}

void theme_css_apply_palette_widget (GtkWidget *widget, const GdkRGBA *bg, const GdkRGBA *fg,
				     const PangoFontDescription *font_desc)
{
	(void) widget;
	(void) bg;
	(void) fg;
	(void) font_desc;
}

void theme_runtime_load (void)
{
}

void theme_runtime_save (void)
{
}

gboolean theme_runtime_is_dark_active (void)
{
	return FALSE;
}

gboolean theme_policy_system_prefers_dark (void)
{
	return FALSE;
}

void gtkutil_apply_palette (GtkWidget *widget, const GdkRGBA *background, const GdkRGBA *foreground,
				const PangoFontDescription *font_desc)
{
	(void) widget;
	(void) background;
	(void) foreground;
	(void) font_desc;
}

void theme_get_widget_style_values (ThemeWidgetStyleValues *out_values)
{
	gdk_rgba_parse (&out_values->background, "#101010");
	gdk_rgba_parse (&out_values->foreground, "#f0f0f0");
}

void theme_get_widget_style_values_for_widget (GtkWidget *widget, ThemeWidgetStyleValues *out_values)
{
	(void) widget;
	theme_get_widget_style_values (out_values);
}

void fe_win32_apply_native_titlebar (GtkWidget *window, gboolean dark)
{
	(void) window;
	(void) dark;
}

static void
listener_a (const ThemeChangedEvent *event, gpointer userdata)
{
	(void) userdata;
	listener_a_calls++;
	listener_last_event = *event;
}

static void
listener_b (const ThemeChangedEvent *event, gpointer userdata)
{
	(void) userdata;
	(void) event;
	listener_b_calls++;
}

static void
reset_manager_stubs (void)
{
	stub_policy_dark = FALSE;
	stub_policy_mode = 999;
	stub_apply_mode_result = TRUE;
	stub_apply_mode_palette_changed = FALSE;
	stub_dark_set_calls = 0;
	stub_user_set_calls = 0;
	stub_apply_mode_calls = 0;
	stub_reload_style_calls = 0;
	stub_last_dark_token = -1;
	stub_last_user_token = -1;
	listener_a_calls = 0;
	listener_b_calls = 0;
}

static void
test_token_roundtrip (void)
{
	size_t i;

	for (i = 0; i < theme_palette_token_def_count (); i++)
	{
		const ThemePaletteTokenDef *def = theme_palette_token_def_at (i);
		int legacy_idx = -1;
		ThemeSemanticToken token = THEME_TOKEN_MIRC_0;

		g_assert_nonnull (def);
		g_assert_true (theme_palette_token_to_legacy_index (def->token, &legacy_idx));
		g_assert_cmpint (legacy_idx, ==, def->legacy_index);
		g_assert_true (theme_palette_legacy_index_to_token (legacy_idx, &token));
		g_assert_cmpint (token, ==, def->token);
	}
}

static void
test_policy_mode_resolution (void)
{
	g_assert_false (theme_policy_is_dark_mode_active (ZOITECHAT_DARK_MODE_LIGHT));
	g_assert_true (theme_policy_is_dark_mode_active (ZOITECHAT_DARK_MODE_DARK));
}

static void
test_manager_set_token_color_routes_by_mode (void)
{
	GdkRGBA color = { 0.1, 0.2, 0.3, 1.0 };
	gboolean palette_changed = FALSE;

	reset_manager_stubs ();
	stub_policy_dark = FALSE;
	theme_manager_set_token_color (ZOITECHAT_DARK_MODE_LIGHT, THEME_TOKEN_MIRC_2, &color, &palette_changed);
	g_assert_cmpint (stub_policy_mode, ==, ZOITECHAT_DARK_MODE_LIGHT);
	g_assert_cmpint (stub_user_set_calls, ==, 1);
	g_assert_cmpint (stub_dark_set_calls, ==, 0);
	g_assert_cmpint (stub_apply_mode_calls, ==, 1);
	g_assert_cmpint (stub_reload_style_calls, ==, 1);
	g_assert_true (palette_changed);

	reset_manager_stubs ();
	stub_policy_dark = TRUE;
	theme_manager_set_token_color (ZOITECHAT_DARK_MODE_DARK, THEME_TOKEN_MIRC_2, &color, &palette_changed);
	g_assert_cmpint (stub_policy_mode, ==, ZOITECHAT_DARK_MODE_DARK);
	g_assert_cmpint (stub_user_set_calls, ==, 0);
	g_assert_cmpint (stub_dark_set_calls, ==, 1);

	reset_manager_stubs ();
	stub_policy_dark = TRUE;
	theme_manager_set_token_color (ZOITECHAT_DARK_MODE_AUTO, THEME_TOKEN_MIRC_2, &color, &palette_changed);
	g_assert_cmpint (stub_policy_mode, ==, ZOITECHAT_DARK_MODE_AUTO);
	g_assert_cmpint (stub_user_set_calls, ==, 0);
	g_assert_cmpint (stub_dark_set_calls, ==, 1);
}


static void
test_manager_set_token_color_routes_setup_indexes (void)
{
	GdkRGBA color = { 0.7, 0.3, 0.2, 1.0 };
	gboolean palette_changed = FALSE;
	size_t i;

	for (i = 0; i < theme_palette_token_def_count (); i++)
	{
		const ThemePaletteTokenDef *def = theme_palette_token_def_at (i);

		g_assert_nonnull (def);
		reset_manager_stubs ();
		stub_policy_dark = FALSE;
		palette_changed = FALSE;
		theme_manager_set_token_color (ZOITECHAT_DARK_MODE_LIGHT, def->token, &color, &palette_changed);
		g_assert_cmpint (stub_user_set_calls, ==, 1);
		g_assert_cmpint (stub_last_user_token, ==, def->token);
		g_assert_cmpint (stub_dark_set_calls, ==, 0);
		g_assert_true (palette_changed);

		reset_manager_stubs ();
		stub_policy_dark = TRUE;
		palette_changed = FALSE;
		theme_manager_set_token_color (ZOITECHAT_DARK_MODE_DARK, def->token, &color, &palette_changed);
		g_assert_cmpint (stub_dark_set_calls, ==, 1);
		g_assert_cmpint (stub_last_dark_token, ==, def->token);
		g_assert_cmpint (stub_user_set_calls, ==, 0);
		g_assert_true (palette_changed);
	}
}

static void
test_manager_listener_registration_dispatch_and_unregister (void)
{
	guint id_a;
	guint id_b;

	reset_manager_stubs ();
	id_a = theme_listener_register ("test.a", listener_a, NULL);
	id_b = theme_listener_register ("test.b", listener_b, NULL);
	g_assert_cmpuint (id_a, >, 0);
	g_assert_cmpuint (id_b, >, 0);

	theme_manager_dispatch_changed (THEME_CHANGED_REASON_PIXMAP | THEME_CHANGED_REASON_USERLIST | THEME_CHANGED_REASON_IDENTD | THEME_CHANGED_REASON_WIDGET_STYLE);
	g_assert_cmpint (listener_a_calls, ==, 1);
	g_assert_cmpint (listener_b_calls, ==, 1);
	g_assert_true (theme_changed_event_has_reason (&listener_last_event, THEME_CHANGED_REASON_PIXMAP));
	g_assert_true (theme_changed_event_has_reason (&listener_last_event, THEME_CHANGED_REASON_USERLIST));
	g_assert_false (theme_changed_event_has_reason (&listener_last_event, THEME_CHANGED_REASON_LAYOUT));
	g_assert_true (theme_changed_event_has_reason (&listener_last_event, THEME_CHANGED_REASON_IDENTD));
	g_assert_true (theme_changed_event_has_reason (&listener_last_event, THEME_CHANGED_REASON_WIDGET_STYLE));

	theme_listener_unregister (id_a);
	theme_manager_dispatch_changed (THEME_CHANGED_REASON_PIXMAP | THEME_CHANGED_REASON_LAYOUT | THEME_CHANGED_REASON_WIDGET_STYLE);
	g_assert_cmpint (listener_a_calls, ==, 1);
	g_assert_cmpint (listener_b_calls, ==, 2);

	theme_listener_unregister (id_b);
}

static void
test_manager_window_attach_detach_idempotence (void)
{
	GtkWidget *window;
	gulong *first_handler_ptr;
	gulong first_handler_id;
	gulong *second_handler_ptr;
	gulong second_handler_id;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_assert_nonnull (window);

	theme_manager_attach_window (window);
	first_handler_ptr = g_object_get_data (G_OBJECT (window), "theme-manager-window-destroy-handler");
	g_assert_nonnull (first_handler_ptr);
	first_handler_id = *first_handler_ptr;
	g_assert_cmpuint (first_handler_id, >, 0);
	g_assert_true (g_signal_handler_is_connected (G_OBJECT (window), first_handler_id));

	theme_manager_attach_window (window);
	second_handler_ptr = g_object_get_data (G_OBJECT (window), "theme-manager-window-destroy-handler");
	g_assert_nonnull (second_handler_ptr);
	g_assert_true (first_handler_ptr == second_handler_ptr);
	g_assert_cmpuint (*second_handler_ptr, ==, first_handler_id);

	theme_manager_detach_window (window);
	g_assert_null (g_object_get_data (G_OBJECT (window), "theme-manager-window-destroy-handler"));
	g_assert_false (g_signal_handler_is_connected (G_OBJECT (window), first_handler_id));

	theme_manager_detach_window (window);
	g_assert_null (g_object_get_data (G_OBJECT (window), "theme-manager-window-destroy-handler"));

	theme_manager_attach_window (window);
	second_handler_ptr = g_object_get_data (G_OBJECT (window), "theme-manager-window-destroy-handler");
	g_assert_nonnull (second_handler_ptr);
	second_handler_id = *second_handler_ptr;
	g_assert_cmpuint (second_handler_id, >, 0);
	g_assert_true (g_signal_handler_is_connected (G_OBJECT (window), second_handler_id));

	theme_manager_detach_window (window);
	g_assert_null (g_object_get_data (G_OBJECT (window), "theme-manager-window-destroy-handler"));
	g_assert_false (g_signal_handler_is_connected (G_OBJECT (window), second_handler_id));

	gtk_widget_destroy (window);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/theme/palette/token_roundtrip", test_token_roundtrip);
	g_test_add_func ("/theme/policy/mode_resolution", test_policy_mode_resolution);
	g_test_add_func ("/theme/manager/set_token_color_routes_by_mode", test_manager_set_token_color_routes_by_mode);
	g_test_add_func ("/theme/manager/set_token_color_routes_setup_indexes",
			 test_manager_set_token_color_routes_setup_indexes);
	g_test_add_func ("/theme/manager/listener_registration_dispatch_and_unregister",
			 test_manager_listener_registration_dispatch_and_unregister);
	g_test_add_func ("/theme/manager/window_attach_detach_idempotence",
			 test_manager_window_attach_detach_idempotence);
	return g_test_run ();
}
