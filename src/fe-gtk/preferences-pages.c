/* ZoiteChat
 * Copyright (C) 2026 deepend-tildeclub.
 * Licensed under the GNU General Public License, version 2 or later.
 */

#include "../common/zoitechat.h"
#include "../common/fe.h"
#include "../common/text.h"
#include "../common/util.h"
#include "preferences-pages.h"
#include "setup.h"
#include "theme/theme-access.h"
#include "notifications/notification-backend.h"

typedef enum { PAGE_TEXT, PAGE_TABS, PAGE_NOTIFICATIONS, PAGE_HISTORY, PAGE_TYPING } PageKind;

/* Stored gui_tab_pos values, shared with the placement menu and maingui. */
typedef enum
{
        POS_TOPLEFT = 1,
        POS_BOTTOMLEFT,
        POS_TOPRIGHT,
        POS_BOTTOMRIGHT,
        POS_TOP,
        POS_BOTTOM,
        POS_HIDDEN
} PreferencesSwitcherPosition;

typedef struct
{
        PageKind kind;
        struct zoitechatprefs *prefs;
        GtkWidget *root;
        GtkWidget *preview, *preview_title, *preview_topic;
        GtkWidget *stamp_combo, *stamp_entry, *stamp_options, *stamp_example;
        GtkWidget *wrap_options, *topic_multiline;
        GtkWidget *tree_options, *tab_options, *placement;
        GtkWidget *layout_drawings[2];
        GtkWidget *tray_options, *sound_entry;
        GtkWidget *quiet_hidden, *tray_alerts[3];
        GtkWidget *test_event, *test_status;
        char *custom_stamp;
        char **beep_file;
        PreferencesSoundChanged sound_changed;
        gpointer sound_data;
        PreferencesAlertCapabilities capabilities;
        gboolean refreshing;
        int tree_position;
        GtkWidget *log_options, *log_style, *log_entry, *log_custom;
        GtkWidget *log_folder, *log_open, *log_file, *log_status, *log_line;
        GtkWidget *history_open, *links_limit;
        char *config_dir, *custom_logmask;
        gboolean log_dirty;
        GtkWidget *typing_preview, *typing_nick, *typing_mode, *spelling_options;
        GtkWidget *language_search, *language_list, *language_entry, *language_status;
        GtkWidget *suffix_entry, *completion_input, *completion_result;
        GPtrArray *language_buttons;
        PreferencesTypingPreview update_typing_preview;
} PreferencesPage;

typedef struct
{
        PreferencesPage *page;
        int *integer;
        char *string;
        gsize capacity;
        int base;
} PreferenceBinding;

static void page_refresh (PreferencesPage *page);

static void
page_mapped (GtkWidget *widget, PreferencesPage *page)
{
        page_refresh (page);
}

static GtkWidget *
page_label (const char *text, gboolean secondary)
{
        GtkWidget *label = gtk_label_new (text);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (label), 58);
        if (secondary)
                gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
        return label;
}

static void
page_pack (GtkWidget *box, GtkWidget *child)
{
        gtk_box_pack_start (GTK_BOX (box), child, FALSE, FALSE, 0);
}

static void
page_status (GtkWidget *label, const char *text)
{
        gtk_widget_set_no_show_all (label, TRUE);
        gtk_label_set_text (GTK_LABEL (label), text);
        gtk_widget_set_visible (label, *text != '\0');
}

static GtkWidget *
page_section (GtkWidget *parent, const char *title, const char *description)
{
        GtkWidget *section = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        GtkWidget *heading = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget *label = page_label (title, FALSE);
        GtkWidget *line = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        char *markup = g_markup_printf_escaped ("<b>%s</b>", title);
        gtk_label_set_markup (GTK_LABEL (label), markup);
        g_free (markup);
        page_pack (heading, label);
        gtk_widget_set_valign (line, GTK_ALIGN_CENTER);
        gtk_box_pack_start (GTK_BOX (heading), line, TRUE, TRUE, 0);
        page_pack (section, heading);
        /* Layout-specific sections manage their own visibility, so their
         * heading children must already be visible when the section is shown. */
        gtk_widget_show_all (heading);
        gtk_widget_set_margin_start (box, 12);
        page_pack (section, box);
        page_pack (parent, section);
        if (description) gtk_widget_set_tooltip_text (label, description);
        return box;
}

static void
page_row (GtkWidget *box, const char *text, GtkWidget *control)
{
        GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *label = page_label (text, FALSE);
        GtkSizeGroup *labels = g_object_get_data (G_OBJECT (box), "row-labels");
        if (!labels)
        {
                labels = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
                g_object_set_data_full (G_OBJECT (box), "row-labels", labels, g_object_unref);
        }
        gtk_label_set_max_width_chars (GTK_LABEL (label), 26);
        gtk_size_group_add_widget (labels, label);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), control);
        gtk_box_pack_start (GTK_BOX (row), label, FALSE, FALSE, 0);
        gboolean expand = (GTK_IS_ENTRY (control) && !GTK_IS_SPIN_BUTTON (control) && gtk_entry_get_max_width_chars (GTK_ENTRY (control)) < 0) || gtk_widget_get_hexpand (control);
        gtk_box_pack_start (GTK_BOX (row), control, expand, expand, 0);
        page_pack (box, row);
}

static PreferenceBinding *
page_binding (PreferencesPage *page, GtkWidget *widget, const char *name)
{
        PreferenceBinding *binding = g_new0 (PreferenceBinding, 1);
        binding->page = page;
        gtk_widget_set_name (widget, name);
        g_object_set_data_full (G_OBJECT (widget), "preference-binding", binding, g_free);
        return binding;
}

static void
page_toggle_changed (GtkToggleButton *button, PreferenceBinding *binding)
{
        *binding->integer = gtk_toggle_button_get_active (button);
        page_refresh (binding->page);
}

static GtkWidget *
page_toggle (PreferencesPage *page, GtkWidget *box, const char *name,
             const char *text, int *value)
{
        GtkWidget *button = gtk_check_button_new_with_label (text);
        GtkWidget *label = gtk_bin_get_child (GTK_BIN (button));
        PreferenceBinding *binding = page_binding (page, button, name);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (label), 56);
        binding->integer = value;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), *value);
        g_signal_connect (button, "toggled", G_CALLBACK (page_toggle_changed), binding);
        if (GTK_IS_FLOW_BOX (box)) gtk_container_add (GTK_CONTAINER (box), button);
        else if (box) page_pack (box, button);
        return button;
}

static void
page_entry_changed (GtkEntry *entry, PreferenceBinding *binding)
{
        g_strlcpy (binding->string, gtk_entry_get_text (entry), binding->capacity);
        page_refresh (binding->page);
}

static GtkWidget *
page_entry (PreferencesPage *page, GtkWidget *box, const char *name,
            const char *text, char *value, gsize capacity)
{
        GtkWidget *entry = gtk_entry_new ();
        PreferenceBinding *binding = page_binding (page, entry, name);
        binding->string = value;
        binding->capacity = capacity;
        gtk_entry_set_max_length (GTK_ENTRY (entry), capacity - 1);
        gtk_entry_set_text (GTK_ENTRY (entry), value);
        atk_object_set_name (gtk_widget_get_accessible (entry), text);
        if (text && *text) page_row (box, text, entry);
        else page_pack (box, entry);
        g_signal_connect (entry, "changed", G_CALLBACK (page_entry_changed), binding);
        return entry;
}

static void
page_combo_changed (GtkComboBox *combo, PreferenceBinding *binding)
{
        int selected = gtk_combo_box_get_active (combo);
        if (selected >= 0) *binding->integer = selected + binding->base;
        page_refresh (binding->page);
}

static GtkWidget *
page_combo (PreferencesPage *page, GtkWidget *box, const char *name,
            const char *text, int *value, int base, const char *const *choices)
{
        GtkWidget *combo = gtk_combo_box_text_new ();
        PreferenceBinding *binding = page_binding (page, combo, name);
        binding->integer = value;
        binding->base = base;
        for (int i = 0; choices[i]; i++)
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(choices[i]));
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), *value - base);
        page_row (box, text, combo);
        g_signal_connect (combo, "changed", G_CALLBACK (page_combo_changed), binding);
        return combo;
}

static void
page_spin_changed (GtkSpinButton *spin, PreferenceBinding *binding)
{
        *binding->integer = gtk_spin_button_get_value_as_int (spin);
        page_refresh (binding->page);
}

static GtkWidget *
page_spin (PreferencesPage *page, GtkWidget *box, const char *name,
           const char *text, int *value, int maximum)
{
        /* Preserve an existing out-of-range value on opening, just as with
         * unknown combo choices. Only a user's edit changes the stored value. */
        GtkWidget *spin = gtk_spin_button_new_with_range (0, MAX (maximum, *value), 1);
        PreferenceBinding *binding = page_binding (page, spin, name);
        binding->integer = value;
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), *value);
        page_row (box, text, spin);
        g_signal_connect (spin, "value-changed", G_CALLBACK (page_spin_changed), binding);
        return spin;
}

static void
page_free (gpointer data)
{
        PreferencesPage *page = data;
        g_free (page->custom_stamp);
        g_free (page->config_dir);
        g_free (page->custom_logmask);
        if (page->language_buttons) g_ptr_array_unref (page->language_buttons);
        g_free (page);
}

static PreferencesPage *
page_new (PageKind kind, struct zoitechatprefs *prefs)
{
        PreferencesPage *page = g_new0 (PreferencesPage, 1);
        page->kind = kind;
        page->prefs = prefs;
        page->root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
        gtk_widget_set_valign (page->root, GTK_ALIGN_START);
        g_object_set_data_full (G_OBJECT (page->root), "preferences-page", page, page_free);
        g_signal_connect (page->root, "map", G_CALLBACK (page_mapped), page);
        return page;
}

static GtkWindow *
page_window (PreferencesPage *page)
{
        GtkWidget *window = gtk_widget_get_toplevel (page->root);
        return GTK_IS_WINDOW (window) ? GTK_WINDOW (window) : NULL;
}

static void
page_font_response (GtkDialog *dialog, int response, GtkWidget *entry)
{
        if (response == GTK_RESPONSE_OK)
        {
                char *font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));
                if (font) gtk_entry_set_text (GTK_ENTRY (entry), font);
                g_free (font);
        }
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
page_choose_font (GtkButton *button, GtkWidget *entry)
{
        PreferenceBinding *binding = g_object_get_data (G_OBJECT (entry), "preference-binding");
        GtkWidget *dialog = gtk_font_chooser_dialog_new (_("Choose a font"), page_window (binding->page));
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
        gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), gtk_entry_get_text (GTK_ENTRY (entry)));
        g_signal_connect_object (dialog, "response", G_CALLBACK (page_font_response), entry, 0);
        gtk_widget_show (dialog);
}

static void
page_font (PreferencesPage *page, GtkWidget *box, const char *name,
           const char *text, char *value, gsize capacity)
{
        GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *entry = page_entry (page, row, name, "", value, capacity);
        GtkWidget *button = gtk_button_new_with_label (_("Choose font…"));
        atk_object_set_name (gtk_widget_get_accessible (entry), text);
        gtk_box_set_child_packing (GTK_BOX (row), entry, TRUE, TRUE, 0, GTK_PACK_START);
        page_pack (row, button);
        gtk_widget_set_hexpand (row, TRUE);
        page_row (box, text, row);
        g_signal_connect (button, "clicked", G_CALLBACK (page_choose_font), entry);
}

static void
page_timestamp (PreferencesPage *page, char *text, gsize size)
{
        GDateTime *today = g_date_time_new_now_local ();
        GDateTime *date = page->kind == PAGE_HISTORY ?
                g_date_time_new_local (g_date_time_get_year (today), g_date_time_get_month (today),
                        g_date_time_get_day_of_month (today), 14, 35, 8) :
                g_date_time_new_local (2026, 1, 15, 14, 35, 8);
        time_t stamp = g_date_time_to_unix (date);
        char *formatted = NULL;
        char *format = page->kind == PAGE_HISTORY ? page->prefs->hex_stamp_log_format : page->prefs->hex_stamp_text_format;
        char *locale_format = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
        text[0] = 0;
        if (locale_format && get_stamp_str (format, stamp, &formatted) > 0)
                g_strlcpy (text, formatted, size);
        g_free (locale_format);
        g_free (formatted);
        g_date_time_unref (date);
        g_date_time_unref (today);
}

static void
page_text_preview (PreferencesPage *page)
{
        struct zoitechatprefs *prefs = page->prefs;
        GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->preview));
        GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
        GtkTextIter iter;
        GdkRGBA foreground, background, nick, marker;
        char timestamp[256], *title;
        int stamp_width = 0, nick_width = 0, space_width = 1;
        PangoLayout *layout;
        const char *font;
#ifdef WIN32
        font = prefs->hex_text_font_main;
#else
        font = prefs->hex_text_font;
#endif
        PangoFontDescription *description = pango_font_description_from_string (*font ? font : "Sans 11");
        page_timestamp (page, timestamp, sizeof timestamp);
        if (!prefs->hex_stamp_text) timestamp[0] = 0;
        layout = gtk_widget_create_pango_layout (page->preview, timestamp);
        pango_layout_set_font_description (layout, description);
        pango_layout_get_pixel_size (layout, &stamp_width, NULL);
        pango_layout_set_text (layout, "longnickname  ", -1);
        pango_layout_get_pixel_size (layout, &nick_width, NULL);
        pango_layout_set_text (layout, " ", -1);
        pango_layout_get_pixel_size (layout, &space_width, NULL);
        theme_get_color (THEME_TOKEN_TEXT_FOREGROUND, &foreground);
        theme_get_color (THEME_TOKEN_TEXT_BACKGROUND, &background);
        theme_get_color (THEME_TOKEN_MIRC_12, &nick);
        theme_get_color (THEME_TOKEN_MARKER, &marker);
        {
                GtkCssProvider *provider = g_object_get_data (G_OBJECT (page->preview), "preview-palette");
                char *bg = gdk_rgba_to_string (&background), *fg = gdk_rgba_to_string (&foreground);
                char *css = g_strdup_printf ("textview, textview text { background-color: %s; color: %s; }", bg, fg);
                if (!provider)
                {
                        provider = gtk_css_provider_new ();
                        gtk_style_context_add_provider (gtk_widget_get_style_context (page->preview),
                                GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
                        g_object_set_data_full (G_OBJECT (page->preview), "preview-palette", provider, g_object_unref);
                }
                gtk_css_provider_load_from_data (provider, css, -1, NULL);
                g_free (css); g_free (bg); g_free (fg);
        }
        g_object_set (gtk_text_tag_table_lookup (table, "body"), "font-desc", description,
                "foreground-rgba", &foreground, "background-rgba", &background, NULL);
        g_object_set (gtk_text_tag_table_lookup (table, "nickname0"), "foreground-rgba",
                prefs->hex_text_color_nicks ? &nick : &foreground, NULL);
        theme_get_color (THEME_TOKEN_MIRC_9, &nick);
        g_object_set (gtk_text_tag_table_lookup (table, "nickname1"), "foreground-rgba",
                prefs->hex_text_color_nicks ? &nick : &foreground, NULL);
        g_object_set (gtk_text_tag_table_lookup (table, "marker"), "foreground-rgba", &marker, NULL);
        gtk_text_buffer_set_text (buffer, "", 0);
        for (int i = 0; i < 3; i++)
        {
                GtkTextIter start, end;
                const char *name = i == 1 ? "longnickname" : "alice";
                const char *message = i == 0 ? _("Hello! Welcome to #zoitechat.") :
                        i == 1 ? _("A longer message wraps onto another line so you can see where the next line begins.") :
                        _("This message arrived after you last read the conversation.");
                int start_offset, wrap_margin, name_width;
                gtk_text_buffer_get_end_iter (buffer, &iter);
                if (i == 2 && prefs->hex_text_show_marker)
                {
                        GtkTextMark *mark = gtk_text_buffer_get_mark (buffer, "unread-preview");
                        if (mark) gtk_text_buffer_move_mark (buffer, mark, &iter);
                        else gtk_text_buffer_create_mark (buffer, "unread-preview", &iter, TRUE);
                        gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, _("Unread messages\n"), -1, "marker", NULL);
                }
                start_offset = gtk_text_iter_get_offset (&iter);
                gtk_text_buffer_insert (buffer, &iter, timestamp, -1);
                pango_layout_set_text (layout, name, -1);
                pango_layout_get_pixel_size (layout, &name_width, NULL);
                if (prefs->hex_text_indent)
                {
                        char *padding = g_strnfill (MAX (0, (nick_width - name_width - 2 * space_width) / MAX (1, space_width)), ' ');
                        gtk_text_buffer_insert (buffer, &iter, padding, -1);
                        g_free (padding);
                }
                gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, name, -1, i == 1 ? "nickname1" : "nickname0", NULL);
                gtk_text_buffer_insert (buffer, &iter, prefs->hex_text_indent ? "\t" : "  ", -1);
                gtk_text_buffer_insert (buffer, &iter, message, -1);
                gtk_text_buffer_insert (buffer, &iter, "\n", 1);
                gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);
                gtk_text_buffer_get_end_iter (buffer, &end);
                wrap_margin = prefs->hex_text_indent ? stamp_width + nick_width :
                        prefs->hex_text_wrap_align == 2 ? stamp_width + name_width + 2 * space_width :
                        prefs->hex_text_wrap_align == 1 ? stamp_width : 0;
                GtkTextTag *paragraph = gtk_text_tag_table_lookup (table, i == 0 ? "line0" : i == 1 ? "line1" : "line2");
                /* GtkTextView's negative indent supplies the hanging margin;
                 * adding left-margin as well would offset the first line too. */
                g_object_set (paragraph, "left-margin", 10,
                        "indent", -wrap_margin,
                        "pixels-below-lines", 8, NULL);
                gtk_text_buffer_apply_tag (buffer, paragraph, &start, &end);
                if (prefs->hex_text_indent)
                {
                        /* Use a tab stop for the message column, so differently
                         * sized nicknames share the same alignment. */
                        PangoTabArray *tabs = pango_tab_array_new (1, TRUE);
                        pango_tab_array_set_tab (tabs, 0, PANGO_TAB_LEFT, stamp_width + nick_width);
                        g_object_set (paragraph, "tabs", tabs, NULL);
                        pango_tab_array_free (tabs);
                }
        }
        g_object_unref (layout);
        pango_font_description_free (description);
        {
                GtkTextIter end;
                gtk_text_buffer_get_bounds (buffer, &iter, &end);
                gtk_text_buffer_apply_tag_by_name (buffer, "body", &iter, &end);
        }
        title = g_strdup_printf ("ZoiteChat — #zoitechat%s%s%s",
                prefs->hex_gui_win_nick ? " · alice" : "",
                prefs->hex_gui_win_ucount ? _(" · 42 users") : "",
                prefs->hex_gui_win_modes ? " · +nt" : "");
        gtk_label_set_text (GTK_LABEL (page->preview_title), title);
        g_free (title);
        gtk_label_set_text (GTK_LABEL (page->preview_topic), prefs->hex_gui_mode_buttons_inline ?
                _("Topic: Welcome to ZoiteChat     [ +n ] [ +t ]") :
                prefs->hex_gui_topicbar_multiline ? _("Topic: Welcome to ZoiteChat\nA longer topic can use another line.") :
                _("Topic: Welcome to ZoiteChat"));
        gtk_widget_set_sensitive (page->stamp_options, prefs->hex_stamp_text);
        gtk_widget_set_visible (gtk_widget_get_parent (page->stamp_entry),
                !g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (page->stamp_combo)), "custom"));
        gtk_widget_set_sensitive (page->stamp_entry,
                !g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (page->stamp_combo)), "custom"));
        gtk_widget_set_sensitive (page->wrap_options, !prefs->hex_text_indent);
        gtk_widget_set_sensitive (page->topic_multiline, !prefs->hex_gui_mode_buttons_inline);
        page_timestamp (page, timestamp, sizeof timestamp);
        title = g_strdup_printf (_("Example: %s"), *timestamp ? timestamp : _("(no timestamp text)"));
        gtk_label_set_text (GTK_LABEL (page->stamp_example), title);
        g_free (title);
}

static gboolean
page_preview_marker (GtkWidget *widget, cairo_t *cr, PreferencesPage *page)
{
        GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
        GtkTextMark *mark = gtk_text_buffer_get_mark (buffer, "unread-preview");
        GtkTextIter iter;
        GdkRectangle line;
        GdkRGBA color;
        int x, y;
        if (!page->prefs->hex_text_show_marker || !mark) return FALSE;
        gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
        gtk_text_view_get_iter_location (GTK_TEXT_VIEW (widget), &iter, &line);
        gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (widget), GTK_TEXT_WINDOW_WIDGET,
                line.x, line.y, &x, &y);
        theme_get_color (THEME_TOKEN_MARKER, &color);
        gdk_cairo_set_source_rgba (cr, &color);
        cairo_set_line_width (cr, 1);
        cairo_move_to (cr, 10, y - 2.5);
        cairo_line_to (cr, gtk_widget_get_allocated_width (widget) - 10, y - 2.5);
        cairo_stroke (cr);
        return FALSE;
}

static void
page_stamp_changed (GtkComboBox *combo, PreferencesPage *page)
{
        const char *id = gtk_combo_box_get_active_id (combo);
        if (!id) return;
        gtk_entry_set_text (GTK_ENTRY (page->stamp_entry),
                strcmp (id, "custom") ? id : page->custom_stamp);
        page_refresh (page);
}

static void
page_custom_stamp_changed (GtkEntry *entry, PreferencesPage *page)
{
        if (g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (page->stamp_combo)), "custom")) return;
        g_free (page->custom_stamp);
        page->custom_stamp = g_strdup (gtk_entry_get_text (entry));
}

GtkWidget *
preferences_text_page_new (struct zoitechatprefs *prefs, const char *const *languages)
{
        PreferencesPage *page = page_new (PAGE_TEXT, prefs);
        GtkWidget *box, *preview, *view, *preview_expander;
        GtkTextBuffer *buffer;
        const char *const alignment[] = {N_("Start of line"), N_("Nickname"), N_("Message"), NULL};
        const char *formats[] = {"%H:%M ", "%I:%M %p ", "[%H:%M:%S] ", "%H:%M:%S ", NULL};
        const char *const names[] = {N_("24-hour · 14:35"), N_("12-hour · 02:35 PM"),
                N_("Bracketed · [14:35:08]"), N_("With seconds · 14:35:08"), NULL};
        preview_expander = gtk_expander_new (_("Conversation preview"));
        gtk_widget_set_name (preview_expander, "text-preview-expander");
        gtk_expander_set_expanded (GTK_EXPANDER (preview_expander), TRUE);
        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add (GTK_CONTAINER (preview_expander), box);
        page->preview_title = page_label ("", TRUE);
        gtk_label_set_ellipsize (GTK_LABEL (page->preview_title), PANGO_ELLIPSIZE_END);
        page->preview_topic = page_label ("", TRUE);
        page_pack (box, page->preview_title);
        page_pack (box, page->preview_topic);
        preview = gtk_scrolled_window_new (NULL, NULL);
        /* Automatic scrolling lets the sample shrink with the preferences
         * window; NEVER would impose TextView's full content width on it. */
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (preview), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (preview), GTK_SHADOW_IN);
        gtk_widget_set_size_request (preview, -1, 110);
        page->preview = view = gtk_text_view_new ();
        gtk_widget_set_name (view, "conversation-preview");
        gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
        gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
        gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 10);
        gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 10);
        gtk_text_view_set_top_margin (GTK_TEXT_VIEW (view), 10);
        gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (view), 10);
        atk_object_set_name (gtk_widget_get_accessible (view), _("Conversation preview"));
        gtk_container_add (GTK_CONTAINER (preview), view);
        g_signal_connect_after (view, "draw", G_CALLBACK (page_preview_marker), page);
        page_pack (box, preview);
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
        gtk_text_buffer_create_tag (buffer, "body", NULL);
        gtk_text_buffer_create_tag (buffer, "nickname0", NULL);
        gtk_text_buffer_create_tag (buffer, "nickname1", NULL);
        gtk_text_buffer_create_tag (buffer, "marker", "scale", 0.85, NULL);
        for (int i = 0; i < 3; i++)
        {
                char name[16];
                g_snprintf (name, sizeof name, "line%d", i);
                gtk_text_buffer_create_tag (buffer, name, NULL);
        }
        box = page_section (page->root, _("Conversation text"), NULL);
#ifdef WIN32
        if (languages) page_combo (page, box, "language", _("Language"), &prefs->hex_gui_lang, 0, languages);
        page_font (page, box, "chat-font", _("Main font"), prefs->hex_text_font_main, sizeof prefs->hex_text_font_main);
#else
        (void)languages;
        page_font (page, box, "chat-font", _("Font"), prefs->hex_text_font, sizeof prefs->hex_text_font);
#endif
        page_toggle (page, box, "colored-nicks", _("Give nicknames different colours"), &prefs->hex_text_color_nicks);
        page_toggle (page, box, "indent-nicks", _("Align nicknames in a separate column"), &prefs->hex_text_indent);
        page->wrap_options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        page_pack (box, page->wrap_options);
        page_combo (page, page->wrap_options, "wrap-alignment", _("Align wrapped lines under"), &prefs->hex_text_wrap_align, 0, alignment);
        page_toggle (page, box, "unread-marker", _("Show a line where unread messages begin"), &prefs->hex_text_show_marker);
        page_toggle (page, box, "keep-search-position", _("Keep the conversation at the search result after closing search"), &prefs->hex_text_search_keep_position);
        page_pack (page->root, preview_expander);
        box = page_section (page->root, _("Timestamps"), NULL);
        page_toggle (page, box, "timestamps", _("Show a time beside each message"), &prefs->hex_stamp_text);
        page->stamp_options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        page_pack (box, page->stamp_options);
        page->stamp_combo = gtk_combo_box_text_new ();
        gtk_widget_set_name (page->stamp_combo, "timestamp-style");
        for (int i = 0; formats[i]; i++)
                gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->stamp_combo), formats[i], _(names[i]));
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->stamp_combo), "custom", _("Custom format"));
        if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->stamp_combo), prefs->hex_stamp_text_format))
                gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->stamp_combo), "custom");
        page->custom_stamp = g_strdup (prefs->hex_stamp_text_format);
        page_row (page->stamp_options, _("Time style"), page->stamp_combo);
        page->stamp_entry = page_entry (page, page->stamp_options, "timestamp-format", _("Custom format"),
                prefs->hex_stamp_text_format, sizeof prefs->hex_stamp_text_format);
        gtk_widget_show_all (gtk_widget_get_parent (page->stamp_entry));
        gtk_widget_set_no_show_all (gtk_widget_get_parent (page->stamp_entry), TRUE);
        page->stamp_example = page_label ("", TRUE);
        page_pack (page->stamp_options, page->stamp_example);
        gtk_widget_set_tooltip_text (page->stamp_entry, _("%H: hour (00–23), %I: hour (01–12), %M: minute, %S: second, %p: AM/PM. Spaces are kept."));
        g_signal_connect (page->stamp_combo, "changed", G_CALLBACK (page_stamp_changed), page);
        g_signal_connect (page->stamp_entry, "changed", G_CALLBACK (page_custom_stamp_changed), page);
        box = page_section (page->root, _("Window information"), NULL);
        page_toggle (page, box, "title-modes", _("Show channel modes in the title bar"), &prefs->hex_gui_win_modes);
        page_toggle (page, box, "title-users", _("Show the number of users in the title bar"), &prefs->hex_gui_win_ucount);
        page_toggle (page, box, "title-nick", _("Show your nickname in the title bar"), &prefs->hex_gui_win_nick);
        page_toggle (page, box, "inline-modes", _("Place channel mode buttons beside the topic"), &prefs->hex_gui_mode_buttons_inline);
        page->topic_multiline = page_toggle (page, box, "multiline-topic", _("Allow topics to use multiple lines"), &prefs->hex_gui_topicbar_multiline);
        gtk_widget_set_tooltip_text (page->topic_multiline, _("Available when mode buttons are not beside the topic."));
        page_refresh (page);
        return page->root;
}

static void
page_layout_outline (cairo_t *cr, double x, double y, double width, double height)
{
        const double radius = 4;
        cairo_new_sub_path (cr);
        cairo_arc (cr, x + width - radius, y + radius, radius, -G_PI / 2, 0);
        cairo_arc (cr, x + width - radius, y + height - radius, radius, 0, G_PI / 2);
        cairo_arc (cr, x + radius, y + height - radius, radius, G_PI / 2, G_PI);
        cairo_arc (cr, x + radius, y + radius, radius, G_PI, 3 * G_PI / 2);
        cairo_close_path (cr);
}

static gboolean
page_layout_draw (GtkWidget *widget, cairo_t *cr, PreferencesPage *page)
{
        GtkStyleContext *style = gtk_widget_get_style_context (widget);
        GdkRGBA ink, base = {1, 1, 1, 1}, accent = {0.2, 0.45, 0.8, 1};
        int type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "layout"));
        int position = page->prefs->hex_gui_tab_layout == type ? page->prefs->hex_gui_tab_pos : (type == 0 ? POS_BOTTOM : POS_TOPLEFT);
        double x = 5, y = 5, width = gtk_widget_get_allocated_width (widget) - 10;
        double height = gtk_widget_get_allocated_height (widget) - 10;
        double nav_x = x, nav_y = y, nav_width = width, nav_height = 12;
        gtk_style_context_get_color (style, GTK_STATE_FLAG_NORMAL, &ink);
        gtk_style_context_lookup_color (style, "theme_base_color", &base);
        gtk_style_context_lookup_color (style, "theme_selected_bg_color", &accent);
        page_layout_outline (cr, x, y, width, height);
        gdk_cairo_set_source_rgba (cr, &base);
        cairo_fill_preserve (cr);
        ink.alpha = 0.35;
        gdk_cairo_set_source_rgba (cr, &ink);
        cairo_set_line_width (cr, 1);
        cairo_stroke (cr);
        if (position <= POS_BOTTOMRIGHT)
        {
                nav_width = width * 0.28;
                nav_height = height * 0.78;
                if (position == POS_BOTTOMLEFT || position == POS_BOTTOMRIGHT) nav_y += height - nav_height;
                if (position >= POS_TOPRIGHT) nav_x += width - nav_width;
        }
        else if (position == POS_BOTTOM) nav_y += height - nav_height;
        cairo_save (cr);
        page_layout_outline (cr, x, y, width, height);
        cairo_clip (cr);
        if (position != POS_HIDDEN)
        {
                accent.alpha = 0.18;
                gdk_cairo_set_source_rgba (cr, &accent);
                cairo_rectangle (cr, nav_x, nav_y, nav_width, nav_height);
                cairo_fill (cr);
                accent.alpha = 0.8;
                gdk_cairo_set_source_rgba (cr, &accent);
                for (int i = 0; i < 3; i++)
                {
                        if (position <= POS_BOTTOMRIGHT)
                                cairo_rectangle (cr, nav_x + 5 + (type == 2 && i ? 3 : 0), nav_y + 8 + i * 10, nav_width - 12, 2);
                        else
                                cairo_rectangle (cr, nav_x + 6 + i * nav_width / 3, nav_y + 5, nav_width / 3 - 12, 2);
                }
                cairo_fill (cr);
        }
        double chat_x = x + 7, chat_y = y + 10, chat_width = width - 14;
        if (position <= POS_BOTTOMRIGHT)
        {
                chat_width -= nav_width;
                if (position <= POS_BOTTOMLEFT) chat_x += nav_width;
        }
        else if (position == POS_TOP) chat_y += nav_height;
        ink.alpha = 0.3;
        gdk_cairo_set_source_rgba (cr, &ink);
        for (int i = 0; i < 3; i++)
                cairo_rectangle (cr, chat_x, chat_y + i * 9, chat_width * (i == 1 ? 0.65 : 0.9), 2);
        cairo_fill (cr);
        cairo_restore (cr);
        return FALSE;
}

static void
page_layout_changed (GtkToggleButton *button, PreferencesPage *page)
{
        int layout = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "layout"));
        if (!gtk_toggle_button_get_active (button) || page->prefs->hex_gui_tab_layout == layout) return;
        if (layout == 0)
        {
                page->tree_position = page->prefs->hex_gui_tab_pos;
                /* Mirror mg_change_layout's horizontal-tab placement, but make
                 * that result visible before Save. Merely opening changes none. */
                if (page->prefs->hex_gui_tab_pos != POS_TOP && page->prefs->hex_gui_tab_pos != POS_BOTTOM)
                        page->prefs->hex_gui_tab_pos = POS_BOTTOM;
        }
        else page->prefs->hex_gui_tab_pos = page->tree_position;
        page->prefs->hex_gui_tab_layout = layout;
        gtk_combo_box_set_active (GTK_COMBO_BOX (page->placement), page->prefs->hex_gui_tab_pos - 1);
        page_refresh (page);
}

GtkWidget *
preferences_tabs_page_new (struct zoitechatprefs *prefs)
{
        PreferencesPage *page = page_new (PAGE_TABS, prefs);
        GtkWidget *box, *choices, *button, *content, *drawing;
        GSList *group = NULL;
        const char *const positions[] = {N_("Left · upper"), N_("Left · lower"), N_("Right · upper"),
                N_("Right · lower"), N_("Top"), N_("Bottom"), N_("Hidden"), NULL};
        const char *const focus[] = {N_("Never"), N_("Always"), N_("Only tabs I request"), NULL};
        const char *const notices[] = {N_("Automatic"), N_("A separate tab"), N_("The current tab"), NULL};
        const char *const containers[] = {N_("Separate windows"), N_("Tabs"), NULL};
        page->tree_position = prefs->hex_gui_tab_pos;
        box = page_section (page->root, _("Channel navigation"),
                _("Choose how networks and conversations are arranged."));
        choices = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 18);
        page_pack (box, choices);
        for (int i = 0; i < 2; i++)
        {
                int layout = i == 0 ? 0 : 2;
                button = gtk_radio_button_new (group);
                gtk_widget_set_name (button, i == 0 ? "layout-tabs" : "layout-tree");
                atk_object_set_name (gtk_widget_get_accessible (button), i == 0 ? _("Tabs") : _("Tree"));
                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), prefs->hex_gui_tab_layout == layout);
                g_object_set_data (G_OBJECT (button), "layout", GINT_TO_POINTER (layout));
                content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
                page_pack (content, page_label (i == 0 ? _("Tabs") : _("Tree"), FALSE));
                drawing = gtk_drawing_area_new ();
                page->layout_drawings[i] = drawing;
                gtk_widget_set_size_request (drawing, 120, 64);
                g_object_set_data (G_OBJECT (drawing), "layout", GINT_TO_POINTER (layout));
                g_signal_connect (drawing, "draw", G_CALLBACK (page_layout_draw), page);
                page_pack (content, drawing);
                gtk_container_add (GTK_CONTAINER (button), content);
                page_pack (choices, button);
                g_signal_connect (button, "toggled", G_CALLBACK (page_layout_changed), page);
        }
        page->placement = page_combo (page, box, "channel-placement", _("Placement"), &prefs->hex_gui_tab_pos, 1, positions);
        gtk_widget_set_tooltip_text (page->placement, _("Switching to tabs uses the bottom when the tree is at a side or hidden."));
        page_toggle (page, box, "alphabetical-tabs", _("Sort conversations alphabetically"), &prefs->hex_gui_tab_sort);
        page_toggle (page, box, "wheel-tabs", _("Use the mouse wheel over the switcher to change conversations"), &prefs->hex_gui_tab_scrollchans);
        page_toggle (page, box, "small-tabs", _("Use smaller text in the channel switcher"), &prefs->hex_gui_tab_small);
        gtk_widget_set_tooltip_text (page_spin (page, box, "tab-label-length", _("Label length (characters)"), &prefs->hex_gui_tab_trunc, 99), _("0, 1 or 2 keeps full conversation names."));
        page->tree_options = page_section (page->root, _("Sidebar tree details"), NULL);
        /* Include the section heading when switching layout-specific controls. */
        box = page->tree_options;
        page->tree_options = gtk_widget_get_parent (box);
        gtk_widget_set_no_show_all (page->tree_options, TRUE);
        page_toggle (page, box, "tree-icons", _("Show network and channel icons"), &prefs->hex_gui_tab_icons);
        page_toggle (page, box, "tree-dots", _("Show dotted lines between tree items"), &prefs->hex_gui_tab_dots);
        gtk_widget_show_all (box);
        page->tab_options = page_section (page->root, _("Tab details"), NULL);
        box = page->tab_options;
        page->tab_options = gtk_widget_get_parent (box);
        gtk_widget_set_no_show_all (page->tab_options, TRUE);
        page_font (page, box, "tab-font", _("Tab font"), prefs->hex_gui_tab_font, sizeof prefs->hex_gui_tab_font);
        gtk_widget_set_tooltip_text (box, _("Leave the font blank to use the application theme. Smaller text still applies."));
        page_toggle (page, box, "tab-close-buttons", _("Show a close button on each tab"), &prefs->hex_gui_tab_closebuttons);
        gtk_widget_show_all (box);
        box = page_section (page->root, _("Opening conversations"), NULL);
        page_toggle (page, box, "server-message-tab", _("Keep server messages in a separate tab"), &prefs->hex_gui_tab_server);
        page_toggle (page, box, "private-message-tab", _("Open a conversation when someone sends a private message"), &prefs->hex_gui_autoopen_dialog);
        page_combo (page, box, "focus-new-tabs", _("Switch to new tabs"), &prefs->hex_gui_tab_newtofront, 0, focus);
        page_combo (page, box, "notice-placement", _("Show notices in"), &prefs->hex_irc_notice_pos, 0, notices);
        page_combo (page, box, "channel-container", _("Open channels in"), &prefs->hex_gui_tab_chans, 0, containers);
        page_combo (page, box, "dialog-container", _("Open private conversations in"), &prefs->hex_gui_tab_dialogs, 0, containers);
        page_combo (page, box, "utility-container", _("Open utility windows in"), &prefs->hex_gui_tab_utils, 0, containers);
        box = page_section (page->root, _("Closing conversations"), NULL);
        page_toggle (page, box, "middle-close", _("Close a conversation with the middle mouse button"), &prefs->hex_gui_tab_middleclose);
        gtk_widget_show_all (page->root);
        page_refresh (page);
        return page->root;
}

static void
page_sound_changed (GtkEntry *entry, PreferencesPage *page)
{
        const char *text = gtk_entry_get_text (entry);
        if (page->refreshing || !g_strcmp0 (text, *page->beep_file)) return;
        g_free (*page->beep_file);
        *page->beep_file = g_strdup (text);
        if (page->sound_changed)
                page->sound_changed (page->sound_data);
        page_refresh (page);
}

void
preferences_notifications_refresh_sound (GtkWidget *widget)
{
        PreferencesPage *page = g_object_get_data (G_OBJECT (widget), "preferences-page");
        if (!page || page->kind != PAGE_NOTIFICATIONS) return;
        page->refreshing = TRUE;
        gtk_entry_set_text (GTK_ENTRY (page->sound_entry), *page->beep_file ? *page->beep_file : "");
        page->refreshing = FALSE;
        page_refresh (page);
}

static void
page_sound_response (GtkDialog *dialog, int response, GtkWidget *entry)
{
        if (response == GTK_RESPONSE_ACCEPT)
        {
                char *file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
                if (file) gtk_entry_set_text (GTK_ENTRY (entry), file);
                g_free (file);
        }
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
page_choose_sound (GtkButton *button, PreferencesPage *page)
{
        GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Choose an alert sound"), page_window (page),
                GTK_FILE_CHOOSER_ACTION_OPEN, _("Cancel"), GTK_RESPONSE_CANCEL,
                _("Open"), GTK_RESPONSE_ACCEPT, NULL);
        GtkFileFilter *filter = gtk_file_filter_new ();
#ifdef WIN32
        gtk_file_filter_set_name (filter, _("WAV sound files"));
        gtk_file_filter_add_pattern (filter, "*.[wW][aA][vV]");
#else
        gtk_file_filter_set_name (filter, _("Audio files"));
        gtk_file_filter_add_mime_type (filter, "audio/*");
#endif
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
        if (*page->beep_file && g_path_is_absolute (*page->beep_file))
                gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), *page->beep_file);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
        g_signal_connect_object (dialog, "response", G_CALLBACK (page_sound_response), page->sound_entry, 0);
        gtk_widget_show (dialog);
}

static void
page_system_sound (GtkButton *button, PreferencesPage *page)
{
        gtk_entry_set_text (GTK_ENTRY (page->sound_entry), "");
}

static void
page_play_sound (GtkButton *button, PreferencesPage *page)
{
        if (*page->beep_file && **page->beep_file) sound_play (*page->beep_file, FALSE);
        else fe_beep (NULL);
}

static void
page_open_sounds (GtkButton *button, gpointer unused)
{
        /* The full editor remains available for less common event sounds. */
        setup_open_page ("sounds");
}

static void
page_test_notification (GtkButton *button, PreferencesPage *page)
{
        int event = gtk_combo_box_get_active (GTK_COMBO_BOX (page->test_event));
        int enabled = event == 0 ? page->prefs->hex_input_balloon_priv :
                event == 1 ? page->prefs->hex_input_balloon_hilight : page->prefs->hex_input_balloon_chans;
        GString *status;
        if (!(page->capabilities & PREFERENCES_ALERT_DESKTOP) || !notification_backend_supported ())
        {
                gtk_label_set_text (GTK_LABEL (page->test_status), _("Desktop notifications are not available in this session."));
                return;
        }
        notification_backend_show (_("ZoiteChat · notification test"),
                event == 0 ? _("alice sent you a private message: Hello!") :
                event == 1 ? _("alice mentioned you in #zoitechat: Are you there?") :
                _("alice posted in #zoitechat: Hello, everyone!"));
        status = g_string_new (_("Test requested. Your desktop may still silence notifications."));
        if (!enabled) g_string_append (status, _(" Desktop alerts for this message type are currently switched off in this page."));
        if (page->prefs->hex_away_omit_alerts) g_string_append (status, _(" Real alerts are muted while you are marked away."));
        if (page->prefs->hex_gui_focus_omitalerts) g_string_append (status, _(" Real desktop alerts are muted while the main window is active."));
        if (page->prefs->hex_gui_tray && page->prefs->hex_gui_tray_quiet)
                g_string_append (status, _(" Real desktop alerts are only shown when hidden to the tray."));
        gtk_label_set_text (GTK_LABEL (page->test_status), status->str);
        g_string_free (status, TRUE);
}

static void
page_notification_group (PreferencesPage *page, GtkWidget *grid, int row,
                         const char *title, const char *description,
                         int *desktop, int *beep, int *tray, int *attention, const char *id)
{
        GtkWidget *label = page_label (title, FALSE), *control;
        gtk_widget_set_tooltip_text (label, description);
        gtk_widget_set_hexpand (label, TRUE);
        gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
        int *values[] = {desktop, beep, tray, attention};
        const char *suffixes[] = {"desktop", "beep", "tray", "attention"};
        const char *names[] = {N_("Desktop notification"), N_("Play alert sound"), N_("Blink tray icon"),
#ifdef HAVE_GTK_MAC
                N_("Bounce Dock icon")};
#else
                N_("Flash taskbar")};
#endif
        int column = 1;
        for (int i = 0; i < 4; i++)
        {
                if (i == 2 && !(page->capabilities & PREFERENCES_ALERT_TRAY)) continue;
                if (i == 3 && !(page->capabilities & PREFERENCES_ALERT_ATTENTION)) continue;
                char *name = g_strdup_printf ("%s-%s", id, suffixes[i]);
                char *accessible = g_strdup_printf ("%s: %s", title, _(names[i]));
                control = page_toggle (page, NULL, name, "", values[i]);
                atk_object_set_name (gtk_widget_get_accessible (control), accessible);
                gtk_widget_set_tooltip_text (control, accessible);
                gtk_widget_set_halign (control, GTK_ALIGN_CENTER);
                gtk_grid_attach (GTK_GRID (grid), control, column++, row, 1, 1);
                g_free (name); g_free (accessible);
                if (i == 0)
                {
                        gtk_widget_set_sensitive (control, page->capabilities & PREFERENCES_ALERT_DESKTOP);
                        if (!(page->capabilities & PREFERENCES_ALERT_DESKTOP))
                                gtk_widget_set_tooltip_text (control, _("Desktop notifications are unavailable. Your saved choice is kept."));
                        else if (!strcmp (id, "private"))
                                gtk_widget_set_tooltip_text (control, _("Also covers notices, channel invitations and file offers."));
                }
                if (i == 2)
                {
                        page->tray_alerts[row - 1] = control;
                        gtk_widget_set_tooltip_text (control, _("Enable the system tray icon below to use this indicator."));
                }
        }
}

GtkWidget *
preferences_notifications_page_new (struct zoitechatprefs *prefs,
        PreferencesAlertCapabilities capabilities, char **beep_file,
        PreferencesSoundChanged sound_changed, gpointer user_data)
{
        PreferencesPage *page = page_new (PAGE_NOTIFICATIONS, prefs);
        GtkWidget *box, *row, *button, *advanced, *rules;
        const char *const events[] = {N_("Private message"), N_("Mention"), N_("Channel message"), NULL};
        page->capabilities = capabilities;
        page->beep_file = beep_file;
        page->sound_changed = sound_changed;
        page->sound_data = user_data;
        box = page_section (page->root, _("Notify me about"), NULL);
        GtkWidget *grid = gtk_grid_new ();
        gtk_widget_set_name (grid, "notification-options");
        gtk_grid_set_column_spacing (GTK_GRID (grid), 16);
        gtk_grid_set_row_spacing (GTK_GRID (grid), 4);
        const char *headers[] = {N_("Desktop alert"), N_("Sound"), N_("Tray icon"),
#ifdef HAVE_GTK_MAC
                N_("Dock")};
#else
                N_("Taskbar")};
#endif
        int column = 1;
        for (int i = 0; i < 4; i++)
        {
                if (i == 2 && !(capabilities & PREFERENCES_ALERT_TRAY)) continue;
                if (i == 3 && !(capabilities & PREFERENCES_ALERT_ATTENTION)) continue;
                GtkWidget *label = gtk_label_new (_(headers[i]));
                gtk_grid_attach (GTK_GRID (grid), label, column++, 0, 1, 1);
        }
        GtkWidget *table = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (table), GTK_SHADOW_IN);
        gtk_container_set_border_width (GTK_CONTAINER (grid), 10);
        gtk_style_context_add_class (gtk_widget_get_style_context (grid), "view");
        gtk_container_add (GTK_CONTAINER (table), grid);
        page_pack (box, table);
        page_notification_group (page, grid, 1, _("Private messages"), _("Messages sent directly to you."),
                &prefs->hex_input_balloon_priv, &prefs->hex_input_beep_priv,
                &prefs->hex_input_tray_priv, &prefs->hex_input_flash_priv, "private");
        page_notification_group (page, grid, 2, _("Mentions"), _("Channel messages containing your nickname or matching your mention rules."),
                &prefs->hex_input_balloon_hilight, &prefs->hex_input_beep_hilight,
                &prefs->hex_input_tray_hilight, &prefs->hex_input_flash_hilight, "mention");
        page_notification_group (page, grid, 3, _("Other channel messages"), _("Channel messages without a mention."),
                &prefs->hex_input_balloon_chans, &prefs->hex_input_beep_chans,
                &prefs->hex_input_tray_chans, &prefs->hex_input_flash_chans, "channel");
        page_pack (box, page_label (_("Per-channel alert settings can override these choices."), TRUE));
        box = page_section (page->root, _("Alert sound"),
                _("Used by “Play alert sound” above. Leave the filename empty for the system sound. Other event sounds are configured separately."));
        page->sound_entry = gtk_entry_new ();
        gtk_widget_set_name (page->sound_entry, "alert-sound-file");
        gtk_entry_set_placeholder_text (GTK_ENTRY (page->sound_entry), _("System sound"));
        atk_object_set_name (gtk_widget_get_accessible (page->sound_entry), _("Alert sound filename"));
        gtk_entry_set_text (GTK_ENTRY (page->sound_entry), *beep_file ? *beep_file : "");
        page_pack (box, page->sound_entry);
        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        button = gtk_button_new_with_label (_("Choose file…"));
        g_signal_connect (button, "clicked", G_CALLBACK (page_choose_sound), page);
        page_pack (row, button);
        button = gtk_button_new_with_label (_("Play"));
        g_signal_connect (button, "clicked", G_CALLBACK (page_play_sound), page);
        page_pack (row, button);
        button = gtk_button_new_with_label (_("Use system sound"));
        g_signal_connect (button, "clicked", G_CALLBACK (page_system_sound), page);
        page_pack (row, button);
        page_pack (box, row);
        button = gtk_button_new_with_label (_("Event sounds…"));
        gtk_widget_set_name (button, "open-event-sounds");
        g_signal_connect (button, "clicked", G_CALLBACK (page_open_sounds), NULL);
        gtk_widget_set_halign (button, GTK_ALIGN_START);
        page_pack (box, button);
        g_signal_connect (page->sound_entry, "changed", G_CALLBACK (page_sound_changed), page);
        box = page_section (page->root, _("Quiet rules"), NULL);
        page_toggle (page, box, "quiet-away", _("Mute alerts while marked away"), &prefs->hex_away_omit_alerts);
        page_toggle (page, box, "quiet-focused", _("Mute desktop notifications and alert sounds while the main window is active"), &prefs->hex_gui_focus_omitalerts);
        if (capabilities & PREFERENCES_ALERT_HIDDEN_ONLY)
                page->quiet_hidden = page_toggle (page, box, "quiet-hidden", _("Show desktop notifications only when hidden to the tray"), &prefs->hex_gui_tray_quiet);
        gtk_widget_set_tooltip_text (box, _("Per-channel alert overrides still apply. Event sounds and activity indicators can follow different rules."));
        advanced = gtk_expander_new (_("Test notification"));
        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_start (box, 12);
        gtk_container_add (GTK_CONTAINER (advanced), box);
        page_pack (page->root, advanced);
        page_pack (box, page_label (_("Sends immediately and bypasses quiet rules."), TRUE));
        page->test_event = gtk_combo_box_text_new ();
        gtk_widget_set_name (page->test_event, "notification-test-event");
        for (int i = 0; events[i]; i++)
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (page->test_event), _(events[i]));
        gtk_combo_box_set_active (GTK_COMBO_BOX (page->test_event), 0);
        page_row (box, _("Message type"), page->test_event);
        button = gtk_button_new_with_label (_("Send test"));
        gtk_widget_set_name (button, "notification-test");
        gtk_widget_set_sensitive (button, capabilities & PREFERENCES_ALERT_DESKTOP);
        g_signal_connect (button, "clicked", G_CALLBACK (page_test_notification), page);
        page_pack (box, button);
        page->test_status = page_label (capabilities & PREFERENCES_ALERT_DESKTOP ?
                _("Your desktop may require notifications to be allowed for ZoiteChat.") :
                _("Desktop notifications are not available in this session. Your saved choices are kept."), TRUE);
        gtk_widget_set_name (page->test_status, "notification-test-status");
        page_pack (box, page->test_status);
        advanced = gtk_expander_new (_("Mention rules"));
        rules = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_start (rules, 12);
        gtk_container_add (GTK_CONTAINER (advanced), rules);
        page_pack (rules, page_label (_("Separate entries with commas. Wildcards (*) are supported. Your nickname is included automatically."), TRUE));
        page_entry (page, rules, "mention-words", _("Additional words to highlight"), prefs->hex_irc_extra_hilight, sizeof prefs->hex_irc_extra_hilight);
        page_entry (page, rules, "mention-ignore-nicks", _("Nicknames not to highlight"), prefs->hex_irc_no_hilight, sizeof prefs->hex_irc_no_hilight);
        page_entry (page, rules, "mention-always-nicks", _("Nicknames to always highlight"), prefs->hex_irc_nick_hilight, sizeof prefs->hex_irc_nick_hilight);
        page_pack (page->root, advanced);
        if (capabilities & PREFERENCES_ALERT_TRAY_BEHAVIOR)
        {
                box = page_section (page->root, _("System tray"), NULL);
                page_toggle (page, box, "tray-enabled", _("Show the system tray icon"), &prefs->hex_gui_tray);
                page->tray_options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
                page_pack (box, page->tray_options);
                page_toggle (page, page->tray_options, "tray-minimize", _("Hide to the tray when minimized"), &prefs->hex_gui_tray_minimize);
                page_toggle (page, page->tray_options, "tray-close", _("Hide to the tray when the main window is closed"), &prefs->hex_gui_tray_close);
                page_toggle (page, page->tray_options, "tray-away", _("Mark away when hidden to the tray and back when restored"), &prefs->hex_gui_tray_away);
        }
        page_refresh (page);
        return page->root;
}

static gboolean
page_path_separator (char c)
{
#ifdef WIN32
        return c == '/' || c == '\\';
#else
        return c == '/';
#endif
}

/* Separate the fixed folder from a filename pattern. Stop at the first
 * substitution so a custom dated directory remains part of the pattern. */
static void
page_log_parts (PreferencesPage *page, const char *mask, char **folder, char **pattern)
{
        const char *last = NULL;
        if (!g_path_is_absolute (mask))
        {
                *folder = g_build_filename (page->config_dir, "logs", NULL);
                *pattern = g_strdup (mask);
                return;
        }
        for (const char *p = mask; *p; p++)
        {
                if (*p == '%' && p[1] == '%') { p++; continue; }
                if (*p == '%') break;
                if (page_path_separator (*p)) last = p;
        }
        if (!last)
        {
                *folder = g_path_get_dirname (mask);
                *pattern = g_path_get_basename (mask);
        }
        else
        {
                char *raw = g_strndup (mask, last - mask + 1);
                char **parts = g_strsplit (raw, "%%", -1);
                *folder = g_strjoinv ("%", parts);
                *pattern = g_strdup (last + 1);
                g_strfreev (parts);
                g_free (raw);
        }
}

static char *
page_log_join (const char *folder, const char *pattern)
{
        char **parts = g_strsplit (folder, "%", -1);
        char *escaped = g_strjoinv ("%%", parts);
        char *mask = g_build_filename (escaped, pattern, NULL);
        g_strfreev (parts);
        g_free (escaped);
        return mask;
}

static void
page_open_folder (GtkButton *button, gpointer unused)
{
        const char *path = g_object_get_data (G_OBJECT (button), "folder-path");
        if (path && g_file_test (path, G_FILE_TEST_IS_DIR)) fe_open_url (path);
}

static void
page_folder_target (GtkWidget *button, const char *path)
{
        g_object_set_data_full (G_OBJECT (button), "folder-path", g_strdup (path), g_free);
        gtk_widget_set_sensitive (button, g_file_test (path, G_FILE_TEST_IS_DIR));
}

static GtkWidget *
page_folder_button (GtkWidget *box, const char *name, const char *label, const char *path)
{
        GtkWidget *button = gtk_button_new_with_label (label);
        gtk_widget_set_name (button, name);
        gtk_widget_set_halign (button, GTK_ALIGN_START);
        page_folder_target (button, path);
        g_signal_connect (button, "clicked", G_CALLBACK (page_open_folder), NULL);
        page_pack (box, button);
        return button;
}

static GtkWidget *
page_path_label (GtkWidget *box, const char *name, const char *path)
{
        GtkWidget *label = page_label (path, FALSE);
        gtk_label_set_selectable (GTK_LABEL (label), TRUE);
        gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
        gtk_widget_set_name (label, name);
        page_pack (box, label);
        return label;
}

static void
page_log_entry_changed (GtkEntry *entry, PreferencesPage *page)
{
        if (page->refreshing) return;
        page->log_dirty = TRUE;
        if (!g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (page->log_style)), "custom"))
        {
                g_free (page->custom_logmask);
                page->custom_logmask = g_strdup (gtk_entry_get_text (entry));
        }
        page_refresh (page);
}

static void
page_log_style_changed (GtkComboBox *combo, PreferencesPage *page)
{
        const char *id = gtk_combo_box_get_active_id (combo);
        char *folder, *pattern, *mask;
        if (!id) return;
        page_log_parts (page, page->prefs->hex_irc_logmask, &folder, &pattern);
        mask = !strcmp (id, "custom") ? g_strdup (page->custom_logmask) :
                g_path_is_absolute (page->prefs->hex_irc_logmask) ? page_log_join (folder, id) : g_strdup (id);
        gtk_entry_set_text (GTK_ENTRY (page->log_entry), mask);
        page_refresh (page);
        g_free (folder); g_free (pattern); g_free (mask);
}

static void
page_log_change_folder (PreferencesPage *page, const char *folder)
{
        char *old_folder, *pattern, *custom_folder, *custom_pattern;
        char *mask, *custom;
        page_log_parts (page, page->prefs->hex_irc_logmask, &old_folder, &pattern);
        page_log_parts (page, page->custom_logmask, &custom_folder, &custom_pattern);
        mask = folder ? page_log_join (folder, pattern) : g_strdup (pattern);
        custom = folder ? page_log_join (folder, custom_pattern) : g_strdup (custom_pattern);
        if (strlen (mask) < sizeof page->prefs->hex_irc_logmask &&
            strlen (custom) < sizeof page->prefs->hex_irc_logmask)
        {
                /* A later switch to Custom must keep the folder just chosen. */
                g_free (page->custom_logmask);
                page->custom_logmask = custom;
                custom = NULL;
                gtk_entry_set_text (GTK_ENTRY (page->log_entry), mask);
        }
        else page_status (page->log_status,
                _("That folder makes the filename format too long. Choose a shorter path or use a shorter custom format."));
        g_free (old_folder); g_free (pattern); g_free (custom_folder); g_free (custom_pattern);
        g_free (mask); g_free (custom);
}

static void
page_log_folder_response (GtkDialog *dialog, int response, GtkWidget *root)
{
        PreferencesPage *page = g_object_get_data (G_OBJECT (root), "preferences-page");
        if (response == GTK_RESPONSE_ACCEPT)
        {
                char *native = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
                char *folder = native ? g_filename_to_utf8 (native, -1, NULL, NULL, NULL) : NULL;
                if (folder)
                {
                        page_log_change_folder (page, folder);
                }
                else page_status (page->log_status, _("This folder name cannot be represented in the configuration's UTF-8 format."));
                g_free (native); g_free (folder);
        }
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
page_choose_log_folder (GtkButton *button, PreferencesPage *page)
{
        GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Choose a log folder"), page_window (page),
                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _("Cancel"), GTK_RESPONSE_CANCEL,
                _("Choose"), GTK_RESPONSE_ACCEPT, NULL);
        char *folder, *pattern, *native;
        page_log_parts (page, page->prefs->hex_irc_logmask, &folder, &pattern);
        native = g_filename_from_utf8 (folder, -1, NULL, NULL, NULL);
        if (native) gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), native);
        g_free (folder); g_free (pattern); g_free (native);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
        g_signal_connect_object (dialog, "response", G_CALLBACK (page_log_folder_response), page->root, 0);
        gtk_widget_show (dialog);
}

static void
page_default_log_folder (GtkButton *button, PreferencesPage *page)
{
        page_log_change_folder (page, NULL);
}

static void
page_history_refresh (PreferencesPage *page)
{
        struct zoitechatprefs *prefs = page->prefs;
        char *folder, *pattern, *path, stamp[256], *line;
        gboolean custom = !g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (page->log_style)), "custom");
        const char *input = gtk_entry_get_text (GTK_ENTRY (page->log_entry));
        gboolean input_valid = *input && strlen (input) < sizeof prefs->hex_irc_logmask &&
                !strpbrk (input, "\r\n") && !page_path_separator (input[strlen (input) - 1]);
        page_log_parts (page, prefs->hex_irc_logmask, &folder, &pattern);
        gtk_label_set_text (GTK_LABEL (page->log_folder), folder);
        page_folder_target (page->log_open, folder);
        gtk_widget_set_sensitive (page->log_options, prefs->hex_irc_logging);
        gtk_widget_set_visible (page->log_custom, custom);
        gtk_widget_set_sensitive (page->stamp_options, prefs->hex_stamp_log);
        gtk_widget_set_visible (gtk_widget_get_parent (page->stamp_entry),
                !g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (page->stamp_combo)), "custom"));
        gtk_widget_set_sensitive (page->stamp_entry,
                !g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (page->stamp_combo)), "custom"));
        path = input_valid ? log_build_pathname (prefs->hex_irc_logmask, page->config_dir,
                "irc.example.net", "#example", "ExampleNet", time (NULL)) : NULL;
        gtk_label_set_text (GTK_LABEL (page->log_file), path ? path : _("Enter a valid filename format to see an example."));
        page_status (page->log_status, !path ?
                _("The format must fit in 255 UTF-8 bytes and include a filename. Correct the custom format before saving changes to it.") :
                "");
        page_timestamp (page, stamp, sizeof stamp);
        line = g_strdup_printf ("%s<alice> %s", prefs->hex_stamp_log ? stamp : "", _("Hello! This is an example log entry."));
        gtk_label_set_text (GTK_LABEL (page->log_line), line);
        gtk_widget_set_sensitive (page->links_limit, prefs->hex_url_grabber);
        char *history_dir = g_build_filename (page->config_dir, "scrollback", NULL);
        page_folder_target (page->history_open, history_dir);
        gtk_widget_set_tooltip_text (page->history_open, prefs->hex_text_replay ?
                _("History folders are created as needed.") :
                _("Existing history is kept. Per-conversation overrides still apply."));
        g_free (history_dir); g_free (folder); g_free (pattern); g_free (path); g_free (line);
}

gboolean
preferences_history_page_stage (GtkWidget *widget, GError **error)
{
        PreferencesPage *page = g_object_get_data (G_OBJECT (widget), "preferences-page");
        const char *input;
        char *path;
        if (!page || page->kind != PAGE_HISTORY || !page->log_dirty || !page->prefs->hex_irc_logging) return TRUE;
        input = gtk_entry_get_text (GTK_ENTRY (page->log_entry));
        path = *input && strlen (input) < sizeof page->prefs->hex_irc_logmask && !strpbrk (input, "\r\n") &&
                !page_path_separator (input[strlen (input) - 1]) ?
                log_build_pathname (input, page->config_dir, "irc.example.net", "#example", "ExampleNet", time (NULL)) : NULL;
        if (!path)
        {
                g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                        _("Enter a log filename format of at most 255 UTF-8 bytes, including a filename and no line breaks."));
                gtk_widget_grab_focus (gtk_widget_get_visible (page->log_custom) ? page->log_entry : page->log_style);
                return FALSE;
        }
        g_free (path);
        return TRUE;
}

GtkWidget *
preferences_history_page_new (struct zoitechatprefs *prefs, const char *config_dir)
{
        PreferencesPage *page = page_new (PAGE_HISTORY, prefs);
        GtkWidget *box, *row, *button, *details;
        const char *patterns[] = {"%n" G_DIR_SEPARATOR_S "%c.log", "%n" G_DIR_SEPARATOR_S "%Y-%m" G_DIR_SEPARATOR_S "%c.log",
                "%n" G_DIR_SEPARATOR_S "%c" G_DIR_SEPARATOR_S "%Y-%m-%d.log", NULL};
        const char *const pattern_names[] = {N_("By network and conversation"), N_("By network and month"), N_("Daily file for each conversation"), NULL};
        const char *formats[] = {"%H:%M ", "%I:%M %p ", "[%H:%M:%S] ", "%Y-%m-%d %H:%M:%S ", NULL};
        const char *const format_names[] = {N_("24-hour · 14:35"), N_("12-hour · 02:35 PM"), N_("Bracketed · [14:35:08]"), N_("Date and time"), NULL};
        char *history_dir, *folder, *pattern, *url_file;
        page->config_dir = g_strdup (config_dir);
        page->custom_logmask = g_strdup (prefs->hex_irc_logmask);
        box = page_section (page->root, _("Conversation history"),
                _("The conversation buffer holds messages while you chat. Saved history can bring recent messages back when you return; readable log files are configured separately below."));
        page_pack (box, page_label (_("Recent history restores messages when you reopen a conversation."), TRUE));
        page_spin (page, box, "history-line-limit", _("Conversation length (lines)"), &prefs->hex_text_max_lines, 100000);
        gtk_widget_set_tooltip_text (box, _("The line limit also controls saved-history trimming. Values 0–2 remove the limit for open conversations. Saved history has a separate built-in cap; log files are unaffected."));
        page_toggle (page, box, "restore-history", _("Save and restore recent history"), &prefs->hex_text_replay);
        history_dir = g_build_filename (config_dir, "scrollback", NULL);
        details = gtk_expander_new (_("History folder"));
        row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add (GTK_CONTAINER (details), row);
        page_path_label (row, "history-folder", history_dir);
        page->history_open = page_folder_button (row, "open-history-folder", _("Open history folder"), history_dir);
        page_pack (box, details);
        box = page_section (page->root, _("Saved logs"),
                _("Keep readable conversation files on disk. Turning this off does not delete files already saved."));
        page_pack (box, page_label (_("Log files keep a readable copy of conversations on disk."), TRUE));
        page_toggle (page, box, "save-logs", _("Save conversations to log files"), &prefs->hex_irc_logging);
        page->log_options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
        page_pack (box, page->log_options);
        page->log_style = gtk_combo_box_text_new ();
        gtk_widget_set_name (page->log_style, "log-file-style");
        for (int i = 0; patterns[i]; i++)
                gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->log_style), patterns[i], _(pattern_names[i]));
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->log_style), "custom", _("Custom filename format"));
        page_log_parts (page, prefs->hex_irc_logmask, &folder, &pattern);
        if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->log_style), pattern))
                gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->log_style), "custom");
        page_row (page->log_options, _("Organize files"), page->log_style);
        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        button = gtk_button_new_with_label (_("Choose folder…"));
        gtk_widget_set_name (button, "choose-log-folder");
        g_signal_connect (button, "clicked", G_CALLBACK (page_choose_log_folder), page);
        page_pack (row, button);
        button = gtk_button_new_with_label (_("Use default folder"));
        gtk_widget_set_name (button, "default-log-folder");
        g_signal_connect (button, "clicked", G_CALLBACK (page_default_log_folder), page);
        page_pack (row, button);
        page_pack (page->log_options, row);
        page->log_custom = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        page->log_entry = page_entry (page, page->log_custom, "log-file-format", _("Filename format"), prefs->hex_irc_logmask, sizeof prefs->hex_irc_logmask);
        gtk_widget_set_tooltip_text (page->log_entry, _("%n: network, %c: conversation, %s: server, %Y: year, %m: month, %d: day. Relative paths use the default log folder. Use %% for a literal percent sign."));
        gtk_widget_show_all (page->log_custom);
        gtk_widget_set_no_show_all (page->log_custom, TRUE);
        page_pack (page->log_options, page->log_custom);
        page_pack (box, page_label (_("Log folder"), TRUE));
        page->log_folder = page_path_label (box, "log-folder", folder);
        page->log_open = page_folder_button (box, "open-log-folder", _("Open log folder"), folder);
        details = gtk_expander_new (_("Filename example"));
        row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add (GTK_CONTAINER (details), row);
        page->log_file = page_path_label (row, "log-example-file", "");
        gtk_widget_set_tooltip_text (details, _("ExampleNet, #example, today's date."));
        page_pack (box, details);
        page->log_status = page_label ("", TRUE);
        page_pack (box, page->log_status);
        gtk_widget_set_tooltip_text (page->log_open, _("Folders are created when messages are logged. Available once the folder exists."));
        g_signal_connect (page->log_style, "changed", G_CALLBACK (page_log_style_changed), page);
        g_signal_connect (page->log_entry, "changed", G_CALLBACK (page_log_entry_changed), page);
        box = page_section (page->root, _("Log timestamps"), _("These affect saved log files. Conversation timestamps are configured in Text & layout."));
        page_toggle (page, box, "log-timestamps", _("Include a time with each saved message"), &prefs->hex_stamp_log);
        page->stamp_options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        page_pack (box, page->stamp_options);
        page->stamp_combo = gtk_combo_box_text_new ();
        gtk_widget_set_name (page->stamp_combo, "log-timestamp-style");
        for (int i = 0; formats[i]; i++)
                gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->stamp_combo), formats[i], _(format_names[i]));
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->stamp_combo), "custom", _("Custom format"));
        if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->stamp_combo), prefs->hex_stamp_log_format))
                gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->stamp_combo), "custom");
        page->custom_stamp = g_strdup (prefs->hex_stamp_log_format);
        page_row (page->stamp_options, _("Time style"), page->stamp_combo);
        page->stamp_entry = page_entry (page, page->stamp_options, "log-timestamp-format", _("Custom timestamp format"), prefs->hex_stamp_log_format, sizeof prefs->hex_stamp_log_format);
        gtk_widget_show_all (gtk_widget_get_parent (page->stamp_entry));
        gtk_widget_set_no_show_all (gtk_widget_get_parent (page->stamp_entry), TRUE);
        gtk_widget_set_tooltip_text (page->stamp_entry, _("%H: hour (00–23), %I: hour (01–12), %M: minute, %S: second, %p: AM/PM. Spaces are kept."));
        page->log_line = page_path_label (box, "log-example-line", "");
        g_signal_connect (page->stamp_combo, "changed", G_CALLBACK (page_stamp_changed), page);
        g_signal_connect (page->stamp_entry, "changed", G_CALLBACK (page_custom_stamp_changed), page);
        box = page_section (page->root, _("Collected links"), _("The URL grabber keeps a list of links found in chat. Saving links to a file is an independent choice."));
        page_toggle (page, box, "collect-links", _("Collect links in the URL grabber"), &prefs->hex_url_grabber);
        page->links_limit = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        page_pack (box, page->links_limit);
        page_spin (page, page->links_limit, "link-limit", _("Maximum collected links"), &prefs->hex_url_grabber_limit, 9999);
        gtk_widget_set_tooltip_text (page->links_limit, _("0 keeps an unlimited number of links."));
        page_toggle (page, box, "save-links", _("Also save discovered links to url.log"), &prefs->hex_url_logging);
        url_file = g_build_filename (config_dir, "url.log", NULL);
        page_path_label (box, "url-log-file", url_file);
        page_folder_button (box, "open-data-folder", _("Open data folder"), config_dir);
        gtk_widget_set_tooltip_text (box, _("Disabling collection or logging keeps existing files."));
        page_history_refresh (page);
        g_free (history_dir); g_free (folder); g_free (pattern); g_free (url_file);
        return page->root;
}

static gboolean
typing_has_language (const char *configured, const char *code)
{
        char **tokens = g_strsplit_set (configured, ", \t", -1);
        gboolean found = FALSE;
        for (int i = 0; tokens[i]; i++) if (!strcmp (tokens[i], code)) found = TRUE;
        g_strfreev (tokens);
        return found;
}

static void
typing_completion_example (GtkWidget *widget, PreferencesPage *page)
{
        const char *prefix = gtk_entry_get_text (GTK_ENTRY (page->completion_input));
        const char *names[] = {page->prefs->hex_completion_sort == 1 ? "Alice" : "Alex",
                               page->prefs->hex_completion_sort == 1 ? "Alex" : "Alice"};
        char *folded = g_utf8_casefold (prefix, -1);
        GPtrArray *matches = g_ptr_array_new ();
        for (guint i = 0; i < G_N_ELEMENTS (names); i++)
        {
                char *name = g_utf8_casefold (names[i], -1);
                if (*folded && g_str_has_prefix (name, folded)) g_ptr_array_add (matches, (gpointer)names[i]);
                g_free (name);
        }
        char *result;
        if (!matches->len) result = g_strdup (_("No sample match. Try Al, Alex or Alice."));
        else if (matches->len > 1 && (page->prefs->hex_completion_amount <= 0 ||
                 matches->len > (guint) page->prefs->hex_completion_amount))
                result = g_strdup_printf (_("Show matching names: %s, %s"), (char *)matches->pdata[0], (char *)matches->pdata[1]);
        else
        {
                char suffix[8] = "";
                gunichar ch = g_utf8_get_char_validated (page->prefs->hex_completion_suffix, -1);
                if (ch && ch != (gunichar)-1 && ch != (gunichar)-2) g_unichar_to_utf8 (ch, suffix);
                result = g_strdup_printf (_("Complete to: %s%s (followed by a space)"), (char *)matches->pdata[0], suffix);
        }
        gtk_label_set_text (GTK_LABEL (page->completion_result), result);
        g_free (result); g_free (folded); g_ptr_array_unref (matches);
}

static void
typing_refresh (PreferencesPage *page)
{
        struct zoitechatprefs *prefs = page->prefs;
        GString *missing = g_string_new ("");
        char **tokens = g_strsplit_set (prefs->hex_text_spell_langs, ", \t", -1);
        page->refreshing = TRUE;
        for (guint i = 0; i < page->language_buttons->len; i++)
        {
                GtkWidget *button = page->language_buttons->pdata[i];
                const char *code = g_object_get_data (G_OBJECT (button), "language-code");
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), typing_has_language (prefs->hex_text_spell_langs, code));
        }
        for (int i = 0; tokens[i]; i++)
        {
                gboolean available = FALSE;
                if (!*tokens[i]) continue;
                for (guint j = 0; j < page->language_buttons->len; j++)
                        if (!strcmp (tokens[i], g_object_get_data (G_OBJECT (page->language_buttons->pdata[j]), "language-code"))) available = TRUE;
                if (!available)
                {
                        if (missing->len) g_string_append (missing, ", ");
                        g_string_append (missing, tokens[i]);
                }
        }
        char *status = !page->language_buttons->len ? g_strdup (_("No dictionaries found. Install dictionaries and reopen Preferences.")) :
                missing->len ? g_strdup_printf (_("Additional language codes: %s"), missing->str) :
                g_strdup (
                !*prefs->hex_text_spell_langs ? _("No selection; ZoiteChat will try English (en).") : "");
        page_status (page->language_status, status);
        gtk_widget_set_sensitive (page->spelling_options, prefs->hex_gui_input_spell);
        gtk_widget_set_visible (page->typing_nick, prefs->hex_gui_input_nick);
        gtk_widget_set_sensitive (page->typing_mode, prefs->hex_gui_input_nick);
        gtk_label_set_text (GTK_LABEL (page->typing_nick), prefs->hex_gui_input_icon ? "● alice ▾" : "alice ▾");
        if (page->update_typing_preview) page->update_typing_preview (page->typing_preview, prefs);
        typing_completion_example (NULL, page);
        page->refreshing = FALSE;
        g_free (status); g_string_free (missing, TRUE); g_strfreev (tokens);
}

static void
typing_codes_changed (GtkEntry *entry, PreferencesPage *page)
{
        const char *text = gtk_entry_get_text (entry);
        if (strlen (text) >= sizeof page->prefs->hex_text_spell_langs)
        {
                page_status (page->language_status, _("Language codes must fit in 63 UTF-8 bytes. Shorten this list before saving."));
                return;
        }
        g_strlcpy (page->prefs->hex_text_spell_langs, text, sizeof page->prefs->hex_text_spell_langs);
        page_refresh (page);
}

static void
typing_suffix_changed (GtkEntry *entry, PreferencesPage *page)
{
        const char *text = gtk_entry_get_text (entry);
        if (strlen (text) < sizeof page->prefs->hex_completion_suffix)
                g_strlcpy (page->prefs->hex_completion_suffix, text, sizeof page->prefs->hex_completion_suffix);
        page_refresh (page);
        if (strlen (text) >= sizeof page->prefs->hex_completion_suffix)
        {
                gtk_label_set_text (GTK_LABEL (page->completion_result), _("This suffix is too long. Use a letter or punctuation mark."));
                GtkWidget *expander = gtk_widget_get_ancestor (page->completion_result, GTK_TYPE_EXPANDER);
                if (expander) gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);
        }
}

static void
typing_language_toggled (GtkToggleButton *button, PreferencesPage *page)
{
        if (page->refreshing) return;
        const char *code = g_object_get_data (G_OBJECT (button), "language-code");
        char **tokens = g_strsplit_set (page->prefs->hex_text_spell_langs, ", \t", -1);
        GString *value = g_string_new ("");
        for (int i = 0; tokens[i]; i++)
        {
                if (!*tokens[i] || !strcmp (tokens[i], code)) continue;
                if (value->len) g_string_append_c (value, ',');
                g_string_append (value, tokens[i]);
        }
        if (gtk_toggle_button_get_active (button))
        {
                if (value->len) g_string_append_c (value, ',');
                g_string_append (value, code);
        }
        if (value->len < sizeof page->prefs->hex_text_spell_langs)
                gtk_entry_set_text (GTK_ENTRY (page->language_entry), value->str);
        else
        {
                typing_refresh (page);
                page_status (page->language_status, _("The language list is full. Shorten the codes under Advanced before adding another dictionary."));
        }
        g_string_free (value, TRUE); g_strfreev (tokens);
}

static gboolean
typing_language_filter (GtkListBoxRow *row, gpointer data)
{
        PreferencesPage *page = data;
        const char *query = gtk_entry_get_text (GTK_ENTRY (page->language_search));
        const char *text = g_object_get_data (G_OBJECT (row), "language-search");
        char *folded = g_utf8_casefold (query, -1);
        gboolean visible = !*folded || strstr (text, folded);
        g_free (folded);
        return visible;
}

static void
typing_language_search (GtkEntry *entry, PreferencesPage *page)
{
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (page->language_list));
}

static void
typing_open_keyboard (GtkButton *button, gpointer unused)
{
        setup_open_page ("completion-shortcuts");
}

/* Some backends expose only locale tags when the optional ISO name database
 * is absent. Friendly fallbacks are display-only; backend tags stay untouched. */
static char *
typing_language_name (const char *code, const char *provided)
{
        if (provided && *provided && strcmp (provided, code)) return g_strdup (provided);
        static const struct { const char *id, *name; } languages[] = {
                {"en", N_("English")}, {"fr", N_("French")}, {"de", N_("German")}, {"es", N_("Spanish")},
                {"pt", N_("Portuguese")}, {"it", N_("Italian")}, {"nl", N_("Dutch")}, {"pl", N_("Polish")},
                {"uk", N_("Ukrainian")}, {"ru", N_("Russian")}, {"sv", N_("Swedish")}, {"da", N_("Danish")},
                {"nb", N_("Norwegian Bokmål")}, {"nn", N_("Norwegian Nynorsk")}, {"fi", N_("Finnish")},
                {"cs", N_("Czech")}, {"sk", N_("Slovak")}, {"hu", N_("Hungarian")}, {"ro", N_("Romanian")},
                {"el", N_("Greek")}, {"tr", N_("Turkish")}, {"ar", N_("Arabic")}, {"he", N_("Hebrew")},
                {"ja", N_("Japanese")}, {"ko", N_("Korean")}, {"zh", N_("Chinese")}, {"hi", N_("Hindi")},
        };
        static const struct { const char *id, *name; } regions[] = {
                {"CA", N_("Canada")}, {"US", N_("United States")}, {"GB", N_("United Kingdom")},
                {"AU", N_("Australia")}, {"NZ", N_("New Zealand")}, {"IE", N_("Ireland")},
                {"FR", N_("France")}, {"BE", N_("Belgium")}, {"CH", N_("Switzerland")},
                {"DE", N_("Germany")}, {"AT", N_("Austria")}, {"ES", N_("Spain")},
                {"MX", N_("Mexico")}, {"BR", N_("Brazil")}, {"PT", N_("Portugal")},
        };
        char **parts = g_strsplit_set (code, "_-.@", -1);
        const char *language = NULL, *region = parts[0] && parts[1] && *parts[1] ? parts[1] : NULL;
        for (guint i = 0; i < G_N_ELEMENTS (languages); i++)
                if (!strcmp (parts[0], languages[i].id)) language = _(languages[i].name);
        for (guint i = 0; region && i < G_N_ELEMENTS (regions); i++)
                if (!strcmp (region, regions[i].id)) { region = _(regions[i].name); break; }
        char *name = !language ? g_strdup (code) : region ? g_strdup_printf ("%s (%s)", language, region) : g_strdup (language);
        g_strfreev (parts);
        return name;
}

GtkWidget *
preferences_typing_page_new (struct zoitechatprefs *prefs, GtkWidget *preview_entry,
        const PreferencesSpellLanguage *languages, guint count, PreferencesTypingPreview update_preview)
{
        PreferencesPage *page = page_new (PAGE_TYPING, prefs);
        GtkWidget *box, *row, *scroll, *advanced, *codes, *button, *input_preview, *example;
        const char *const orders[] = {N_("Alphabetical order"), N_("Most recent speaker"), NULL};
        page->typing_preview = preview_entry;
        page->update_typing_preview = update_preview;
        page->language_buttons = g_ptr_array_new ();
        box = page_section (page->root, _("Message input"), NULL);
        input_preview = gtk_expander_new (_("Input preview"));
        gtk_widget_set_name (input_preview, "input-preview-expander");
        gtk_expander_set_expanded (GTK_EXPANDER (input_preview), TRUE);
        example = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add (GTK_CONTAINER (input_preview), example);
        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        page->typing_nick = page_label ("alice ▾", FALSE);
        gtk_widget_set_no_show_all (page->typing_nick, TRUE);
        page_pack (row, page->typing_nick);
        gtk_widget_set_hexpand (preview_entry, TRUE);
        gtk_entry_set_text (GTK_ENTRY (preview_entry), _("Hello, welcome to ZoiteChat."));
        gtk_widget_set_name (preview_entry, "typing-preview");
        atk_object_set_name (gtk_widget_get_accessible (preview_entry), _("Local typing preview; text is not sent"));
        gtk_box_pack_start (GTK_BOX (row), preview_entry, TRUE, TRUE, 0);
        page_pack (example, row);
        page_pack (example, page_label (_("Local preview; text is never sent."), TRUE));
        page_toggle (page, box, "input-formatting", _("Show colours and formatting in the input"), &prefs->hex_gui_input_attr);
        page_toggle (page, box, "input-nickname", _("Show nickname selector"), &prefs->hex_gui_input_nick);
        page->typing_mode = page_toggle (page, box, "input-mode-icon", _("Show channel status icon"), &prefs->hex_gui_input_icon);
        gtk_widget_set_tooltip_text (page->typing_mode, _("Shows your channel role, such as operator or voice."));
        page_pack (box, input_preview);
        box = page_section (page->root, _("Spelling"), _("Choose dictionaries reported by this system. Other configured language codes are preserved."));
        page_toggle (page, box, "spelling-enabled", _("Check spelling while I type"), &prefs->hex_gui_input_spell);
        page->spelling_options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        page_pack (box, page->spelling_options);
        page->language_search = gtk_search_entry_new ();
        gtk_widget_set_name (page->language_search, "spelling-search");
        gtk_entry_set_placeholder_text (GTK_ENTRY (page->language_search), _("Find a language or language code"));
        page_pack (page->spelling_options, page->language_search);
        page->language_list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (page->language_list), GTK_SELECTION_NONE);
        gtk_list_box_set_filter_func (GTK_LIST_BOX (page->language_list), typing_language_filter, page, NULL);
        gtk_list_box_set_placeholder (GTK_LIST_BOX (page->language_list), page_label (_("No matching installed dictionaries"), TRUE));
        for (guint i = 0; i < count; i++)
        {
                char *name = typing_language_name (languages[i].code, languages[i].name);
                char *label = !strcmp (name, languages[i].code) ? g_strdup (name) : g_strdup_printf ("%s · %s", name, languages[i].code);
                button = gtk_check_button_new_with_label (label);
                gtk_label_set_line_wrap (GTK_LABEL (gtk_bin_get_child (GTK_BIN (button))), TRUE);
                gtk_label_set_max_width_chars (GTK_LABEL (gtk_bin_get_child (GTK_BIN (button))), 52);
                g_object_set_data_full (G_OBJECT (button), "language-code", g_strdup (languages[i].code), g_free);
                gtk_container_add (GTK_CONTAINER (page->language_list), button);
                GtkWidget *list_row = gtk_widget_get_parent (button);
                g_object_set_data_full (G_OBJECT (list_row), "language-search", g_utf8_casefold (label, -1), g_free);
                g_ptr_array_add (page->language_buttons, button);
                g_signal_connect (button, "toggled", G_CALLBACK (typing_language_toggled), page);
                g_free (label); g_free (name);
        }
        scroll = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll), count ? 68 : 35);
        gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), 120);
        gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroll), TRUE);
        gtk_container_add (GTK_CONTAINER (scroll), page->language_list);
        page_pack (page->spelling_options, scroll);
        advanced = gtk_expander_new (_("Advanced language codes"));
        codes = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_set_border_width (GTK_CONTAINER (codes), 8);
        gtk_container_add (GTK_CONTAINER (advanced), codes);
        page->language_entry = gtk_entry_new ();
        gtk_widget_set_name (page->language_entry, "spelling-codes");
        atk_object_set_name (gtk_widget_get_accessible (page->language_entry), _("Dictionary language codes"));
        gtk_entry_set_text (GTK_ENTRY (page->language_entry), prefs->hex_text_spell_langs);
        page_pack (codes, page->language_entry);
        gtk_entry_set_placeholder_text (GTK_ENTRY (page->language_entry), "en_CA, en_US, fr_FR");
        gtk_widget_set_tooltip_text (page->language_entry, _("Separate codes with commas or spaces. Codes not listed on this system are preserved."));
        page_pack (page->spelling_options, advanced);
        page->language_status = page_label ("", TRUE);
        page_pack (box, page->language_status);
        g_signal_connect (page->language_search, "search-changed", G_CALLBACK (typing_language_search), page);
        g_signal_connect (page->language_entry, "changed", G_CALLBACK (typing_codes_changed), page);
        box = page_section (page->root, _("Nickname completion"), _("Completion finishes matching nicknames and commands. Your completion shortcut is configured under Keyboard shortcuts."));
        page->suffix_entry = gtk_entry_new ();
        gtk_widget_set_name (page->suffix_entry, "completion-suffix");
        atk_object_set_name (gtk_widget_get_accessible (page->suffix_entry), _("Nickname completion suffix"));
        gtk_entry_set_text (GTK_ENTRY (page->suffix_entry), prefs->hex_completion_suffix);
        gtk_entry_set_width_chars (GTK_ENTRY (page->suffix_entry), 6);
        gtk_entry_set_max_width_chars (GTK_ENTRY (page->suffix_entry), 6);
        page_row (box, _("Add after nickname"), page->suffix_entry);
        gtk_widget_set_tooltip_text (page->suffix_entry, _("First character only, followed by a space. Leave empty for no suffix."));
        page_combo (page, box, "completion-order", _("Match order"), &prefs->hex_completion_sort, 0, orders);
        gtk_widget_set_tooltip_text (page_spin (page, box, "completion-limit", _("List matches if more than"), &prefs->hex_completion_amount, 1000), _("Above this number, list matching names instead of completing. With 0, multiple matches are always listed; a unique match can still complete."));
        advanced = gtk_expander_new (_("Completion example"));
        gtk_widget_set_name (advanced, "completion-example-expander");
        example = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_start (example, 12);
        gtk_container_add (GTK_CONTAINER (advanced), example);
        page_pack (box, advanced);
        page->completion_input = gtk_entry_new ();
        gtk_widget_set_name (page->completion_input, "completion-example-input");
        gtk_entry_set_text (GTK_ENTRY (page->completion_input), "Al");
        atk_object_set_name (gtk_widget_get_accessible (page->completion_input), _("Sample nickname prefix"));
        page_pack (example, page_label (_("Alice (most recent) and Alex"), TRUE));
        page_row (example, _("Prefix"), page->completion_input);
        page->completion_result = page_label ("", FALSE);
        gtk_label_set_selectable (GTK_LABEL (page->completion_result), TRUE);
        page_pack (example, page->completion_result);
        button = gtk_button_new_with_label (_("Open keyboard shortcuts…"));
        gtk_widget_set_halign (button, GTK_ALIGN_START);
        g_signal_connect (button, "clicked", G_CALLBACK (typing_open_keyboard), NULL);
        page_pack (box, button);
        g_signal_connect (page->suffix_entry, "changed", G_CALLBACK (typing_suffix_changed), page);
        g_signal_connect (page->completion_input, "changed", G_CALLBACK (typing_completion_example), page);
        g_signal_connect (page->completion_input, "activate", G_CALLBACK (typing_completion_example), page);
        typing_refresh (page);
        return page->root;
}

gboolean
preferences_typing_page_stage (GtkWidget *widget, GError **error)
{
        PreferencesPage *page = g_object_get_data (G_OBJECT (widget), "preferences-page");
        if (!page || page->kind != PAGE_TYPING) return TRUE;
        GtkWidget *entries[] = {page->language_entry, page->suffix_entry};
        gsize limits[] = {sizeof page->prefs->hex_text_spell_langs, sizeof page->prefs->hex_completion_suffix};
        for (guint i = 0; i < G_N_ELEMENTS (entries); i++)
        {
                const char *text = gtk_entry_get_text (GTK_ENTRY (entries[i]));
                if (strlen (text) >= limits[i] || strpbrk (text, "\r\n"))
                {
                        g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                i ? _("The nickname suffix must fit in 3 UTF-8 bytes and stay on one line.") :
                                    _("Dictionary language codes must fit in 63 UTF-8 bytes and stay on one line."));
                        setup_reveal_widget (entries[i]);
                        gtk_widget_grab_focus (entries[i]);
                        return FALSE;
                }
        }
        return TRUE;
}

typedef struct
{
        const char *id, *title, *category, *data1, *data2;
} ShortcutPresentation;

static const ShortcutPresentation shortcut_presentations[] = {
        {"Run Command", N_("Run text or a command"), "Commands", N_("Text or command (supports escaped sequences)"), NULL},
        {"Change Page", N_("Switch conversation"), "Navigation", N_("Conversation, page number, or auto"), N_("Relative page flag (any non-empty value)")},
        {"Insert in Buffer", N_("Insert text at the cursor"), "Typing", N_("Text to insert (supports escaped sequences)"), NULL},
        {"Scroll Page", N_("Scroll the conversation"), "Navigation", N_("Direction: Top, Bottom, Up, Down, +1 or -1"), NULL},
        {"Set Buffer", N_("Replace the input text"), "Typing", N_("Replacement input text"), NULL},
        {"Last Command", N_("Previous input from history"), "Typing", NULL, NULL},
        {"Next Command", N_("Next input from history"), "Typing", NULL, NULL},
        {"Complete nick/command", N_("Complete a nickname or command"), "Typing", N_("Reverse completion flag (any non-empty value)"), NULL},
        {"Change Selected Nick", N_("Cycle nickname completion"), "Typing", N_("Reverse completion flag (any non-empty value)"), NULL},
        {"Check For Replace", N_("Apply text replacements"), "Typing", NULL, NULL},
        {"Move front tab left", N_("Move current tab left"), "Navigation", NULL, NULL},
        {"Move front tab right", N_("Move current tab right"), "Navigation", NULL, NULL},
        {"Move tab family left", N_("Move current network left"), "Navigation", NULL, NULL},
        {"Move tab family right", N_("Move current network right"), "Navigation", NULL, NULL},
        {"Push input line into history", N_("Save input to history without sending"), "Typing", NULL, NULL},
        {"Menu Shortcut", N_("Run an application action"), "Application", N_("Application action name or legacy number"), NULL},
        {"Reopen Closed Tab", N_("Reopen the last closed channel"), "Navigation", NULL, NULL},
};

typedef struct
{
        GtkWidget *root, *view, *search, *category, *count, *editor;
        GtkWidget *action, *key_label, *record, *duplicate, *remove, *up, *down;
        GtkWidget *fields, *advanced, *advanced_fields, *rows[2], *labels[2], *entries[2];
        GtkWidget *reference, *policy, *shared, *syntax;
        GtkListStore *store;
        GtkTreeModelFilter *filter;
        GtkTreeRowReference *selected;
        PreferencesShortcutAction *actions;
        guint action_count;
        PreferencesShortcutModifiers normalize;
        PreferencesShortcutsRestore restore;
        gpointer restore_data;
        gboolean updating, keep_selected;
} ShortcutsEditor;

static const ShortcutPresentation *
shortcut_presentation (const char *id)
{
        for (guint i = 0; i < G_N_ELEMENTS (shortcut_presentations); i++)
                if (!g_strcmp0 (id, shortcut_presentations[i].id)) return &shortcut_presentations[i];
        return NULL;
}

static const char *
shortcut_title (const char *id)
{
        const ShortcutPresentation *p = shortcut_presentation (id);
        return p ? _(p->title) : id ? id : _("Choose an action");
}

static gboolean
shortcuts_selected (ShortcutsEditor *ui, GtkTreeIter *iter)
{
        GtkTreePath *path = ui->selected ? gtk_tree_row_reference_get_path (ui->selected) : NULL;
        gboolean valid = path && gtk_tree_model_get_iter (GTK_TREE_MODEL (ui->store), iter, path);
        if (path) gtk_tree_path_free (path);
        return valid;
}

static void
shortcuts_free (gpointer data)
{
        ShortcutsEditor *ui = data;
        if (ui->selected) gtk_tree_row_reference_free (ui->selected);
        g_object_unref (ui->store);
        g_free (ui->actions);
        g_free (ui);
}

static gboolean
shortcuts_visible (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
        ShortcutsEditor *ui = data;
        if (ui->keep_selected && ui->selected)
        {
                GtkTreePath *a = gtk_tree_row_reference_get_path (ui->selected);
                GtkTreePath *b = gtk_tree_model_get_path (model, iter);
                gboolean same = a && !gtk_tree_path_compare (a, b);
                if (a) gtk_tree_path_free (a);
                gtk_tree_path_free (b);
                if (same) return TRUE;
        }
        char *key, *action, *d1, *d2;
        gtk_tree_model_get (model, iter, PREFERENCES_SHORTCUT_LABEL, &key, PREFERENCES_SHORTCUT_ACTION, &action,
                PREFERENCES_SHORTCUT_DATA1, &d1, PREFERENCES_SHORTCUT_DATA2, &d2, -1);
        const ShortcutPresentation *p = shortcut_presentation (action);
        const char *category = gtk_combo_box_get_active_id (GTK_COMBO_BOX (ui->category));
        gboolean visible = !category || !strcmp (category, "all") || !g_strcmp0 (category, p ? p->category : "Other");
        char *all = g_strdup_printf ("%s %s %s %s %s", key ? key : "", action ? action : "", shortcut_title (action), d1 ? d1 : "", d2 ? d2 : "");
        char *folded = g_utf8_casefold (all, -1);
        char *query = g_utf8_casefold (gtk_entry_get_text (GTK_ENTRY (ui->search)), -1);
        char **words = g_strsplit_set (query, " \t", -1);
        for (int i = 0; visible && words[i]; i++) if (*words[i] && !strstr (folded, words[i])) visible = FALSE;
        g_free (key); g_free (action); g_free (d1); g_free (d2); g_free (all); g_free (folded); g_free (query); g_strfreev (words);
        return visible;
}

static gboolean
shortcuts_filtered (ShortcutsEditor *ui)
{
        const char *category = gtk_combo_box_get_active_id (GTK_COMBO_BOX (ui->category));
        return *gtk_entry_get_text (GTK_ENTRY (ui->search)) || g_strcmp0 (category, "all");
}

static void
shortcuts_status (ShortcutsEditor *ui)
{
        GtkTreeIter selected, iter;
        guint key = 0, other_key, matches = 0;
        GdkModifierType mods = 0, other_mods;
        char *accel = NULL;
        gboolean custom = FALSE, valid = shortcuts_selected (ui, &selected);
        int total = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (ui->store), NULL);
        int visible = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (ui->filter), NULL);
        char *count = g_strdup_printf (_("%d of %d shortcuts"), visible, total);
        gtk_label_set_text (GTK_LABEL (ui->count), count); g_free (count);
        if (valid)
        {
                gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &selected, PREFERENCES_SHORTCUT_ACCEL, &accel, PREFERENCES_SHORTCUT_CUSTOM, &custom, -1);
                gtk_accelerator_parse (accel ? accel : "", &key, &mods);
                mods = ui->normalize (mods);
                if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ui->store), &iter)) do
                {
                        char *other;
                        gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &iter, PREFERENCES_SHORTCUT_ACCEL, &other, -1);
                        gtk_accelerator_parse (other ? other : "", &other_key, &other_mods);
                        if (key && key == other_key && mods == ui->normalize (other_mods)) matches++;
                        g_free (other);
                } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (ui->store), &iter));
        }
        char *shared = matches > 1 ? g_strdup_printf (_("%u bindings share this key; checked in list order."), matches) :
                g_strdup (valid && !key ? _("Assign a shortcut before saving this binding.") : "");
        page_status (ui->shared, valid ? shared : _("Select a shortcut to see its settings."));
        gtk_widget_set_sensitive (ui->remove, valid && custom);
        gtk_widget_set_sensitive (ui->duplicate, valid);
        gtk_widget_set_sensitive (ui->record, valid);
        for (int delta = -1; delta <= 1; delta += 2)
        {
                gboolean can_move = FALSE;
                if (valid && custom && !shortcuts_filtered (ui))
                {
                        GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (ui->store), &selected);
                        if (delta > 0) gtk_tree_path_next (path);
                        if ((delta > 0 || gtk_tree_path_prev (path)) && gtk_tree_model_get_iter (GTK_TREE_MODEL (ui->store), &iter, path))
                                gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &iter, PREFERENCES_SHORTCUT_CUSTOM, &can_move, -1);
                        gtk_tree_path_free (path);
                }
                gtk_widget_set_sensitive (delta < 0 ? ui->up : ui->down, can_move);
        }
        g_free (shared); g_free (accel);
}

static void
shortcuts_refilter (ShortcutsEditor *ui)
{
        gtk_tree_model_filter_refilter (ui->filter);
        shortcuts_status (ui);
}

static void
shortcuts_query_changed (GtkWidget *widget, ShortcutsEditor *ui)
{
        if (ui->updating) return;
        ui->keep_selected = FALSE;
        shortcuts_refilter (ui);
}

static void
shortcuts_selection_changed (GtkTreeSelection *selection, ShortcutsEditor *ui)
{
        if (ui->updating) return;
        GtkTreeModel *model;
        GtkTreeIter filtered, raw;
        gboolean valid = gtk_tree_selection_get_selected (selection, &model, &filtered), custom = FALSE;
        char *key = NULL, *accel = NULL, *action = NULL, *values[2] = {NULL, NULL};
        if (ui->selected) gtk_tree_row_reference_free (ui->selected);
        ui->selected = NULL;
        if (valid)
        {
                gtk_tree_model_filter_convert_iter_to_child_iter (ui->filter, &raw, &filtered);
                GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (ui->store), &raw);
                ui->selected = gtk_tree_row_reference_new (GTK_TREE_MODEL (ui->store), path);
                gtk_tree_path_free (path);
                gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &raw, PREFERENCES_SHORTCUT_LABEL, &key,
                        PREFERENCES_SHORTCUT_ACCEL, &accel, PREFERENCES_SHORTCUT_ACTION, &action,
                        PREFERENCES_SHORTCUT_DATA1, &values[0], PREFERENCES_SHORTCUT_DATA2, &values[1], PREFERENCES_SHORTCUT_CUSTOM, &custom, -1);
        }
        ui->updating = TRUE;
        gtk_widget_set_sensitive (ui->editor, valid);
        gtk_label_set_text (GTK_LABEL (ui->key_label), key && *key ? key : _("Unassigned"));
        gtk_label_set_text (GTK_LABEL (ui->syntax), accel ? accel : "");
        if (!action || !gtk_combo_box_set_active_id (GTK_COMBO_BOX (ui->action), action)) gtk_combo_box_set_active (GTK_COMBO_BOX (ui->action), -1);
        gtk_widget_set_sensitive (ui->action, custom);
        const ShortcutPresentation *p = shortcut_presentation (action);
        for (int i = 0; i < 2; i++)
        {
                const char *label = p ? (i ? p->data2 : p->data1) : NULL;
                GtkWidget *parent = label ? ui->fields : ui->advanced_fields;
                if (gtk_widget_get_parent (ui->rows[i]) != parent)
                {
                        g_object_ref (ui->rows[i]);
                        gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (ui->rows[i])), ui->rows[i]);
                        page_pack (parent, ui->rows[i]);
                        g_object_unref (ui->rows[i]);
                }
                gtk_label_set_text (GTK_LABEL (ui->labels[i]), label ? _(label) :
                        i ? _("Stored parameter 2 (not used by this action)") : _("Stored parameter 1 (not used by this action)"));
                gtk_entry_set_text (GTK_ENTRY (ui->entries[i]), values[i] ? values[i] : "");
                gtk_editable_set_editable (GTK_EDITABLE (ui->entries[i]), custom);
                gtk_widget_set_sensitive (ui->entries[i], custom);
        }
        const char *help = "";
        for (guint i = 0; i < ui->action_count; i++) if (!g_strcmp0 (action, ui->actions[i].id)) help = _(ui->actions[i].help);
        GString *plain = g_string_new ("");
        for (const unsigned char *c = (const unsigned char *)help; *c; c++) if (*c >= 32 || *c == '\n' || *c == '\t') g_string_append_c (plain, *c);
        gtk_label_set_text (GTK_LABEL (ui->reference), plain->str);
        g_string_free (plain, TRUE);
        page_status (ui->policy, valid && !custom ? _("Built-in action. Duplicate to edit its action or parameters.") : "");
        ui->updating = FALSE;
        shortcuts_status (ui);
        g_free (key); g_free (accel); g_free (action); g_free (values[0]); g_free (values[1]);
}

void
preferences_shortcuts_reveal_path (GtkWidget *root, GtkTreePath *raw_path)
{
        ShortcutsEditor *ui = g_object_get_data (G_OBJECT (root), "shortcuts-editor");
        gtk_entry_set_text (GTK_ENTRY (ui->search), "");
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (ui->category), "all");
        ui->keep_selected = FALSE;
        shortcuts_refilter (ui);
        GtkTreePath *path = gtk_tree_model_filter_convert_child_path_to_path (ui->filter, raw_path);
        if (path)
        {
                gtk_tree_view_set_cursor (GTK_TREE_VIEW (ui->view), path, NULL, FALSE);
                gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (ui->view), path, NULL, FALSE, 0, 0);
                gtk_tree_path_free (path);
        }
}

static void
shortcuts_edit_changed (GtkWidget *widget, ShortcutsEditor *ui)
{
        GtkTreeIter iter;
        gboolean custom;
        if (ui->updating || !shortcuts_selected (ui, &iter)) return;
        gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &iter, PREFERENCES_SHORTCUT_CUSTOM, &custom, -1);
        if (!custom) return;
        ui->keep_selected = TRUE;
        ui->updating = TRUE;
        if (widget == ui->action)
        {
                const char *id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (ui->action));
                if (id) gtk_list_store_set (ui->store, &iter, PREFERENCES_SHORTCUT_ACTION, id, -1);
        }
        else for (int i = 0; i < 2; i++) if (widget == ui->entries[i])
                gtk_list_store_set (ui->store, &iter, i ? PREFERENCES_SHORTCUT_DATA2 : PREFERENCES_SHORTCUT_DATA1,
                        gtk_entry_get_text (GTK_ENTRY (widget)), -1);
        ui->updating = FALSE;
        shortcuts_refilter (ui);
        if (widget == ui->action) shortcuts_selection_changed (gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->view)), ui);
}

void
preferences_shortcuts_find_action (GtkWidget *root, const char *action_id)
{
        ShortcutsEditor *ui = g_object_get_data (G_OBJECT (root), "shortcuts-editor");
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (ui->category), "all");
        gtk_entry_set_text (GTK_ENTRY (ui->search), action_id);
        ui->keep_selected = FALSE;
        shortcuts_refilter (ui);
        if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (ui->filter), NULL))
        {
                GtkTreePath *path = gtk_tree_path_new_first ();
                gtk_tree_view_set_cursor (GTK_TREE_VIEW (ui->view), path, NULL, FALSE);
                gtk_tree_path_free (path);
        }
}

static void
shortcuts_model_changed (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, GtkWidget *root)
{
        ShortcutsEditor *ui = g_object_get_data (G_OBJECT (root), "shortcuts-editor");
        if (!ui->updating) shortcuts_refilter (ui);
}

static void
shortcuts_add (GtkButton *button, ShortcutsEditor *ui)
{
        GtkTreeIter source, added;
        char *values[5] = {NULL};
        gboolean duplicate = GTK_WIDGET (button) == ui->duplicate && shortcuts_selected (ui, &source);
        if (duplicate) gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &source, 0, &values[0], 1, &values[1], 2, &values[2], 3, &values[3], 4, &values[4], -1);
        ui->updating = TRUE;
        gtk_list_store_append (ui->store, &added);
        gtk_list_store_set (ui->store, &added, PREFERENCES_SHORTCUT_LABEL, duplicate ? values[0] : _("Unassigned"),
                PREFERENCES_SHORTCUT_ACCEL, values[1], PREFERENCES_SHORTCUT_ACTION, duplicate ? values[2] : ui->actions[0].id,
                PREFERENCES_SHORTCUT_DATA1, values[3], PREFERENCES_SHORTCUT_DATA2, values[4], PREFERENCES_SHORTCUT_CUSTOM, TRUE, -1);
        ui->updating = FALSE;
        GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (ui->store), &added);
        preferences_shortcuts_reveal_path (ui->root, path);
        gtk_tree_path_free (path);
        for (int i = 0; i < 5; i++) g_free (values[i]);
}

static void
shortcuts_remove (GtkButton *button, ShortcutsEditor *ui)
{
        GtkTreeIter iter;
        gboolean custom;
        if (!shortcuts_selected (ui, &iter)) return;
        gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &iter, PREFERENCES_SHORTCUT_CUSTOM, &custom, -1);
        if (!custom) return;
        ui->updating = TRUE;
        gtk_list_store_remove (ui->store, &iter);
        if (ui->selected) gtk_tree_row_reference_free (ui->selected);
        ui->selected = NULL;
        ui->updating = FALSE;
        shortcuts_refilter (ui);
        shortcuts_selection_changed (gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->view)), ui);
}

static void
shortcuts_move (GtkButton *button, ShortcutsEditor *ui)
{
        GtkTreeIter iter, neighbor;
        gboolean custom;
        if (shortcuts_filtered (ui) || !shortcuts_selected (ui, &iter)) return;
        gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &iter, PREFERENCES_SHORTCUT_CUSTOM, &custom, -1);
        if (!custom) return;
        GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (ui->store), &iter);
        if (GTK_WIDGET (button) == ui->down) gtk_tree_path_next (path);
        else if (!gtk_tree_path_prev (path)) { gtk_tree_path_free (path); return; }
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (ui->store), &neighbor, path))
        {
                gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &neighbor, PREFERENCES_SHORTCUT_CUSTOM, &custom, -1);
                if (custom) gtk_list_store_swap (ui->store, &iter, &neighbor);
        }
        gtk_tree_path_free (path);
        shortcuts_status (ui);
}

static void
shortcuts_restore (GtkButton *button, ShortcutsEditor *ui)
{
        ui->updating = TRUE;
        ui->restore (ui->restore_data);
        if (ui->selected) gtk_tree_row_reference_free (ui->selected);
        ui->selected = NULL;
        ui->updating = FALSE;
        GtkTreePath *path = gtk_tree_path_new_first ();
        preferences_shortcuts_reveal_path (ui->root, path);
        gtk_tree_path_free (path);
}

static gboolean
shortcuts_tree_keypress (GtkWidget *widget, GdkEventKey *event, ShortcutsEditor *ui)
{
        if (!(event->state & GDK_SHIFT_MASK) || (event->keyval != GDK_KEY_Up && event->keyval != GDK_KEY_Down)) return FALSE;
        GtkWidget *button = event->keyval == GDK_KEY_Up ? ui->up : ui->down;
        if (gtk_widget_get_sensitive (button)) shortcuts_move (GTK_BUTTON (button), ui);
        return TRUE;
}

typedef struct { GWeakRef owner; GtkTreeRowReference *row; GtkWidget *view; } ShortcutRecorder;

static void
shortcut_recorder_free (gpointer data)
{
        ShortcutRecorder *rec = data;
        g_weak_ref_clear (&rec->owner);
        gtk_tree_row_reference_free (rec->row);
        g_free (rec);
}

static void
shortcut_recorded (GtkCellRendererAccel *renderer, char *path, guint key, GdkModifierType mods, guint code, GtkWidget *dialog)
{
        ShortcutRecorder *rec = g_object_get_data (G_OBJECT (dialog), "shortcut-recorder");
        GtkWidget *root = g_weak_ref_get (&rec->owner);
        if (root && key)
        {
                ShortcutsEditor *ui = g_object_get_data (G_OBJECT (root), "shortcuts-editor");
                GtkTreePath *raw_path = gtk_tree_row_reference_get_path (rec->row);
                GtkTreeIter iter;
                if (raw_path && gtk_tree_model_get_iter (GTK_TREE_MODEL (ui->store), &iter, raw_path))
                {
                        mods = ui->normalize (mods);
                        if (key == GDK_KEY_Tab && (mods & GDK_SHIFT_MASK)) key = GDK_KEY_ISO_Left_Tab;
                        char *label = gtk_accelerator_get_label (key, mods), *accel = gtk_accelerator_name (key, mods);
                        ui->keep_selected = TRUE;
                        gtk_list_store_set (ui->store, &iter, PREFERENCES_SHORTCUT_LABEL, label, PREFERENCES_SHORTCUT_ACCEL, accel, -1);
                        shortcuts_selection_changed (gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->view)), ui);
                        g_free (label); g_free (accel);
                }
                if (raw_path) gtk_tree_path_free (raw_path);
        }
        if (root) g_object_unref (root);
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
}

static void
shortcut_record_map (GtkWidget *dialog, ShortcutRecorder *rec)
{
        GtkTreePath *path = gtk_tree_path_new_first ();
        gtk_widget_grab_focus (rec->view);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (rec->view), path, gtk_tree_view_get_column (GTK_TREE_VIEW (rec->view), 0), TRUE);
        gtk_tree_path_free (path);
}

static void
shortcut_record_cancelled (GtkCellRenderer *renderer, GtkWidget *dialog)
{
        if (!g_object_get_data (G_OBJECT (dialog), "recorder-closing"))
                gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}

static void
shortcut_record_cleared (GtkCellRendererAccel *renderer, char *path, GtkWidget *dialog)
{
        shortcut_record_cancelled (GTK_CELL_RENDERER (renderer), dialog);
}

static void
shortcut_record_response (GtkDialog *dialog, int response, gpointer unused)
{
        if (g_object_get_data (G_OBJECT (dialog), "recorder-closing")) return;
        g_object_set_data (G_OBJECT (dialog), "recorder-closing", GINT_TO_POINTER (1));
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
shortcuts_record (GtkButton *button, ShortcutsEditor *ui)
{
        GtkTreeIter iter;
        if (!shortcuts_selected (ui, &iter)) return;
        GtkWidget *top = gtk_widget_get_toplevel (ui->root);
        GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Press a shortcut"), GTK_IS_WINDOW (top) ? GTK_WINDOW (top) : NULL,
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, _("Cancel"), GTK_RESPONSE_CANCEL, NULL);
        gtk_widget_set_name (dialog, "shortcut-recorder");
        ShortcutRecorder *rec = g_new0 (ShortcutRecorder, 1);
        g_weak_ref_init (&rec->owner, ui->root);
        GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (ui->store), &iter);
        rec->row = gtk_tree_row_reference_new (GTK_TREE_MODEL (ui->store), path);
        gtk_tree_path_free (path);
        GtkWidget *box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        gtk_container_set_border_width (GTK_CONTAINER (box), 18);
        gtk_box_set_spacing (GTK_BOX (box), 12);
        page_pack (box, page_label (_("Press a key combination now. Esc cancels. Nothing is executed."), FALSE));
        GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING);
        GtkTreeIter sample;
        gtk_list_store_append (store, &sample);
        gtk_list_store_set (store, &sample, 0, _("Press a shortcut…"), -1);
        rec->view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
        g_object_unref (store);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (rec->view), FALSE);
        GtkCellRenderer *renderer = gtk_cell_renderer_accel_new ();
        g_object_set (renderer, "editable", TRUE,
#ifndef WIN32
                "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_OTHER,
#endif
                NULL);
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, "text", 0, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (rec->view), column);
        page_pack (box, rec->view);
        g_object_set_data_full (G_OBJECT (dialog), "shortcut-recorder", rec, shortcut_recorder_free);
        g_signal_connect_object (renderer, "accel-edited", G_CALLBACK (shortcut_recorded), dialog, 0);
        g_signal_connect_object (renderer, "editing-canceled", G_CALLBACK (shortcut_record_cancelled), dialog, 0);
        g_signal_connect_object (renderer, "accel-cleared", G_CALLBACK (shortcut_record_cleared), dialog, 0);
        g_signal_connect (dialog, "map", G_CALLBACK (shortcut_record_map), rec);
        g_signal_connect (dialog, "response", G_CALLBACK (shortcut_record_response), NULL);
        gtk_widget_show_all (dialog);
}

static void
shortcuts_cell (GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
        int field = GPOINTER_TO_INT (data);
        char *action, *d1, *d2;
        gboolean custom;
        gtk_tree_model_get (model, iter, PREFERENCES_SHORTCUT_ACTION, &action, PREFERENCES_SHORTCUT_DATA1, &d1, PREFERENCES_SHORTCUT_DATA2, &d2, PREFERENCES_SHORTCUT_CUSTOM, &custom, -1);
        char *text = field == 0 ? g_strdup (shortcut_title (action)) : field == 1 ?
                g_strescape (d1 && *d1 ? d1 : d2 && *d2 ? d2 : "", NULL) : g_strdup (custom ? _("Custom") : _("Built-in"));
        g_object_set (renderer, "text", text, NULL);
        g_free (text); g_free (action); g_free (d1); g_free (d2);
}

static GtkWidget *
shortcuts_button (GtkWidget *row, const char *name, const char *label, GCallback callback, ShortcutsEditor *ui)
{
        GtkWidget *button = gtk_button_new_with_label (label);
        gtk_widget_set_name (button, name);
        g_signal_connect (button, "clicked", callback, ui);
        page_pack (row, button);
        return button;
}

GtkWidget *
preferences_shortcuts_page_new (GtkListStore *store, const PreferencesShortcutAction *actions, guint count,
        PreferencesShortcutModifiers normalize, PreferencesShortcutsRestore restore, gpointer data)
{
        ShortcutsEditor *ui = g_new0 (ShortcutsEditor, 1);
        GtkWidget *box, *row, *scroll, *reference;
        ui->root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        ui->store = g_object_ref (store);
        ui->actions = g_new (PreferencesShortcutAction, count);
        memcpy (ui->actions, actions, count * sizeof *actions);
        ui->action_count = count; ui->normalize = normalize; ui->restore = restore; ui->restore_data = data;
        g_object_set_data_full (G_OBJECT (ui->root), "shortcuts-editor", ui, shortcuts_free);
        g_object_set_data (G_OBJECT (ui->root), "bindings-store", store);
        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        ui->search = gtk_search_entry_new ();
        gtk_widget_set_name (ui->search, "shortcut-search");
        gtk_entry_set_placeholder_text (GTK_ENTRY (ui->search), _("Search keys, actions or commands"));
        gtk_box_pack_start (GTK_BOX (row), ui->search, TRUE, TRUE, 0);
        ui->category = gtk_combo_box_text_new ();
        gtk_widget_set_name (ui->category, "shortcut-category");
        const char *ids[] = {"all", "Navigation", "Typing", "Commands", "Application", "Other"};
        const char *titles[] = {N_("All actions"), N_("Navigation"), N_("Typing"), N_("Commands"), N_("Application"), N_("Other")};
        for (guint i = 0; i < G_N_ELEMENTS (ids); i++) gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (ui->category), ids[i], _(titles[i]));
        gtk_combo_box_set_active (GTK_COMBO_BOX (ui->category), 0);
        page_pack (row, ui->category); page_pack (ui->root, row);
        ui->count = page_label ("", TRUE); page_pack (ui->root, ui->count);
        ui->filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL));
        gtk_tree_model_filter_set_visible_func (ui->filter, shortcuts_visible, ui, NULL);
        ui->view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (ui->filter));
        g_object_unref (ui->filter);
        gtk_widget_set_name (ui->view, "shortcuts-list");
        gtk_tree_view_set_enable_search (GTK_TREE_VIEW (ui->view), FALSE);
        g_object_set_data (G_OBJECT (ui->root), "view", ui->view);
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui->view), -1, _("Shortcut"), renderer, "text", PREFERENCES_SHORTCUT_LABEL, NULL);
        const char *columns[] = {N_("Action"), N_("Parameters"), N_("Type")};
        for (int i = 0; i < 3; i++)
        {
                renderer = gtk_cell_renderer_text_new ();
                g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
                GtkTreeViewColumn *column = gtk_tree_view_column_new ();
                gtk_tree_view_column_set_title (column, _(columns[i]));
                gtk_tree_view_column_pack_start (column, renderer, TRUE);
                gtk_tree_view_column_set_cell_data_func (column, renderer, shortcuts_cell, GINT_TO_POINTER (i), NULL);
                gtk_tree_view_column_set_expand (column, i == 0);
                gtk_tree_view_column_set_min_width (column, i == 0 ? 170 : 65);
                if (i == 1) gtk_tree_view_column_set_max_width (column, 170);
                gtk_tree_view_append_column (GTK_TREE_VIEW (ui->view), column);
        }
        scroll = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll), 190);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (scroll), ui->view); page_pack (ui->root, scroll);
        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        shortcuts_button (row, "shortcut-add", _("Add"), G_CALLBACK (shortcuts_add), ui);
        ui->duplicate = shortcuts_button (row, "shortcut-duplicate", _("Duplicate"), G_CALLBACK (shortcuts_add), ui);
        ui->remove = shortcuts_button (row, "shortcut-remove", _("Remove"), G_CALLBACK (shortcuts_remove), ui);
        ui->up = shortcuts_button (row, "shortcut-up", _("Move up"), G_CALLBACK (shortcuts_move), ui);
        ui->down = shortcuts_button (row, "shortcut-down", _("Move down"), G_CALLBACK (shortcuts_move), ui);
        gtk_widget_set_tooltip_text (ui->up, _("Clear the search and choose All actions to reorder adjacent custom bindings."));
        gtk_widget_set_tooltip_text (ui->down, _("Clear the search and choose All actions to reorder adjacent custom bindings."));
        g_object_set_data (G_OBJECT (ui->root), "delete_button", ui->remove);
        page_pack (ui->root, row);
        box = page_section (ui->root, _("Selected shortcut"), NULL);
        ui->editor = box;
        ui->policy = page_label ("", TRUE); page_pack (box, ui->policy);
        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        ui->key_label = page_label ("", FALSE); gtk_widget_set_name (ui->key_label, "shortcut-key");
        gtk_box_pack_start (GTK_BOX (row), ui->key_label, TRUE, TRUE, 0);
        ui->record = shortcuts_button (row, "shortcut-record", _("Change shortcut…"), G_CALLBACK (shortcuts_record), ui);
        gtk_widget_set_halign (ui->record, GTK_ALIGN_START);
        page_pack (box, row);
        ui->action = gtk_combo_box_text_new ();
        gtk_widget_set_name (ui->action, "shortcut-action");
        for (guint i = 0; i < count; i++) gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (ui->action), actions[i].id, shortcut_title (actions[i].id));
        page_row (box, _("Action"), ui->action);
        ui->fields = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6); page_pack (box, ui->fields);
        ui->advanced = gtk_expander_new (_("Advanced"));
        ui->advanced_fields = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_add (GTK_CONTAINER (ui->advanced), ui->advanced_fields);
        for (int i = 0; i < 2; i++)
        {
                ui->rows[i] = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
                ui->labels[i] = page_label ("", FALSE); page_pack (ui->rows[i], ui->labels[i]);
                ui->entries[i] = gtk_entry_new ();
                gtk_widget_set_name (ui->entries[i], i ? "shortcut-data2" : "shortcut-data1");
                gtk_label_set_mnemonic_widget (GTK_LABEL (ui->labels[i]), ui->entries[i]);
                page_pack (ui->rows[i], ui->entries[i]); page_pack (ui->fields, ui->rows[i]);
                g_signal_connect (ui->entries[i], "changed", G_CALLBACK (shortcuts_edit_changed), ui);
        }
        ui->syntax = page_label ("", TRUE); gtk_label_set_selectable (GTK_LABEL (ui->syntax), TRUE);
        page_pack (ui->advanced_fields, ui->syntax); page_pack (box, ui->advanced);
        ui->shared = page_label ("", FALSE); gtk_widget_set_name (ui->shared, "shortcut-shared"); page_pack (box, ui->shared);
        reference = gtk_expander_new (_("Action reference"));
        ui->reference = page_label ("", TRUE); gtk_label_set_selectable (GTK_LABEL (ui->reference), TRUE);
        gtk_container_add (GTK_CONTAINER (reference), ui->reference); page_pack (box, reference);
        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *restore_button = shortcuts_button (row, "shortcut-restore", _("Restore built-in shortcuts"), G_CALLBACK (shortcuts_restore), ui);
        gtk_widget_set_tooltip_text (restore_button, _("Reset built-in keys and keep custom bindings. Applied when you save."));
        gtk_widget_set_tooltip_text (ui->shared, _("Repeated keys form an action chain in saved order. An action can stop the chain."));
        page_pack (ui->root, row);
        g_signal_connect (ui->search, "search-changed", G_CALLBACK (shortcuts_query_changed), ui);
        g_signal_connect (ui->category, "changed", G_CALLBACK (shortcuts_query_changed), ui);
        g_signal_connect (ui->action, "changed", G_CALLBACK (shortcuts_edit_changed), ui);
        g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->view)), "changed", G_CALLBACK (shortcuts_selection_changed), ui);
        g_signal_connect (ui->view, "key-press-event", G_CALLBACK (shortcuts_tree_keypress), ui);
        g_signal_connect_object (store, "row-changed", G_CALLBACK (shortcuts_model_changed), ui->root, 0);
        GtkTreePath *first = gtk_tree_path_new_first ();
        preferences_shortcuts_reveal_path (ui->root, first); gtk_tree_path_free (first);
        shortcuts_selection_changed (gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->view)), ui);
        gtk_widget_show_all (ui->root);
        return ui->root;
}

static void
page_refresh (PreferencesPage *page)
{
        if (page->refreshing) return;
        if (page->kind == PAGE_TEXT) page_text_preview (page);
        else if (page->kind == PAGE_HISTORY) page_history_refresh (page);
        else if (page->kind == PAGE_TYPING) typing_refresh (page);
        else if (page->kind == PAGE_TABS)
        {
                gboolean tree = page->prefs->hex_gui_tab_layout != 0;
                gtk_widget_set_visible (page->tree_options, tree);
                gtk_widget_set_visible (page->tab_options, !tree);
                for (int i = 0; i < 2; i++) gtk_widget_queue_draw (page->layout_drawings[i]);
        }
        else
        {
                if (page->tray_options) gtk_widget_set_sensitive (page->tray_options, page->prefs->hex_gui_tray);
                if (page->quiet_hidden) gtk_widget_set_sensitive (page->quiet_hidden, page->prefs->hex_gui_tray);
                for (int i = 0; i < 3; i++)
                        if (page->tray_alerts[i]) gtk_widget_set_sensitive (page->tray_alerts[i], page->prefs->hex_gui_tray);

        }
}
