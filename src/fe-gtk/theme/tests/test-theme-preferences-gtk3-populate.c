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

#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"
#include "../../../common/gtk3-theme-service.h"
#include "../theme-gtk3.h"
#include "../theme-manager.h"
#include "../theme-runtime.h"

struct session *current_sess;
struct session *current_tab;
struct zoitechatprefs prefs;
InputStyle *input_style;

static gboolean gtk_available;
static int apply_current_calls;
static int set_token_color_calls;
static char applied_theme_id[256];
static ThemeGtk3Variant applied_variant;
static gboolean removed_selected;

static gboolean custom_tokens[THEME_TOKEN_COUNT];
static GdkRGBA custom_colors[THEME_TOKEN_COUNT];

gboolean
theme_runtime_get_user_color (ThemeSemanticToken token, GdkRGBA *color)
{
        if (!custom_tokens[token]) return FALSE;
        *color = custom_colors[token];
        return TRUE;
}

gboolean
theme_runtime_get_default_color (ThemeSemanticToken token, const ThemeGtkPaletteMap *map, GdkRGBA *color)
{
        (void)map;
        return gdk_rgba_parse (color, token == THEME_TOKEN_TEXT_BACKGROUND || token == THEME_TOKEN_SELECTION_FOREGROUND ? "#ffffff" :
                token == THEME_TOKEN_SELECTION_BACKGROUND ? "#336699" : "#000000");
}

gboolean
theme_get_default_color_for_widget (GtkWidget *widget, ThemeSemanticToken token, GdkRGBA *color)
{
        (void)widget;
        theme_runtime_get_default_color (token, NULL, color);
        if (token == THEME_TOKEN_TEXT_BACKGROUND && prefs.hex_gui_dark_mode == ZOITECHAT_DARK_MODE_DARK)
                gdk_rgba_parse (color, "#181818");
        return TRUE;
}

void theme_runtime_reset_user_color (ThemeSemanticToken token) { custom_tokens[token] = FALSE; }
void theme_runtime_user_set_color (ThemeSemanticToken token, const GdkRGBA *color)
{ custom_tokens[token] = TRUE; custom_colors[token] = *color; }

gboolean theme_manager_apply_mode (unsigned int mode, gboolean *changed)
{ (void)mode; if (changed) *changed = FALSE; return FALSE; }
void theme_manager_apply_and_dispatch (unsigned int mode, ThemeChangedReason reasons, gboolean *changed)
{ (void)mode; (void)reasons; if (changed) *changed = FALSE; }
guint theme_listener_register (const char *id, ThemeChangedCallback callback, gpointer data)
{ (void)id; (void)callback; (void)data; return 0; }
void theme_listener_unregister (guint id) { (void)id; }
gboolean pevent_preferences_import_text (const char *contents, GError **error)
{ (void)contents; (void)error; return TRUE; }

void
setup_open_page (const char *id)
{
        (void)id;
}

void
setup_reveal_widget (GtkWidget *widget)
{
        (void)widget;
}

GtkWidget *
gtkutil_box_new (GtkOrientation orientation, gboolean homogeneous, gint spacing)
{
        (void)homogeneous;
        return gtk_box_new (orientation, spacing);
}

void
gtkutil_apply_palette (GtkWidget *wid, const GdkRGBA *fg, const GdkRGBA *bg, const PangoFontDescription *font)
{
        (void)wid;
        (void)fg;
        (void)bg;
        (void)font;
}

void
fe_open_url (const char *url)
{
        (void)url;
}

char *
get_xdir (void)
{
        return (char *)"/tmp";
}

void
load_text_events (void)
{
}

char **
text_color_event_names (int color, int *count)
{
        (void)color;
        if (count)
                *count = 0;
        return NULL;
}

gboolean
theme_get_color (ThemeSemanticToken token, GdkRGBA *color)
{
        (void)token;
        if (color)
                gdk_rgba_parse (color, "#000000");
        return TRUE;
}

void
theme_get_widget_style_values_for_widget (GtkWidget *widget, ThemeWidgetStyleValues *out_values)
{
        (void)widget;
        if (!out_values)
                return;
        gdk_rgba_parse (&out_values->foreground, "#111111");
        gdk_rgba_parse (&out_values->background, "#f0f0f0");
}

void
theme_manager_set_token_color (unsigned int dark_mode, ThemeSemanticToken token, const GdkRGBA *color, gboolean *changed)
{
        (void)dark_mode;
        (void)token;
        (void)color;
        set_token_color_calls++;
        theme_runtime_user_set_color (token, color);
        if (changed)
                *changed = FALSE;
}

void
theme_manager_reset_mode_colors (unsigned int mode, gboolean *palette_changed)
{
        (void)mode;
        if (palette_changed)
                *palette_changed = FALSE;
}

void
theme_runtime_clear_gtk_mapped_custom_tokens (void)
{
}

gboolean
theme_manager_save_preferences (void)
{
	return TRUE;
}

void
theme_manager_apply_to_window (GtkWidget *window)
{
        (void)window;
}

void
theme_manager_dispatch_changed (ThemeChangedReason reasons)
{
        (void)reasons;
}

ThemePaletteBehavior
theme_manager_get_userlist_palette_behavior (const PangoFontDescription *font_desc)
{
        ThemePaletteBehavior behavior;

        behavior.font_desc = font_desc;
        behavior.apply_background = FALSE;
        behavior.apply_foreground = FALSE;
        return behavior;
}

void
theme_manager_apply_userlist_style (GtkWidget *widget, ThemePaletteBehavior behavior)
{
        (void)widget;
        (void)behavior;
}

void
theme_manager_attach_window (GtkWidget *window)
{
        (void)window;
}


char *
zoitechat_gtk3_theme_service_get_user_themes_dir (void)
{
        return g_strdup ("/tmp");
}

static ZoitechatGtk3Theme *
new_theme (const char *id, const char *name, ZoitechatGtk3ThemeSource source)
{
        ZoitechatGtk3Theme *theme = g_new0 (ZoitechatGtk3Theme, 1);
        theme->id = g_strdup (id);
        theme->display_name = g_strdup (name);
        theme->source = source;
        return theme;
}

void
zoitechat_gtk3_theme_free (ZoitechatGtk3Theme *theme)
{
        if (!theme)
                return;
        g_free (theme->id);
        g_free (theme->display_name);
        g_free (theme->path);
        g_free (theme->thumbnail_path);
        g_free (theme);
}

GPtrArray *
zoitechat_gtk3_theme_service_discover (void)
{
        GPtrArray *themes = g_ptr_array_new_with_free_func ((GDestroyNotify)zoitechat_gtk3_theme_free);

        if (!removed_selected)
                g_ptr_array_add (themes, new_theme ("removed-theme", "Removed Theme", ZOITECHAT_GTK3_THEME_SOURCE_USER));
        g_ptr_array_add (themes, new_theme ("fallback-theme", "Fallback Theme", ZOITECHAT_GTK3_THEME_SOURCE_SYSTEM));
        return themes;
}

ZoitechatGtk3Theme *
zoitechat_gtk3_theme_find_by_id (const char *theme_id)
{
        (void)theme_id;
        return NULL;
}

gboolean
zoitechat_gtk3_theme_service_import (const char *source_path, char **imported_id, GError **error)
{
        (void)source_path;
        (void)imported_id;
        (void)error;
        return FALSE;
}

gboolean
zoitechat_gtk3_theme_service_read_archive_text_file (const char *archive_path, const char *name, char **contents, GError **error)
{
        (void)archive_path;
        (void)name;
        (void)contents;
        (void)error;
        return FALSE;
}

gboolean
zoitechat_gtk3_theme_service_remove_user_theme (const char *theme_id, GError **error)
{
        (void)error;
        if (g_strcmp0 (theme_id, "removed-theme") == 0)
        {
                removed_selected = TRUE;
                return TRUE;
        }
        return FALSE;
}

char *
zoitechat_gtk3_theme_pick_css_dir_for_minor (const char *theme_root, int preferred_minor)
{
        (void)theme_root;
        (void)preferred_minor;
        return NULL;
}

char *
zoitechat_gtk3_theme_pick_css_dir (const char *theme_root)
{
        (void)theme_root;
        return NULL;
}

GPtrArray *
zoitechat_gtk3_theme_build_inheritance_chain (const char *theme_root)
{
        (void)theme_root;
        return NULL;
}

gboolean
theme_gtk3_apply_current (GError **error)
{
        (void)error;
        apply_current_calls++;
        g_strlcpy (applied_theme_id, prefs.hex_gui_gtk3_theme, sizeof (applied_theme_id));
        applied_variant = (ThemeGtk3Variant)prefs.hex_gui_gtk3_variant;
        return TRUE;
}

void
theme_gtk3_init (void)
{
}

gboolean
theme_gtk3_apply (const char *theme_id, ThemeGtk3Variant variant, GError **error)
{
        (void)theme_id;
        (void)variant;
        (void)error;
        return TRUE;
}

ThemeGtk3Variant
theme_gtk3_variant_for_theme (const char *theme_id)
{
        if (g_str_has_suffix (theme_id, "dark"))
                return THEME_GTK3_VARIANT_PREFER_DARK;
        return THEME_GTK3_VARIANT_PREFER_LIGHT;
}

void
theme_gtk3_disable (void)
{
}

gboolean
theme_gtk3_is_active (void)
{
        return FALSE;
}

#include "../theme-preferences.c"

static void
test_removed_selected_theme_falls_back_to_system_default (void)
{
        GtkWidget *page;
        theme_preferences_ui *ui;
        struct zoitechatprefs setup_prefs;

        if (!gtk_available)
        {
                g_test_message ("GTK display not available");
                return;
        }

        memset (&setup_prefs, 0, sizeof (setup_prefs));
        memset (&prefs, 0, sizeof (prefs));
        g_strlcpy (prefs.hex_gui_gtk3_theme, "removed-theme", sizeof (prefs.hex_gui_gtk3_theme));
        prefs.hex_gui_gtk3_variant = THEME_GTK3_VARIANT_PREFER_DARK;
        removed_selected = FALSE;

        page = theme_preferences_create_page (NULL, &setup_prefs, NULL);
        ui = g_object_get_data (G_OBJECT (page), "theme-preferences-ui");
        g_assert_nonnull (ui);

        apply_current_calls = 0;
        applied_theme_id[0] = '\0';

        g_assert_nonnull (ui->gtk3_remove);
        gtk_button_clicked (GTK_BUTTON (ui->gtk3_remove));

        /* Removing the active in-app theme returns to the system GTK3
         * theme ("None") instead of forcing another theme. */
        g_assert_cmpstr (prefs.hex_gui_gtk3_theme, ==, "");
        g_assert_cmpstr (setup_prefs.hex_gui_gtk3_theme, ==, "");
        g_assert_cmpint (prefs.hex_gui_gtk3_variant, ==, THEME_GTK3_VARIANT_FOLLOW_SYSTEM);
        g_assert_cmpint (setup_prefs.hex_gui_gtk3_variant, ==, THEME_GTK3_VARIANT_FOLLOW_SYSTEM);
        g_assert_cmpint (apply_current_calls, ==, 1);
        g_assert_cmpstr (applied_theme_id, ==, "");
        g_assert_cmpint (applied_variant, ==, THEME_GTK3_VARIANT_FOLLOW_SYSTEM);

        gtk_widget_destroy (page);
}


static void
test_unset_theme_keeps_system_default_without_apply (void)
{
        GtkWidget *page;
        struct zoitechatprefs setup_prefs;

        if (!gtk_available)
        {
                g_test_message ("GTK display not available");
                return;
        }

        memset (&setup_prefs, 0, sizeof (setup_prefs));
        memset (&prefs, 0, sizeof (prefs));
        removed_selected = FALSE;
        apply_current_calls = 0;
        applied_theme_id[0] = '\0';
        prefs.hex_gui_gtk3_variant = THEME_GTK3_VARIANT_FOLLOW_SYSTEM;

        page = theme_preferences_create_page (NULL, &setup_prefs, NULL);

        g_assert_cmpstr (prefs.hex_gui_gtk3_theme, ==, "");
        g_assert_cmpstr (setup_prefs.hex_gui_gtk3_theme, ==, "");
        g_assert_cmpint (prefs.hex_gui_gtk3_variant, ==, THEME_GTK3_VARIANT_FOLLOW_SYSTEM);
        g_assert_cmpint (setup_prefs.hex_gui_gtk3_variant, ==, 0);
        g_assert_cmpint (apply_current_calls, ==, 0);

        gtk_widget_destroy (page);
}

static void
test_select_none_resets_theme_and_applies (void)
{
        GtkWidget *page;
        theme_preferences_ui *ui;
        struct zoitechatprefs setup_prefs;

        if (!gtk_available)
        {
                g_test_message ("GTK display not available");
                return;
        }

        memset (&setup_prefs, 0, sizeof (setup_prefs));
        memset (&prefs, 0, sizeof (prefs));
        g_strlcpy (prefs.hex_gui_gtk3_theme, "fallback-theme", sizeof (prefs.hex_gui_gtk3_theme));
        prefs.hex_gui_gtk3_variant = THEME_GTK3_VARIANT_PREFER_LIGHT;
        removed_selected = FALSE;
        apply_current_calls = 0;
        applied_theme_id[0] = '\0';

        page = theme_preferences_create_page (NULL, &setup_prefs, NULL);
        ui = g_object_get_data (G_OBJECT (page), "theme-preferences-ui");
        g_assert_nonnull (ui);

        gtk_combo_box_set_active (GTK_COMBO_BOX (ui->gtk3_combo), 0);

        g_assert_cmpstr (prefs.hex_gui_gtk3_theme, ==, "");
        g_assert_cmpstr (setup_prefs.hex_gui_gtk3_theme, ==, "");
        g_assert_cmpint (prefs.hex_gui_gtk3_variant, ==, THEME_GTK3_VARIANT_FOLLOW_SYSTEM);
        g_assert_cmpint (setup_prefs.hex_gui_gtk3_variant, ==, THEME_GTK3_VARIANT_FOLLOW_SYSTEM);
        g_assert_cmpint (apply_current_calls, ==, 1);
        g_assert_cmpstr (applied_theme_id, ==, "");
        g_assert_cmpint (applied_variant, ==, THEME_GTK3_VARIANT_FOLLOW_SYSTEM);

        gtk_widget_destroy (page);
}

static void
test_select_theme_does_not_pin_palette_tokens (void)
{
        GtkWidget *page;
        theme_preferences_ui *ui;
        struct zoitechatprefs setup_prefs;

        if (!gtk_available)
        {
                g_test_message ("GTK display not available");
                return;
        }

        memset (&setup_prefs, 0, sizeof (setup_prefs));
        memset (&prefs, 0, sizeof (prefs));
        removed_selected = FALSE;
        apply_current_calls = 0;
        set_token_color_calls = 0;

        page = theme_preferences_create_page (NULL, &setup_prefs, NULL);
        ui = g_object_get_data (G_OBJECT (page), "theme-preferences-ui");
        g_assert_nonnull (ui);

        /* 0 = None, 1 = removed-theme, 2 = fallback-theme */
        gtk_combo_box_set_active (GTK_COMBO_BOX (ui->gtk3_combo), 2);

        g_assert_cmpstr (prefs.hex_gui_gtk3_theme, ==, "fallback-theme");
        g_assert_cmpint (apply_current_calls, ==, 1);
        /* Selecting a GTK3 theme must leave the mapped palette tokens
         * unpinned so the user list, channel tree and input box keep
         * resolving their colors from the active theme instead of being
         * frozen until the next restart. */
        g_assert_cmpint (set_token_color_calls, ==, 0);

        gtk_widget_destroy (page);
}

static void
test_embedded_color_editor (void)
{
        GtkWidget *window, *editor;
        theme_color_manager_ui *ui;
        theme_color_manager_row *row;
        GError *error = NULL;
        if (!gtk_available)
        {
                g_test_message ("GTK display not available");
                return;
        }
        memset (&prefs, 0, sizeof prefs);
        theme_preferences_stage_begin ();
        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        editor = theme_preferences_create_color_editor (GTK_WINDOW (window), NULL);
        gtk_container_add (GTK_CONTAINER (window), editor);
        ui = g_object_get_data (G_OBJECT (editor), "zoitechat-theme-color-manager");
        row = g_ptr_array_index (ui->rows, 0);
        set_token_color_calls = 0;
        theme_preferences_manager_entry_commit (row);
        g_assert_true (theme_preferences_color_editor_stage (editor, &error));
        g_assert_no_error (error);
        theme_preferences_stage_apply ();
        g_assert_cmpint (set_token_color_calls, ==, 0);
        gtk_entry_set_text (GTK_ENTRY (row->entry), "#123456");
        g_assert_true (theme_preferences_color_editor_stage (editor, &error));
        g_assert_no_error (error);
        /* A typed edit previews its token, then Save reapplies that token. */
        g_assert_cmpint (set_token_color_calls, ==, 1);
        theme_preferences_stage_apply ();
        g_assert_cmpint (set_token_color_calls, ==, 2);
        gtk_entry_set_text (GTK_ENTRY (row->entry), "invalid");
        g_assert_false (theme_preferences_color_editor_stage (editor, &error));
        g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
        g_clear_error (&error);
        gtk_widget_show_all (window);
        gtk_button_clicked (GTK_BUTTON (row->button));
        gtk_widget_destroy (window);
        theme_preferences_stage_discard ();
}

static void
test_appearance_schemes_and_cancel (void)
{
        if (!gtk_available) { g_test_message ("GTK display not available"); return; }
        memset (&prefs, 0, sizeof prefs); memset (custom_tokens, 0, sizeof custom_tokens);
        GdkRGBA original, restored;
        gdk_rgba_parse (&original, "#345678");
        theme_runtime_user_set_color (THEME_TOKEN_TEXT_FOREGROUND, &original);
        theme_preferences_stage_begin ();
        struct zoitechatprefs staged = prefs;
        GtkWidget *page = theme_preferences_create_page (NULL, &staged, NULL);
        theme_preferences_ui *appearance = g_object_get_data (G_OBJECT (page), "theme-preferences-ui");
        theme_color_manager_ui *ui = g_object_get_data (G_OBJECT (appearance->color_editor), "zoitechat-theme-color-manager");
        gtk_button_clicked (GTK_BUTTON (ui->scheme_buttons[2]));
        g_assert_true (theme_preferences_stage.changed);
        g_assert_true (theme_preferences_stage.staged_custom[THEME_TOKEN_TEXT_BACKGROUND]);
        gtk_button_clicked (GTK_BUTTON (appearance->style_buttons[2]));
        g_assert_cmpint (prefs.hex_gui_dark_mode, ==, ZOITECHAT_DARK_MODE_DARK);
        theme_preferences_stage_discard ();
        g_assert_cmpint (prefs.hex_gui_dark_mode, ==, ZOITECHAT_DARK_MODE_AUTO);
        g_assert_true (theme_runtime_get_user_color (THEME_TOKEN_TEXT_FOREGROUND, &restored));
        g_assert_true (gdk_rgba_equal (&original, &restored));
        for (int token = 0; token < THEME_TOKEN_COUNT; token++)
                if (token != THEME_TOKEN_TEXT_FOREGROUND) g_assert_false (custom_tokens[token]);
        gtk_widget_destroy (page);
}

static void
test_individual_reset_preserves_other_overrides (void)
{
        if (!gtk_available) { g_test_message ("GTK display not available"); return; }
        memset (&prefs, 0, sizeof prefs); memset (custom_tokens, 0, sizeof custom_tokens);
        theme_preferences_stage_begin ();
        GtkWidget *editor = theme_preferences_create_color_editor (NULL, NULL);
        theme_color_manager_ui *ui = g_object_get_data (G_OBJECT (editor), "zoitechat-theme-color-manager");
        gtk_button_clicked (GTK_BUTTON (ui->scheme_buttons[2]));
        theme_color_manager_row *row = g_ptr_array_index (ui->rows, THEME_TOKEN_TEXT_BACKGROUND);
        gtk_button_clicked (GTK_BUTTON (row->reset));
        g_assert_false (theme_preferences_stage.staged_custom[THEME_TOKEN_TEXT_BACKGROUND]);
        g_assert_true (theme_preferences_stage.staged_custom[THEME_TOKEN_TEXT_FOREGROUND]);
        g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (row->status)), ==, "Theme");
        GdkRGBA light, dark;
        theme_preferences_staged_get_color (THEME_TOKEN_TEXT_BACKGROUND, &light);
        prefs.hex_gui_dark_mode = ZOITECHAT_DARK_MODE_DARK;
        theme_preferences_staged_get_color (THEME_TOKEN_TEXT_BACKGROUND, &dark);
        g_assert_false (gdk_rgba_equal (&light, &dark));
        theme_preferences_stage_commit ();
        g_assert_false (custom_tokens[THEME_TOKEN_TEXT_BACKGROUND]);
        g_assert_true (custom_tokens[THEME_TOKEN_TEXT_FOREGROUND]);
        gtk_widget_destroy (editor);
}

static void
test_contrast_suggestion_and_preview_navigation (void)
{
        if (!gtk_available) { g_test_message ("GTK display not available"); return; }
        memset (&prefs, 0, sizeof prefs); memset (custom_tokens, 0, sizeof custom_tokens);
        theme_preferences_stage_begin ();
        GtkWidget *editor = theme_preferences_create_color_editor (NULL, NULL);
        theme_color_manager_ui *ui = g_object_get_data (G_OBJECT (editor), "zoitechat-theme-color-manager");
        theme_color_manager_row *row = g_ptr_array_index (ui->rows, THEME_TOKEN_TEXT_FOREGROUND);
        GdkRGBA white, black, gray;
        gdk_rgba_parse (&white, "#ffffff"); gdk_rgba_parse (&black, "#000000"); gdk_rgba_parse (&gray, "#dddddd");
        g_assert_cmpfloat_with_epsilon (theme_preferences_contrast_ratio (&white, &black), 21.0, 0.001);
        theme_preferences_manager_row_commit (row, &gray);
        g_assert_true (gtk_widget_get_visible (ui->contrast_fix));
        gtk_button_clicked (GTK_BUTTON (ui->contrast_fix));
        theme_preferences_staged_get_color (THEME_TOKEN_TEXT_FOREGROUND, &gray);
        g_assert_cmpfloat (theme_preferences_contrast_ratio (&gray, &white), >=, 4.5);
        theme_preferences_manager_focus_color (ui, THEME_TOKEN_MIRC_12);
        g_assert_true (gtk_expander_get_expanded (GTK_EXPANDER (ui->advanced)));
        gtk_entry_set_text (GTK_ENTRY (row->entry), "invalid");
        theme_preferences_manager_entry_commit (row);
        g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (row->entry)), ==, "invalid");
        GError *error = NULL;
        g_assert_false (theme_preferences_color_editor_stage (editor, &error));
        g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL); g_clear_error (&error);
        theme_preferences_stage_discard (); gtk_widget_destroy (editor);
}

static void
test_picker_cancel_restores_inheritance (void)
{
        if (!gtk_available) { g_test_message ("GTK display not available"); return; }
        memset (&prefs, 0, sizeof prefs); memset (custom_tokens, 0, sizeof custom_tokens);
        theme_preferences_stage_begin ();
        GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        GtkWidget *editor = theme_preferences_create_color_editor (GTK_WINDOW (window), NULL);
        gtk_container_add (GTK_CONTAINER (window), editor);
        theme_color_manager_ui *ui = g_object_get_data (G_OBJECT (editor), "zoitechat-theme-color-manager");
        theme_color_manager_row *row = g_ptr_array_index (ui->rows, THEME_TOKEN_MIRC_12);
        gtk_button_clicked (GTK_BUTTON (row->button));
        GList *windows = gtk_window_list_toplevels ();
        GtkWidget *picker = NULL;
        for (GList *i = windows; i; i = i->next) if (GTK_IS_COLOR_CHOOSER_DIALOG (i->data)) picker = i->data;
        g_assert_nonnull (picker);
        GdkRGBA color; gdk_rgba_parse (&color, "#aabbcc");
        gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (picker), &color);
        g_assert_true (theme_preferences_stage.staged_custom[row->token]);
        gtk_dialog_response (GTK_DIALOG (picker), GTK_RESPONSE_CANCEL);
        g_assert_false (theme_preferences_stage.staged_custom[row->token]);
        g_assert_false (custom_tokens[row->token]);
        g_list_free (windows);
        theme_preferences_stage_discard (); gtk_widget_destroy (window);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);
        gtk_available = gtk_init_check (&argc, &argv);
        g_test_add_func ("/theme/preferences/gtk3_removed_selection_falls_back_to_system",
                         test_removed_selected_theme_falls_back_to_system_default);
        g_test_add_func ("/theme/preferences/gtk3_unset_keeps_system_default",
                         test_unset_theme_keeps_system_default_without_apply);
        g_test_add_func ("/theme/preferences/gtk3_select_none_resets_theme",
                         test_select_none_resets_theme_and_applies);
        g_test_add_func ("/theme/preferences/gtk3_select_theme_does_not_pin_palette_tokens",
                         test_select_theme_does_not_pin_palette_tokens);
        g_test_add_func ("/theme/preferences/embedded_color_editor", test_embedded_color_editor);
        g_test_add_func ("/theme/preferences/appearance_schemes_cancel", test_appearance_schemes_and_cancel);
        g_test_add_func ("/theme/preferences/individual_reset_inherits", test_individual_reset_preserves_other_overrides);
        g_test_add_func ("/theme/preferences/contrast_and_navigation", test_contrast_suggestion_and_preview_navigation);
        g_test_add_func ("/theme/preferences/picker_cancel_inherits", test_picker_cancel_restores_inheritance);
        return g_test_run ();
}
