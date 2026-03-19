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

#include "../../fe-gtk.h"

#include "../theme-manager.h"
#include "../theme-gtk3.h"
#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"

struct session *current_sess;
struct session *current_tab;
struct session *lastact_sess;
struct zoitechatprefs prefs;

static gboolean stub_apply_mode_palette_changed;
static gboolean stub_system_prefers_dark;
static int auto_state_calls;
static gboolean last_auto_state;
static int listener_calls;
static ThemeChangedEvent last_event;
static int idle_add_calls;
static guint next_idle_source_id = 33;

void test_theme_gtk3_stub_reset (void);
int test_theme_gtk3_stub_apply_current_calls (void);

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
	auto_state_calls++;
	last_auto_state = enabled;
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
	return stub_system_prefers_dark;
}

gboolean theme_application_apply_mode (unsigned int mode, gboolean *palette_changed)
{
	(void) mode;
	if (palette_changed)
		*palette_changed = stub_apply_mode_palette_changed;
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

gboolean theme_runtime_save (void)
{
	return TRUE;
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
auto_listener (const ThemeChangedEvent *event, gpointer userdata)
{
	(void) userdata;
	listener_calls++;
	last_event = *event;
}

static guint
immediate_idle_add (GSourceFunc function, gpointer data)
{
	idle_add_calls++;
	function (data);
	return next_idle_source_id++;
}

static void
reset_state (void)
{
	stub_apply_mode_palette_changed = FALSE;
	stub_system_prefers_dark = FALSE;
	auto_state_calls = 0;
	last_auto_state = FALSE;
	listener_calls = 0;
	idle_add_calls = 0;
	next_idle_source_id = 33;
	prefs.hex_gui_gtk3_variant = THEME_GTK3_VARIANT_FOLLOW_SYSTEM;
	test_theme_gtk3_stub_reset ();
}

static void
test_auto_refresh_dispatches_mode_palette_and_style_reasons (void)
{
	guint listener_id;

	reset_state ();
	prefs.hex_gui_dark_mode = ZOITECHAT_DARK_MODE_AUTO;
	stub_apply_mode_palette_changed = TRUE;
	stub_system_prefers_dark = TRUE;
	listener_id = theme_listener_register ("auto.refresh", auto_listener, NULL);
	theme_manager_set_idle_add_func (immediate_idle_add);

	theme_manager_refresh_auto_mode ();

	g_assert_cmpint (idle_add_calls, ==, 1);
	g_assert_cmpint (auto_state_calls, ==, 2);
	g_assert_true (last_auto_state);
	g_assert_cmpint (listener_calls, ==, 1);
	g_assert_cmpint (test_theme_gtk3_stub_apply_current_calls (), ==, 1);
	g_assert_true (theme_changed_event_has_reason (&last_event, THEME_CHANGED_REASON_PALETTE));
	g_assert_true (theme_changed_event_has_reason (&last_event, THEME_CHANGED_REASON_WIDGET_STYLE));
	g_assert_true (theme_changed_event_has_reason (&last_event, THEME_CHANGED_REASON_USERLIST));
	g_assert_true (theme_changed_event_has_reason (&last_event, THEME_CHANGED_REASON_MODE));

	theme_manager_set_idle_add_func (NULL);
	theme_listener_unregister (listener_id);
}

static void
test_auto_refresh_ignores_non_auto_mode (void)
{
	guint listener_id;

	reset_state ();
	prefs.hex_gui_dark_mode = ZOITECHAT_DARK_MODE_DARK;
	stub_apply_mode_palette_changed = TRUE;
	listener_id = theme_listener_register ("auto.nonauto", auto_listener, NULL);
	theme_manager_set_idle_add_func (immediate_idle_add);

	theme_manager_refresh_auto_mode ();

	g_assert_cmpint (idle_add_calls, ==, 1);
	g_assert_cmpint (auto_state_calls, ==, 0);
	g_assert_cmpint (listener_calls, ==, 0);
	g_assert_cmpint (test_theme_gtk3_stub_apply_current_calls (), ==, 0);

	theme_manager_set_idle_add_func (NULL);
	theme_listener_unregister (listener_id);
}

static void
test_auto_refresh_reapplies_gtk3_for_follow_system_variant (void)
{
	guint listener_id;

	reset_state ();
	prefs.hex_gui_dark_mode = ZOITECHAT_DARK_MODE_DARK;
	prefs.hex_gui_gtk3_variant = THEME_GTK3_VARIANT_FOLLOW_SYSTEM;
	listener_id = theme_listener_register ("auto.gtk3", auto_listener, NULL);
	theme_manager_set_idle_add_func (immediate_idle_add);

	theme_manager_refresh_auto_mode ();

	g_assert_cmpint (idle_add_calls, ==, 1);
	g_assert_cmpint (auto_state_calls, ==, 0);
	g_assert_cmpint (listener_calls, ==, 0);
	g_assert_cmpint (test_theme_gtk3_stub_apply_current_calls (), ==, 1);

	theme_manager_set_idle_add_func (NULL);
	theme_listener_unregister (listener_id);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/theme/manager/auto_refresh_dispatches_mode_palette_and_style_reasons",
			 test_auto_refresh_dispatches_mode_palette_and_style_reasons);
	g_test_add_func ("/theme/manager/auto_refresh_ignores_non_auto_mode",
			 test_auto_refresh_ignores_non_auto_mode);
	g_test_add_func ("/theme/manager/auto_refresh_reapplies_gtk3_for_follow_system_variant",
			 test_auto_refresh_reapplies_gtk3_for_follow_system_variant);
	return g_test_run ();
}
