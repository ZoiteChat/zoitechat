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

#include "../theme-application.h"
#include "../../maingui.h"
#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"

struct session *current_sess;
struct session *current_tab;
struct session *lastact_sess;
struct zoitechatprefs prefs;
InputStyle *input_style;

static gboolean css_enabled;
static PangoFontDescription *css_font_desc;
static int css_reload_calls;
static int message_calls;

void
fe_message (char *msg, int flags)
{
	(void) msg;
	(void) flags;
	message_calls++;
}

void
theme_css_reload_input_style (gboolean enabled, const PangoFontDescription *font_desc)
{
	css_enabled = enabled;
	if (css_font_desc)
		pango_font_description_free (css_font_desc);
	css_font_desc = font_desc ? pango_font_description_copy (font_desc) : NULL;
	css_reload_calls++;
}

void
theme_runtime_load (void)
{
}

gboolean
theme_runtime_apply_mode (unsigned int mode, gboolean *palette_changed)
{
	(void) mode;
	if (palette_changed)
		*palette_changed = FALSE;
	return FALSE;
}

static void
reset_state (void)
{
	if (css_font_desc)
	{
		pango_font_description_free (css_font_desc);
		css_font_desc = NULL;
	}
	if (input_style)
	{
		if (input_style->font_desc)
			pango_font_description_free (input_style->font_desc);
		g_free (input_style);
		input_style = NULL;
	}
	css_enabled = FALSE;
	css_reload_calls = 0;
	message_calls = 0;
}

static void
test_invalid_font_falls_back_to_sans_11 (void)
{
	InputStyle *style;

	reset_state ();
	g_strlcpy (prefs.hex_text_font, "Sans", sizeof (prefs.hex_text_font));
	prefs.hex_gui_input_style = TRUE;

	style = theme_application_update_input_style (NULL);
	g_assert_nonnull (style);
	g_assert_nonnull (style->font_desc);
	g_assert_cmpstr (pango_font_description_get_family (style->font_desc), ==, "sans");
	g_assert_cmpint (pango_font_description_get_size (style->font_desc), ==, 11 * PANGO_SCALE);
	g_assert_cmpint (message_calls, ==, 1);
	g_assert_cmpint (css_reload_calls, ==, 1);
	g_assert_true (css_enabled);
	g_assert_nonnull (css_font_desc);

	if (style->font_desc)
		pango_font_description_free (style->font_desc);
	g_free (style);
}

static void
test_style_toggle_routes_enabled_flag (void)
{
	reset_state ();
	g_strlcpy (prefs.hex_text_font, "sans 13", sizeof (prefs.hex_text_font));
	prefs.hex_gui_input_style = FALSE;
	input_style = NULL;

	theme_application_reload_input_style ();
	g_assert_nonnull (input_style);
	g_assert_cmpint (css_reload_calls, ==, 1);
	g_assert_false (css_enabled);
	g_assert_nonnull (css_font_desc);
	g_assert_cmpstr (pango_font_description_get_family (css_font_desc), ==, "sans");
	g_assert_cmpint (pango_font_description_get_size (css_font_desc), ==, 13 * PANGO_SCALE);
	g_assert_cmpint (message_calls, ==, 0);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/theme/application/input_style/invalid_font_fallback",
			 test_invalid_font_falls_back_to_sans_11);
	g_test_add_func ("/theme/application/input_style/style_toggle_routes_enabled_flag",
			 test_style_toggle_routes_enabled_flag);
	return g_test_run ();
}
