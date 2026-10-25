/* Native preferences editor regressions. GPL-2.0-or-later. */
#include "../../common/zoitechat.h"
#include <glib/gstdio.h>
#include "../../common/fe.h"
#include "../../common/util.h"
#include "../preferences-pages.h"
#include "../theme/theme-access.h"
#include "../notifications/notification-backend.h"

static gboolean gtk_available, notifications_available;
static int notification_calls, beep_calls, file_calls, sound_changes;
static char *played_file, *opened_page;
static char *opened_folder, *preview_mask;

/* External effects are recorded; tests drive the real native page widgets. */
int notification_backend_supported (void) { return notifications_available; }
void notification_backend_show (const char *title, const char *text) { notification_calls++; }
void fe_beep (session *sess) { beep_calls++; }
void sound_play (const char *file, gboolean quiet)
{
        file_calls++;
        g_free (played_file);
        played_file = g_strdup (file);
}
void setup_open_page (const char *id)
{
        g_free (opened_page);
        opened_page = g_strdup (id);
}
void setup_reveal_widget (GtkWidget *widget) { }
void fe_open_url (const char *url)
{
        g_free (opened_folder);
        opened_folder = g_strdup (url);
}
char *log_build_pathname (const char *mask, const char *config_dir,
                         const char *server, const char *channel,
                         const char *network, time_t now)
{
        /* Record the resolver input and return a sentinel path, so these UI
         * tests verify the displayed result comes from the resolver. */
        g_free (preview_mask);
        preview_mask = g_strdup (mask);
        return g_build_filename (config_dir, "resolved-example.log", NULL);
}
gboolean theme_get_color (ThemeSemanticToken token, GdkRGBA *rgba)
{
        return gdk_rgba_parse (rgba, token == THEME_TOKEN_TEXT_BACKGROUND ? "#20242a" : "#eeeeee");
}
int get_stamp_str (char *format, time_t stamp, char **result)
{
        GDateTime *date = g_date_time_new_from_unix_local (stamp);
        *result = g_date_time_format (date, format);
        g_date_time_unref (date);
        return *result ? strlen (*result) : 0;
}

static void changed (gpointer data) { sound_changes++; }

static GtkWidget *
find_widget (GtkWidget *root, const char *name)
{
        GtkWidget *found = NULL;
        if (!strcmp (gtk_widget_get_name (root), name)) return root;
        if (GTK_IS_BUTTON (root) && !g_strcmp0 (gtk_button_get_label (GTK_BUTTON (root)), name)) return root;
        if (!GTK_IS_CONTAINER (root)) return NULL;
        GList *children = gtk_container_get_children (GTK_CONTAINER (root));
        for (GList *i = children; i && !found; i = i->next) found = find_widget (i->data, name);
        g_list_free (children);
        return found;
}

static GtkWidget *control (GtkWidget *root, const char *name)
{
        GtkWidget *widget = find_widget (root, name);
        g_assert_nonnull (widget);
        return widget;
}

static void
destroy_page (GtkWidget *page)
{
        g_object_ref_sink (page);
        gtk_widget_destroy (page);
        g_object_unref (page);
}

static void
defaults (struct zoitechatprefs *prefs)
{
        memset (prefs, 0, sizeof *prefs);
        g_strlcpy (prefs->hex_text_font, "Monospace 11", sizeof prefs->hex_text_font);
#ifdef WIN32
        g_strlcpy (prefs->hex_text_font_main, "Monospace 11", sizeof prefs->hex_text_font_main);
#endif
        g_strlcpy (prefs->hex_stamp_text_format, "[%H:%M:%S] ", sizeof prefs->hex_stamp_text_format);
        prefs->hex_gui_tab_layout = 2;
        prefs->hex_gui_tab_pos = 3;
        prefs->hex_text_indent = 1;
        prefs->hex_text_show_marker = 1;
        prefs->hex_stamp_text = 1;
}

static void
test_open_preserves_values (void)
{
        struct zoitechatprefs prefs, original;
        GtkWidget *page;
        char *sound = NULL;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        /* Unknown legacy values and formats must survive merely visiting a page. */
        prefs.hex_gui_tab_layout = 1;
        prefs.hex_gui_tab_pos = 7;
        prefs.hex_gui_tab_trunc = 123;
        prefs.hex_gui_tab_newtofront = 9;
        prefs.hex_text_wrap_align = 9;
        g_strlcpy (prefs.hex_stamp_text_format, "时刻 %H:%M  ", sizeof prefs.hex_stamp_text_format);
        original = prefs;
        page = preferences_text_page_new (&prefs, NULL);
        g_assert_cmpmem (&prefs, sizeof prefs, &original, sizeof original);
        destroy_page (page);
        page = preferences_tabs_page_new (&prefs);
        g_assert_cmpmem (&prefs, sizeof prefs, &original, sizeof original);
        destroy_page (page);
        page = preferences_notifications_page_new (&prefs, 0, &sound, changed, NULL);
        g_assert_cmpmem (&prefs, sizeof prefs, &original, sizeof original);
        g_assert_null (sound);
        destroy_page (page);
}

static void
test_timestamp_and_dependencies (void)
{
        struct zoitechatprefs prefs;
        GtkWidget *page, *style, *format;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        g_strlcpy (prefs.hex_stamp_text_format, "Custom %H:%M  ", sizeof prefs.hex_stamp_text_format);
        page = preferences_text_page_new (&prefs, NULL);
        style = control (page, "timestamp-style"); format = control (page, "timestamp-format");
        g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (style)), ==, "custom");
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (style), "%H:%M ");
        g_assert_cmpstr (prefs.hex_stamp_text_format, ==, "%H:%M ");
        g_assert_false (gtk_widget_is_sensitive (format));
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (style), "custom");
        g_assert_cmpstr (prefs.hex_stamp_text_format, ==, "Custom %H:%M  ");
        gtk_entry_set_text (GTK_ENTRY (format), "[%H:%M] ");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "timestamps")), FALSE);
        g_assert_false (gtk_widget_is_sensitive (format));
        g_assert_cmpstr (prefs.hex_stamp_text_format, ==, "[%H:%M] ");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "indent-nicks")), FALSE);
        g_assert_true (gtk_widget_is_sensitive (control (page, "wrap-alignment")));
        gtk_combo_box_set_active (GTK_COMBO_BOX (control (page, "wrap-alignment")), 2);
        g_assert_cmpint (prefs.hex_text_wrap_align, ==, 2);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "indent-nicks")), TRUE);
        g_assert_false (gtk_widget_is_sensitive (control (page, "wrap-alignment")));
        g_assert_cmpint (prefs.hex_text_wrap_align, ==, 2);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "inline-modes")), TRUE);
        g_assert_false (gtk_widget_is_sensitive (control (page, "multiline-topic")));
        destroy_page (page);
}

static void
test_preview_updates (void)
{
        struct zoitechatprefs prefs;
        GtkWidget *page;
        GtkTextBuffer *buffer;
        GtkTextIter start, end;
        char *text;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        page = preferences_text_page_new (&prefs, NULL);
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (control (page, "conversation-preview")));
        gtk_text_buffer_get_bounds (buffer, &start, &end);
        text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
        g_assert_nonnull (strstr (text, "[14:35:08]"));
        g_assert_nonnull (strstr (text, "Unread messages"));
        g_free (text);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "timestamps")), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "unread-marker")), FALSE);
        gtk_text_buffer_get_bounds (buffer, &start, &end);
        text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
        g_assert_null (strstr (text, "14:35:08"));
        g_assert_null (strstr (text, "Unread messages"));
        g_free (text);
        destroy_page (page);
}

static void
test_layout_switching (void)
{
        struct zoitechatprefs prefs;
        GtkWidget *page, *tree_control, *tab_control;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        prefs.hex_gui_tab_closebuttons = 1;
        prefs.hex_gui_tab_dots = 1;
        page = preferences_tabs_page_new (&prefs);
        tree_control = control (page, "tree-dots"); tab_control = control (page, "tab-close-buttons");
        g_assert_true (gtk_widget_is_visible (tree_control));
        g_assert_false (gtk_widget_is_visible (tab_control));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "layout-tabs")), TRUE);
        g_assert_cmpint (prefs.hex_gui_tab_layout, ==, 0);
        g_assert_cmpint (prefs.hex_gui_tab_pos, ==, 6);
        g_assert_true (gtk_widget_is_visible (tab_control));
        g_assert_false (gtk_widget_is_visible (tree_control));
        g_assert_cmpint (prefs.hex_gui_tab_dots, ==, 1);
        gtk_combo_box_set_active (GTK_COMBO_BOX (control (page, "channel-placement")), 4);
        g_assert_cmpint (prefs.hex_gui_tab_pos, ==, 5);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "layout-tree")), TRUE);
        g_assert_cmpint (prefs.hex_gui_tab_pos, ==, 3);
        g_assert_cmpint (prefs.hex_gui_tab_closebuttons, ==, 1);
        destroy_page (page);
}

static void
test_notification_mapping (void)
{
        struct zoitechatprefs prefs;
        GtkWidget *page;
        char *sound = NULL;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        page = preferences_notifications_page_new (&prefs, 31, &sound, changed, NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "mention-desktop")), TRUE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "private-beep")), TRUE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "channel-tray")), TRUE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "private-attention")), TRUE);
        g_assert_cmpint (prefs.hex_input_balloon_hilight, ==, 1);
        g_assert_cmpint (prefs.hex_input_balloon_chans, ==, 0);
        g_assert_cmpint (prefs.hex_input_beep_priv, ==, 1);
        g_assert_cmpint (prefs.hex_input_beep_hilight, ==, 0);
        g_assert_cmpint (prefs.hex_input_tray_chans, ==, 1);
        g_assert_cmpint (prefs.hex_input_tray_priv, ==, 0);
        g_assert_cmpint (prefs.hex_input_flash_priv, ==, 1);
        g_assert_false (gtk_widget_is_sensitive (control (page, "tray-close")));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "tray-enabled")), TRUE);
        g_assert_true (gtk_widget_is_sensitive (control (page, "tray-close")));
        destroy_page (page);
}

static void
test_shared_sound_and_effects (void)
{
        struct zoitechatprefs prefs;
        GtkWidget *page;
        char *sound = g_strdup ("original.wav");
        if (!gtk_available) { g_free (sound); g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        page = preferences_notifications_page_new (&prefs, 31, &sound, changed, NULL);
        sound_changes = beep_calls = file_calls = 0;
        gtk_entry_set_text (GTK_ENTRY (control (page, "alert-sound-file")), "new.wav");
        g_assert_cmpstr (sound, ==, "new.wav");
        g_assert_cmpint (sound_changes, ==, 1);
        gtk_button_clicked (GTK_BUTTON (control (page, "Play")));
        g_assert_cmpstr (played_file, ==, "new.wav");
        gtk_button_clicked (GTK_BUTTON (control (page, "Use system sound")));
        g_assert_cmpstr (sound, ==, "");
        gtk_button_clicked (GTK_BUTTON (control (page, "Play")));
        g_assert_cmpint (beep_calls, ==, 1);
        g_free (sound); sound = g_strdup ("changed-in-sounds.wav");
        preferences_notifications_refresh_sound (page);
        g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (control (page, "alert-sound-file"))), ==, sound);
        g_assert_cmpint (sound_changes, ==, 2);
        gtk_button_clicked (GTK_BUTTON (control (page, "open-event-sounds")));
        g_assert_cmpstr (opened_page, ==, "sounds");
        destroy_page (page);
        g_free (sound);
}

static void
test_notification_test_safety (void)
{
        struct zoitechatprefs prefs, original;
        GtkWidget *page;
        char *sound = NULL;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        prefs.hex_away_omit_alerts = prefs.hex_gui_focus_omitalerts = 1;
        original = prefs;
        notifications_available = TRUE; notification_calls = 0;
        page = preferences_notifications_page_new (&prefs, 31, &sound, changed, NULL);
        gtk_button_clicked (GTK_BUTTON (control (page, "notification-test")));
        g_assert_cmpint (notification_calls, ==, 1);
        g_assert_nonnull (strstr (gtk_label_get_text (GTK_LABEL (control (page, "notification-test-status"))), "marked away"));
        g_assert_cmpmem (&prefs, sizeof prefs, &original, sizeof original);
        notifications_available = FALSE;
        gtk_button_clicked (GTK_BUTTON (control (page, "notification-test")));
        g_assert_cmpint (notification_calls, ==, 1);
        destroy_page (page);
        page = preferences_notifications_page_new (&prefs, 0, &sound, changed, NULL);
        g_assert_false (gtk_widget_is_sensitive (control (page, "notification-test")));
        g_assert_false (gtk_widget_is_sensitive (control (page, "private-desktop")));
        g_assert_null (find_widget (page, "private-tray"));
        g_assert_null (find_widget (page, "private-attention"));
        g_assert_cmpmem (&prefs, sizeof prefs, &original, sizeof original);
        destroy_page (page);
}

static void
test_history_preserves_values (void)
{
        struct zoitechatprefs prefs, original;
        GtkWidget *page;
        char *config, *history;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        config = g_dir_make_tmp ("zoitechat-history-ui-XXXXXX", NULL);
        history = g_build_filename (config, "scrollback", NULL);
        g_strlcpy (prefs.hex_irc_logmask, "custom-%s-%Y-%m.log", sizeof prefs.hex_irc_logmask);
        g_strlcpy (prefs.hex_stamp_log_format, "Custom %H:%M  ", sizeof prefs.hex_stamp_log_format);
        prefs.hex_text_max_lines = 120000;
        prefs.hex_url_grabber_limit = 12000;
        original = prefs;
        page = preferences_history_page_new (&prefs, config);
        g_assert_cmpmem (&prefs, sizeof prefs, &original, sizeof original);
        g_assert_false (g_file_test (history, G_FILE_TEST_EXISTS));
        g_assert_false (gtk_widget_is_sensitive (control (page, "open-history-folder")));
        g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (control (page, "log-file-style"))), ==, "custom");
        char *expected = g_build_filename (config, "resolved-example.log", NULL);
        g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (control (page, "log-example-file"))), ==, expected);
        g_free (expected);
        destroy_page (page);
        g_rmdir (config);
        g_free (config); g_free (history);
}

static void
test_history_presets_and_custom (void)
{
        struct zoitechatprefs prefs;
        GtkWidget *page, *combo;
        char *mask, *expected;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs); prefs.hex_irc_logging = 1;
        mask = g_build_filename (g_get_tmp_dir (), "100%%done", "%s-custom.log", NULL);
        g_strlcpy (prefs.hex_irc_logmask, mask, sizeof prefs.hex_irc_logmask);
        page = preferences_history_page_new (&prefs, g_get_tmp_dir ());
        combo = control (page, "log-file-style");
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);
        expected = g_build_filename (g_get_tmp_dir (), "100%%done", "%n", "%Y-%m", "%c.log", NULL);
        g_assert_cmpstr (prefs.hex_irc_logmask, ==, expected);
        g_assert_cmpstr (preview_mask, ==, expected);
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "custom");
        g_assert_cmpstr (prefs.hex_irc_logmask, ==, mask);
        gtk_button_clicked (GTK_BUTTON (control (page, "default-log-folder")));
        g_assert_cmpstr (prefs.hex_irc_logmask, ==, "%s-custom.log");
        g_free (expected); g_free (mask);
        destroy_page (page);
}

static void
test_history_dependencies_and_timestamps (void)
{
        struct zoitechatprefs prefs;
        GtkWidget *page, *combo;
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs);
        g_strlcpy (prefs.hex_irc_logmask, "%n" G_DIR_SEPARATOR_S "%c.log", sizeof prefs.hex_irc_logmask);
        g_strlcpy (prefs.hex_stamp_log_format, "Time %H:%M  ", sizeof prefs.hex_stamp_log_format);
        prefs.hex_stamp_log = 1;
        prefs.hex_url_logging = 1;
        prefs.hex_url_grabber_limit = 91;
        page = preferences_history_page_new (&prefs, g_get_tmp_dir ());
        g_assert_false (gtk_widget_is_sensitive (control (page, "choose-log-folder")));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "save-logs")), TRUE);
        g_assert_true (gtk_widget_is_sensitive (control (page, "choose-log-folder")));
        combo = control (page, "log-timestamp-style");
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "%H:%M ");
        g_assert_cmpstr (prefs.hex_stamp_log_format, ==, "%H:%M ");
        g_assert_nonnull (strstr (gtk_label_get_text (GTK_LABEL (control (page, "log-example-line"))), "14:35"));
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "custom");
        g_assert_cmpstr (prefs.hex_stamp_log_format, ==, "Time %H:%M  ");
        g_assert_cmpstr (prefs.hex_stamp_text_format, ==, "[%H:%M:%S] ");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "log-timestamps")), FALSE);
        g_assert_false (gtk_widget_is_sensitive (control (page, "log-timestamp-format")));
        g_assert_cmpstr (prefs.hex_stamp_log_format, ==, "Time %H:%M  ");
        g_assert_false (gtk_widget_is_sensitive (control (page, "link-limit")));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "collect-links")), TRUE);
        g_assert_true (gtk_widget_is_sensitive (control (page, "link-limit")));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "collect-links")), FALSE);
        g_assert_cmpint (prefs.hex_url_grabber_limit, ==, 91);
        g_assert_cmpint (prefs.hex_url_logging, ==, 1);
        gtk_button_clicked (GTK_BUTTON (control (page, "open-data-folder")));
        g_assert_cmpstr (opened_folder, ==, g_get_tmp_dir ());
        destroy_page (page);
}

static void
test_history_validation (void)
{
        struct zoitechatprefs prefs;
        GtkWidget *page;
        GError *error = NULL;
        GString *long_format = g_string_new ("");
        if (!gtk_available) { g_string_free (long_format, TRUE); g_test_skip ("GTK display unavailable"); return; }
        defaults (&prefs); prefs.hex_irc_logging = 1;
        g_strlcpy (prefs.hex_irc_logmask, "old-custom.log", sizeof prefs.hex_irc_logmask);
        page = preferences_history_page_new (&prefs, g_get_tmp_dir ());
        g_assert_true (preferences_history_page_stage (page, &error));
        gtk_entry_set_text (GTK_ENTRY (control (page, "log-file-format")), "");
        g_assert_false (preferences_history_page_stage (page, &error));
        g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL); g_clear_error (&error);
        for (int i = 0; i < 140; i++) g_string_append (long_format, "é");
        gtk_entry_set_text (GTK_ENTRY (control (page, "log-file-format")), long_format->str);
        g_assert_false (preferences_history_page_stage (page, &error));
        g_clear_error (&error);
        gtk_entry_set_text (GTK_ENTRY (control (page, "log-file-format")), "corrected-%Y.log");
        g_assert_true (preferences_history_page_stage (page, &error));
        g_assert_no_error (error);
        destroy_page (page);
        g_string_free (long_format, TRUE);
}

static GtkWidget *
language_button (GtkWidget *root, const char *code)
{
        GtkWidget *found = NULL;
        if (!g_strcmp0 (g_object_get_data (G_OBJECT (root), "language-code"), code)) return root;
        if (!GTK_IS_CONTAINER (root)) return NULL;
        GList *children = gtk_container_get_children (GTK_CONTAINER (root));
        for (GList *i = children; i && !found; i = i->next) found = language_button (i->data, code);
        g_list_free (children); return found;
}

static void
test_typing_preservation_and_languages (void)
{
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        struct zoitechatprefs prefs, original;
        defaults (&prefs);
        prefs.hex_gui_input_spell = prefs.hex_gui_input_icon = 1;
        prefs.hex_completion_amount = 5;
        g_strlcpy (prefs.hex_text_spell_langs, "en_US, missing_XX", sizeof prefs.hex_text_spell_langs);
        g_strlcpy (prefs.hex_completion_suffix, ",", sizeof prefs.hex_completion_suffix);
        original = prefs;
        PreferencesSpellLanguage languages[] = {{"en_US", "English (United States)"}, {"en_CA", "English (Canada)"}};
        GtkWidget *page = preferences_typing_page_new (&prefs, gtk_entry_new (), languages, 2, NULL);
        g_assert_cmpmem (&prefs, sizeof prefs, &original, sizeof original);
        g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (language_button (page, "en_US"))));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (language_button (page, "en_CA")), TRUE);
        g_assert_nonnull (strstr (prefs.hex_text_spell_langs, "missing_XX"));
        g_assert_nonnull (strstr (prefs.hex_text_spell_langs, "en_CA"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (language_button (page, "en_US")), FALSE);
        g_assert_null (strstr (prefs.hex_text_spell_langs, "en_US"));
        g_assert_nonnull (strstr (prefs.hex_text_spell_langs, "missing_XX"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "spelling-enabled")), FALSE);
        g_assert_false (gtk_widget_is_sensitive (language_button (page, "en_CA")));
        g_assert_nonnull (strstr (prefs.hex_text_spell_langs, "en_CA"));
        g_assert_false (gtk_widget_is_sensitive (control (page, "input-mode-icon")));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control (page, "input-nickname")), TRUE);
        g_assert_true (gtk_widget_is_sensitive (control (page, "input-mode-icon")));
        destroy_page (page);
}

static void
test_typing_completion_and_validation (void)
{
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        struct zoitechatprefs prefs;
        defaults (&prefs); prefs.hex_completion_amount = 5;
        GtkWidget *page = preferences_typing_page_new (&prefs, gtk_entry_new (), NULL, 0, NULL);
        GtkWidget *result = control (page, "completion-example-input");
        gtk_entry_set_text (GTK_ENTRY (result), "Al");
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (control (page, "completion-limit")), 0);
        gtk_entry_set_text (GTK_ENTRY (control (page, "completion-suffix")), "✓");
        g_assert_cmpstr (prefs.hex_completion_suffix, ==, "✓");
        GError *error = NULL;
        g_assert_true (preferences_typing_page_stage (page, &error));
        gtk_entry_set_text (GTK_ENTRY (control (page, "completion-suffix")), "😀");
        g_assert_false (preferences_typing_page_stage (page, &error));
        g_assert_cmpstr (prefs.hex_completion_suffix, ==, "✓");
        g_clear_error (&error);
        gtk_entry_set_text (GTK_ENTRY (control (page, "completion-suffix")), ":");
        char *too_long = g_strnfill (64, 'a');
        gtk_entry_set_text (GTK_ENTRY (control (page, "spelling-codes")), too_long);
        g_assert_false (preferences_typing_page_stage (page, &error));
        g_clear_error (&error); g_free (too_long);
        gtk_entry_set_text (GTK_ENTRY (control (page, "spelling-codes")), "unavailable-code");
        g_assert_true (preferences_typing_page_stage (page, &error));
        g_assert_no_error (error);
        destroy_page (page);
}

static GdkModifierType test_modifiers (GdkModifierType mods)
{
        return mods & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_META_MASK);
}
static void test_restore (gpointer data) { }

static GtkWidget *
shortcut_fixture (GtkListStore **store)
{
        *store = gtk_list_store_new (6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
        GtkTreeIter iter;
        for (int i = 0; i < 3; i++)
        {
                gtk_list_store_append (*store, &iter);
                gtk_list_store_set (*store, &iter, 0, "Ctrl+K", 1, "<Control>k", 2, i == 1 ? "Change Page" : "Run Command",
                        3, i == 0 ? "\002" : i == 1 ? "2" : "echo original\\nsecond", 4, i == 1 ? "Relative-custom-value" : NULL, 5, i != 0, -1);
        }
        PreferencesShortcutAction actions[] = {{"Run Command", "Run a command"}, {"Change Page", "Change a page"}};
        return preferences_shortcuts_page_new (*store, actions, 2, test_modifiers, test_restore, NULL);
}

static void
test_shortcuts_filter_and_protection (void)
{
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        GtkListStore *store;
        GtkWidget *page = shortcut_fixture (&store);
        GtkTreeIter iter;
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
        char *data;
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 3, &data, -1);
        g_assert_cmpstr (data, ==, "\002"); g_free (data);
        g_assert_false (gtk_widget_get_sensitive (control (page, "shortcut-action")));
        gtk_button_clicked (GTK_BUTTON (control (page, "shortcut-remove")));
        g_assert_cmpint (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL), ==, 3);
        g_assert_nonnull (strstr (gtk_label_get_text (GTK_LABEL (control (page, "shortcut-shared"))), "3 bindings"));
        gtk_entry_set_text (GTK_ENTRY (control (page, "shortcut-search")), "Relative-custom-value");
        g_signal_emit_by_name (control (page, "shortcut-search"), "search-changed");
        GtkTreeModel *filtered = gtk_tree_view_get_model (GTK_TREE_VIEW (control (page, "shortcuts-list")));
        g_assert_cmpint (gtk_tree_model_iter_n_children (filtered, NULL), ==, 1);
        g_assert_cmpint (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL), ==, 3);
        GtkTreePath *path = gtk_tree_path_new_from_indices (1, -1);
        preferences_shortcuts_reveal_path (page, path); gtk_tree_path_free (path);
        g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (control (page, "shortcut-data2"))), ==, "Relative-custom-value");
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (control (page, "shortcut-action")), "Run Command");
        gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter, NULL, 1);
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 4, &data, -1);
        g_assert_cmpstr (data, ==, "Relative-custom-value"); g_free (data);
        destroy_page (page); g_object_unref (store);
}

static void
test_shortcuts_duplicate_and_record (void)
{
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        GtkListStore *store;
        GtkWidget *page = shortcut_fixture (&store);
        gtk_button_clicked (GTK_BUTTON (control (page, "shortcut-duplicate")));
        g_assert_cmpint (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL), ==, 4);
        gtk_entry_set_text (GTK_ENTRY (control (page, "shortcut-data1")), "echo changed\\nnext");
        GtkTreeIter iter; char *data;
        gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter, NULL, 3);
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 3, &data, -1);
        g_assert_cmpstr (data, ==, "echo changed\\nnext"); g_free (data);
        gtk_button_clicked (GTK_BUTTON (control (page, "shortcut-record")));
        GtkWidget *dialog = NULL;
        GList *tops = gtk_window_list_toplevels ();
        for (GList *i = tops; i; i = i->next) if (!strcmp (gtk_widget_get_name (i->data), "shortcut-recorder")) dialog = i->data;
        g_list_free (tops); g_assert_nonnull (dialog);
        GtkWidget *view = control (dialog, "GtkTreeView");
        GList *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (gtk_tree_view_get_column (GTK_TREE_VIEW (view), 0)));
        g_signal_emit_by_name (cells->data, "accel-edited", "0", GDK_KEY_Tab, GDK_SHIFT_MASK | GDK_LOCK_MASK, 23);
        g_list_free (cells);
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 1, &data, -1);
        guint key; GdkModifierType mods;
        gtk_accelerator_parse (data, &key, &mods);
        g_assert_cmpuint (key, ==, GDK_KEY_ISO_Left_Tab);
        g_assert_cmpuint (mods, ==, GDK_SHIFT_MASK); g_free (data);
        gtk_button_clicked (GTK_BUTTON (control (page, "shortcut-remove")));
        g_assert_cmpint (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL), ==, 3);
        destroy_page (page); g_object_unref (store);
}

static void
test_shortcuts_order_and_recorder_close (void)
{
        if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
        GtkListStore *store;
        GtkWidget *page = shortcut_fixture (&store);
        GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        g_object_ref_sink (window);
        gtk_container_add (GTK_CONTAINER (window), page);
        gtk_widget_show_all (window);
        GtkTreePath *path = gtk_tree_path_new_from_indices (2, -1);
        preferences_shortcuts_reveal_path (page, path); gtk_tree_path_free (path);
        GdkEventKey event = {0};
        event.type = GDK_KEY_PRESS; event.keyval = GDK_KEY_Up; event.state = GDK_SHIFT_MASK;
        gboolean handled = FALSE;
        g_signal_emit_by_name (control (page, "shortcuts-list"), "key-press-event", &event, &handled);
        g_assert_true (handled);
        GtkTreeIter iter; char *data;
        gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter, NULL, 1);
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 3, &data, -1);
        g_assert_cmpstr (data, ==, "echo original\\nsecond"); g_free (data);
        g_signal_emit_by_name (control (page, "shortcuts-list"), "key-press-event", &event, &handled);
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 3, &data, -1);
        g_assert_cmpstr (data, ==, "\002"); g_free (data);
        gtk_button_clicked (GTK_BUTTON (control (page, "shortcut-record")));
        GList *tops = gtk_window_list_toplevels ();
        GtkWidget *dialog = NULL;
        for (GList *i = tops; i; i = i->next) if (!strcmp (gtk_widget_get_name (i->data), "shortcut-recorder")) dialog = i->data;
        g_list_free (tops); g_assert_nonnull (dialog);
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
        gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter, NULL, 1);
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 1, &data, -1);
        g_assert_cmpstr (data, ==, "<Control>k"); g_free (data);
        gtk_button_clicked (GTK_BUTTON (control (page, "shortcut-record")));
        gtk_widget_destroy (window);
        g_object_unref (window); g_object_unref (store);
}

int main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);
        gtk_available = gtk_init_check (&argc, &argv);
        g_test_add_func ("/preferences/pages/open-preserves-values", test_open_preserves_values);
        g_test_add_func ("/preferences/pages/timestamps-and-dependencies", test_timestamp_and_dependencies);
        g_test_add_func ("/preferences/pages/conversation-preview", test_preview_updates);
        g_test_add_func ("/preferences/pages/layout-switching", test_layout_switching);
        g_test_add_func ("/preferences/pages/notification-mapping", test_notification_mapping);
        g_test_add_func ("/preferences/pages/shared-sound", test_shared_sound_and_effects);
        g_test_add_func ("/preferences/pages/notification-test", test_notification_test_safety);
        g_test_add_func ("/preferences/history/open-preserves-values", test_history_preserves_values);
        g_test_add_func ("/preferences/history/presets-and-custom", test_history_presets_and_custom);
        g_test_add_func ("/preferences/history/dependencies-and-timestamps", test_history_dependencies_and_timestamps);
        g_test_add_func ("/preferences/history/validation", test_history_validation);
        g_test_add_func ("/preferences/typing/languages-preserved", test_typing_preservation_and_languages);
        g_test_add_func ("/preferences/typing/completion-validation", test_typing_completion_and_validation);
        g_test_add_func ("/preferences/shortcuts/filter-and-protection", test_shortcuts_filter_and_protection);
        g_test_add_func ("/preferences/shortcuts/duplicate-and-record", test_shortcuts_duplicate_and_record);
        g_test_add_func ("/preferences/shortcuts/order-and-recorder-close", test_shortcuts_order_and_recorder_close);
        int result = g_test_run ();
        g_free (played_file); g_free (opened_page);
        g_free (opened_folder); g_free (preview_mask);
        return result;
}
