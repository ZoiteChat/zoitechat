#include "../../fe-gtk.h"

#include "../../../common/zoitechat.h"
#include "../../../common/zoitechatc.h"
#include "../../../common/gtk3-theme-service.h"
#include "../theme-gtk3.h"
#include "../theme-manager.h"

struct session *current_sess;
struct session *current_tab;
struct zoitechatprefs prefs;
InputStyle *input_style;

static gboolean gtk_available;
static int apply_current_calls;
static char applied_theme_id[256];
static ThemeGtk3Variant applied_variant;
static gboolean removed_selected;

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

gboolean
theme_get_color (ThemeSemanticToken token, GdkRGBA *color)
{
        (void)token;
        if (color)
                gdk_rgba_parse (color, "#000000");
        return TRUE;
}

void
theme_manager_set_token_color (unsigned int dark_mode, ThemeSemanticToken token, const GdkRGBA *color, gboolean *changed)
{
        (void)dark_mode;
        (void)token;
        (void)color;
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
theme_manager_save_preferences (void)
{
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
test_removed_selected_theme_commits_fallback_and_applies (void)
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
        apply_current_calls = 0;
        applied_theme_id[0] = '\0';

        page = theme_preferences_create_page (NULL, &setup_prefs, NULL);
        ui = g_object_get_data (G_OBJECT (page), "theme-preferences-ui");
        g_assert_nonnull (ui);

        g_assert_nonnull (ui->gtk3_remove);
        gtk_button_clicked (GTK_BUTTON (ui->gtk3_remove));

        g_assert_cmpstr (prefs.hex_gui_gtk3_theme, ==, "fallback-theme");
        g_assert_cmpstr (setup_prefs.hex_gui_gtk3_theme, ==, "fallback-theme");
        g_assert_cmpint (prefs.hex_gui_gtk3_variant, ==, THEME_GTK3_VARIANT_PREFER_LIGHT);
        g_assert_cmpint (setup_prefs.hex_gui_gtk3_variant, ==, THEME_GTK3_VARIANT_PREFER_LIGHT);
        g_assert_cmpint (apply_current_calls, ==, 1);
        g_assert_cmpstr (applied_theme_id, ==, "fallback-theme");
        g_assert_cmpint (applied_variant, ==, THEME_GTK3_VARIANT_PREFER_LIGHT);

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

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);
        gtk_available = gtk_init_check (&argc, &argv);
        g_test_add_func ("/theme/preferences/gtk3_removed_selection_applies_fallback",
                         test_removed_selected_theme_commits_fallback_and_applies);
        g_test_add_func ("/theme/preferences/gtk3_unset_keeps_system_default",
                         test_unset_theme_keeps_system_default_without_apply);
        return g_test_run ();
}
