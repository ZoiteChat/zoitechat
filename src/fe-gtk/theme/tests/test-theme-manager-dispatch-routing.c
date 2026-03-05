#include <gtk/gtk.h>
#include <string.h>

#include "../theme-manager.h"
#include "../../fe-gtk.h"
#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"

struct session *current_sess;
struct session *current_tab;
struct session *lastact_sess;
struct zoitechatprefs prefs;

static int window_refresh_calls;
static int widget_style_calls;
static int palette_reapply_calls;
static int unmatched_listener_calls;

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
	return mode == ZOITECHAT_DARK_MODE_DARK;
}

gboolean theme_policy_system_prefers_dark (void)
{
	return FALSE;
}

gboolean theme_application_apply_mode (unsigned int mode, gboolean *palette_changed)
{
	(void) mode;
	if (palette_changed)
		*palette_changed = FALSE;
	return TRUE;
}

void theme_application_reload_input_style (void)
{
}

void theme_runtime_dark_set_color (ThemeSemanticToken token, const GdkRGBA *color)
{
	(void) token;
	(void) color;
}

void theme_runtime_user_set_color (ThemeSemanticToken token, const GdkRGBA *color)
{
	(void) token;
	(void) color;
}

void theme_runtime_reset_mode_colors (gboolean dark_mode)
{
	(void) dark_mode;
}

gboolean theme_runtime_apply_mode (unsigned int mode, gboolean *dark_active)
{
	(void) mode;
	(void) dark_active;
	return FALSE;
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
window_refresh_listener (const ThemeChangedEvent *event, gpointer userdata)
{
	(void) userdata;
	if (theme_changed_event_has_reason (event, THEME_CHANGED_REASON_PALETTE))
		palette_reapply_calls++;
	if (theme_changed_event_has_reason (event, THEME_CHANGED_REASON_PALETTE) ||
		theme_changed_event_has_reason (event, THEME_CHANGED_REASON_WIDGET_STYLE))
		window_refresh_calls++;
}

static void
widget_style_listener (const ThemeChangedEvent *event, gpointer userdata)
{
	(void) userdata;
	if (theme_changed_event_has_reason (event, THEME_CHANGED_REASON_WIDGET_STYLE))
		widget_style_calls++;
}

static void
unmatched_reason_listener (const ThemeChangedEvent *event, gpointer userdata)
{
	(void) userdata;
	if (theme_changed_event_has_reason (event, THEME_CHANGED_REASON_IDENTD))
		unmatched_listener_calls++;
}

static void
reset_counters (void)
{
	window_refresh_calls = 0;
	widget_style_calls = 0;
	palette_reapply_calls = 0;
	unmatched_listener_calls = 0;
}

static void
test_dispatch_filters_reasons_across_multiple_subscribers (void)
{
	guint listener_window;
	guint listener_widget;
	guint listener_unmatched;

	reset_counters ();
	listener_window = theme_listener_register ("refresh.window", window_refresh_listener, NULL);
	listener_widget = theme_listener_register ("refresh.widget", widget_style_listener, NULL);
	listener_unmatched = theme_listener_register ("refresh.unmatched", unmatched_reason_listener, NULL);

	theme_manager_dispatch_changed (THEME_CHANGED_REASON_PALETTE);
	g_assert_cmpint (window_refresh_calls, ==, 1);
	g_assert_cmpint (palette_reapply_calls, ==, 1);
	g_assert_cmpint (widget_style_calls, ==, 0);
	g_assert_cmpint (unmatched_listener_calls, ==, 0);

	theme_manager_dispatch_changed (THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST);
	g_assert_cmpint (window_refresh_calls, ==, 2);
	g_assert_cmpint (palette_reapply_calls, ==, 1);
	g_assert_cmpint (widget_style_calls, ==, 1);
	g_assert_cmpint (unmatched_listener_calls, ==, 0);

	theme_manager_dispatch_changed (THEME_CHANGED_REASON_LAYOUT);
	g_assert_cmpint (window_refresh_calls, ==, 2);
	g_assert_cmpint (palette_reapply_calls, ==, 1);
	g_assert_cmpint (widget_style_calls, ==, 1);
	g_assert_cmpint (unmatched_listener_calls, ==, 0);

	theme_listener_unregister (listener_unmatched);
	theme_listener_unregister (listener_widget);
	theme_listener_unregister (listener_window);
}


static void
test_preferences_change_synthesizes_theme_reasons (void)
{
	struct zoitechatprefs old_prefs = { 0 };
	struct zoitechatprefs new_prefs = { 0 };
	ThemeChangedEvent event;
	gboolean color_change = TRUE;

	prefs.hex_gui_dark_mode = ZOITECHAT_DARK_MODE_DARK;
	old_prefs.hex_gui_dark_mode = prefs.hex_gui_dark_mode;
	new_prefs.hex_gui_dark_mode = prefs.hex_gui_dark_mode;
	strcpy (old_prefs.hex_text_background, "old.png");
	strcpy (new_prefs.hex_text_background, "new.png");
	old_prefs.hex_gui_tab_dots = 0;
	new_prefs.hex_gui_tab_dots = 1;
	old_prefs.hex_identd_port = 113;
	new_prefs.hex_identd_port = 114;
	old_prefs.hex_gui_ulist_color = 0;
	new_prefs.hex_gui_ulist_color = 1;

	event = theme_manager_on_preferences_changed (&old_prefs, &new_prefs, prefs.hex_gui_dark_mode, &color_change);

	g_assert_true (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_PIXMAP));
	g_assert_true (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_LAYOUT));
	g_assert_true (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_IDENTD));
	g_assert_true (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_USERLIST));
	g_assert_true (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_WIDGET_STYLE));
}

static void
test_preferences_change_omits_reasons_without_differences (void)
{
	struct zoitechatprefs old_prefs = { 0 };
	struct zoitechatprefs new_prefs = { 0 };
	ThemeChangedEvent event;
	gboolean color_change = FALSE;

	prefs.hex_gui_dark_mode = ZOITECHAT_DARK_MODE_DARK;
	old_prefs.hex_gui_dark_mode = prefs.hex_gui_dark_mode;
	new_prefs.hex_gui_dark_mode = prefs.hex_gui_dark_mode;

	event = theme_manager_on_preferences_changed (&old_prefs, &new_prefs, prefs.hex_gui_dark_mode, &color_change);

	g_assert_false (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_PIXMAP));
	g_assert_false (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_LAYOUT));
	g_assert_false (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_IDENTD));
	g_assert_false (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_USERLIST));
	g_assert_false (theme_changed_event_has_reason (&event, THEME_CHANGED_REASON_WIDGET_STYLE));
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/theme/manager/dispatch_filters_reasons_across_multiple_subscribers",
			 test_dispatch_filters_reasons_across_multiple_subscribers);
	g_test_add_func ("/theme/manager/preferences_change_synthesizes_theme_reasons",
			 test_preferences_change_synthesizes_theme_reasons);
	g_test_add_func ("/theme/manager/preferences_change_omits_reasons_without_differences",
			 test_preferences_change_omits_reasons_without_differences);
	return g_test_run ();
}
