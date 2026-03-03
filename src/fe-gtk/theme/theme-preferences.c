#include <stdio.h>
#include <string.h>

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "../gtkutil.h"
#include "../../common/fe.h"
#include "../../common/util.h"
#include "../../common/cfgfiles.h"
#include "../../common/zoitechat.h"
#include "../../common/theme-service.h"
#include "../../common/zoitechatc.h"
#include "theme-css.h"
#include "theme-manager.h"
#include "theme-preferences.h"

typedef struct
{
        GtkWidget *combo;
        GtkWidget *apply_button;
        GtkWidget *status_label;
        GtkWindow *parent;
        gboolean *color_change_flag;
} theme_preferences_ui;

typedef struct
{
        GtkWidget *button;
        ThemeSemanticToken token;
        gboolean *color_change_flag;
} theme_color_dialog_data;

typedef struct
{
        struct zoitechatprefs *setup_prefs;
} theme_preferences_dark_mode_data;

#define LABEL_INDENT 12

static void
theme_preferences_show_message (theme_preferences_ui *ui, GtkMessageType message_type, const char *primary)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (ui->parent,
                                         GTK_DIALOG_MODAL,
                                         message_type,
                                         GTK_BUTTONS_OK,
                                         "%s",
                                         primary);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static void
theme_preferences_color_button_apply (GtkWidget *button, const GdkRGBA *color)
{
        GtkWidget *target = g_object_get_data (G_OBJECT (button), "zoitechat-color-box");
        GtkWidget *apply_widget = GTK_IS_WIDGET (target) ? target : button;

        gtkutil_apply_palette (apply_widget, color, NULL, NULL);

        if (apply_widget != button)
                gtkutil_apply_palette (button, color, NULL, NULL);

        gtk_widget_queue_draw (button);
}

static void
theme_preferences_color_response_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
        theme_color_dialog_data *data = user_data;

        if (response_id == GTK_RESPONSE_OK)
        {
                GdkRGBA rgba;
                gboolean changed = FALSE;

                gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &rgba);
                theme_manager_set_token_color (prefs.hex_gui_dark_mode,
                                               data->token,
                                               &rgba,
                                               &changed);
                if (data->color_change_flag)
                        *data->color_change_flag = *data->color_change_flag || changed;
                theme_preferences_color_button_apply (data->button, &rgba);
        }

        gtk_widget_destroy (GTK_WIDGET (dialog));
        g_free (data);
}

static void
theme_preferences_color_cb (GtkWidget *button, gpointer userdata)
{
        GtkWidget *dialog;
        ThemeSemanticToken token;
        GdkRGBA rgba;
        theme_color_dialog_data *data;

        token = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "zoitechat-theme-token"));

        if (!theme_get_color (token, &rgba))
                return;
        dialog = gtk_color_chooser_dialog_new (_("Select color"), GTK_WINDOW (userdata));
        gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog), &rgba);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        data = g_new0 (theme_color_dialog_data, 1);
        data->button = button;
        data->token = token;
        data->color_change_flag = g_object_get_data (G_OBJECT (button), "zoitechat-theme-color-change");
        g_signal_connect (dialog, "response", G_CALLBACK (theme_preferences_color_response_cb), data);
        gtk_widget_show (dialog);
}

static void
theme_preferences_create_color_button (GtkWidget *table,
                                       ThemeSemanticToken token,
                                       int row,
                                       int col,
                                       GtkWindow *parent,
                                       gboolean *color_change_flag)
{
        GtkWidget *but;
        GtkWidget *label;
        GtkWidget *box;
        char buf[64];
        GdkRGBA color;

        if (token > THEME_TOKEN_MIRC_31)
                strcpy (buf, "<span size=\"x-small\">&#x2007;&#x2007;</span>");
        else if (token < 10)
                sprintf (buf, "<span size=\"x-small\">&#x2007;%d</span>", token);
        else
                sprintf (buf, "<span size=\"x-small\">%d</span>", token);

        but = gtk_button_new ();
        label = gtk_label_new (" ");
        gtk_label_set_markup (GTK_LABEL (label), buf);
        box = gtk_event_box_new ();
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (box), TRUE);
        gtk_container_add (GTK_CONTAINER (box), label);
        gtk_container_add (GTK_CONTAINER (but), box);
        gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
        gtk_widget_show (label);
        gtk_widget_show (box);
        g_object_set_data (G_OBJECT (but), "zoitechat-color", (gpointer)1);
        g_object_set_data (G_OBJECT (but), "zoitechat-color-box", box);
        g_object_set_data (G_OBJECT (but), "zoitechat-theme-token", GINT_TO_POINTER (token));
        g_object_set_data (G_OBJECT (but), "zoitechat-theme-color-change", color_change_flag);
        gtk_grid_attach (GTK_GRID (table), but, col, row, 1, 1);
        g_signal_connect (G_OBJECT (but), "clicked", G_CALLBACK (theme_preferences_color_cb), parent);
        if (theme_get_color (token, &color))
                theme_preferences_color_button_apply (but, &color);
}

static void
theme_preferences_create_header (GtkWidget *table, int row, const char *labeltext)
{
        GtkWidget *label;
        char buf[128];

        if (row == 0)
                g_snprintf (buf, sizeof (buf), "<b>%s</b>", _(labeltext));
        else
                g_snprintf (buf, sizeof (buf), "\n<b>%s</b>", _(labeltext));

        label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (label), buf);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_grid_attach (GTK_GRID (table), label, 0, row, 4, 1);
        gtk_widget_set_margin_bottom (label, 5);
}

static void
theme_preferences_create_other_color_l (GtkWidget *tab,
                                        const char *text,
                                        ThemeSemanticToken token,
                                        int row,
                                        GtkWindow *parent,
                                        gboolean *color_change_flag)
{
        GtkWidget *label;

        label = gtk_label_new (text);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_start (label, LABEL_INDENT);
        gtk_grid_attach (GTK_GRID (tab), label, 2, row, 1, 1);
        theme_preferences_create_color_button (tab, token, row, 3, parent, color_change_flag);
}

static void
theme_preferences_create_other_color_r (GtkWidget *tab,
                                        const char *text,
                                        ThemeSemanticToken token,
                                        int row,
                                        GtkWindow *parent,
                                        gboolean *color_change_flag)
{
        GtkWidget *label;

        label = gtk_label_new (text);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_start (label, LABEL_INDENT);
        gtk_grid_attach (GTK_GRID (tab), label, 5, row, 4, 1);
        theme_preferences_create_color_button (tab, token, row, 9, parent, color_change_flag);
}

static void
theme_preferences_strip_toggle_cb (GtkToggleButton *toggle, gpointer user_data)
{
        int *field = user_data;

        *field = gtk_toggle_button_get_active (toggle);
}

static void
theme_preferences_create_strip_toggle (GtkWidget *tab,
                                       int row,
                                       const char *text,
                                       int *field)
{
        GtkWidget *toggle;

        toggle = gtk_check_button_new_with_label (text);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), *field);
        g_signal_connect (G_OBJECT (toggle), "toggled",
                          G_CALLBACK (theme_preferences_strip_toggle_cb), field);
        gtk_grid_attach (GTK_GRID (tab), toggle, 2, row, 1, 1);
}

static void
theme_preferences_dark_mode_changed_cb (GtkComboBox *combo, gpointer user_data)
{
        theme_preferences_dark_mode_data *data = user_data;

        data->setup_prefs->hex_gui_dark_mode = gtk_combo_box_get_active (combo);
}

static void
theme_preferences_create_dark_mode_menu (GtkWidget *tab,
                                         int row,
                                         struct zoitechatprefs *setup_prefs)
{
        static const char *const dark_mode_modes[] =
        {
                N_("Auto (system)"),
                N_("Dark"),
                N_("Light"),
                NULL
        };
        GtkWidget *label;
        GtkWidget *combo;
        GtkWidget *box;
        theme_preferences_dark_mode_data *data;
        int i;

        label = gtk_label_new (_("Dark mode:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_start (label, LABEL_INDENT);
        gtk_widget_set_tooltip_text (label,
                                     _("Choose how ZoiteChat selects its color palette for the chat buffer, channel list, and user list.\n"
                                       "This includes message colors, selection colors, and interface highlights.\n"));
        gtk_grid_attach (GTK_GRID (tab), label, 2, row, 1, 1);

        combo = gtk_combo_box_text_new ();
        for (i = 0; dark_mode_modes[i] != NULL; i++)
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(dark_mode_modes[i]));
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), setup_prefs->hex_gui_dark_mode);
        gtk_widget_set_tooltip_text (combo,
                                     _("Choose how ZoiteChat selects its color palette for the chat buffer, channel list, and user list.\n"
                                       "This includes message colors, selection colors, and interface highlights.\n"));

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start (GTK_BOX (box), combo, FALSE, FALSE, 0);
        gtk_grid_attach (GTK_GRID (tab), box, 3, row, 1, 1);

        data = g_new0 (theme_preferences_dark_mode_data, 1);
        data->setup_prefs = setup_prefs;
        g_signal_connect (G_OBJECT (combo), "changed",
                          G_CALLBACK (theme_preferences_dark_mode_changed_cb), data);
        g_object_set_data_full (G_OBJECT (combo), "zoitechat-dark-mode-data", data, g_free);
}

GtkWidget *
theme_preferences_create_color_page (GtkWindow *parent,
                                     struct zoitechatprefs *setup_prefs,
                                     gboolean *color_change_flag)
{
        GtkWidget *tab;
        GtkWidget *box;
        GtkWidget *label;
        int i;

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width (GTK_CONTAINER (box), 6);

        tab = gtk_grid_new ();
        gtk_container_set_border_width (GTK_CONTAINER (tab), 6);
        gtk_grid_set_row_spacing (GTK_GRID (tab), 2);
        gtk_grid_set_column_spacing (GTK_GRID (tab), 3);
        gtk_container_add (GTK_CONTAINER (box), tab);

        theme_preferences_create_header (tab, 0, N_("Text Colors"));

        label = gtk_label_new (_("mIRC colors:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_start (label, LABEL_INDENT);
        gtk_grid_attach (GTK_GRID (tab), label, 2, 1, 1, 1);

        for (i = 0; i < 16; i++)
                theme_preferences_create_color_button (tab,
                                                       THEME_TOKEN_MIRC_0 + i,
                                                       1,
                                                       i + 3,
                                                       parent,
                                                       color_change_flag);

        label = gtk_label_new (_("Local colors:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_start (label, LABEL_INDENT);
        gtk_grid_attach (GTK_GRID (tab), label, 2, 2, 1, 1);

        for (i = 16; i < 32; i++)
                theme_preferences_create_color_button (tab,
                                                       THEME_TOKEN_MIRC_0 + i,
                                                       2,
                                                       (i + 3) - 16,
                                                       parent,
                                                       color_change_flag);

        theme_preferences_create_other_color_l (tab, _("Foreground:"), THEME_TOKEN_TEXT_FOREGROUND, 3,
                                                parent, color_change_flag);
        theme_preferences_create_other_color_r (tab, _("Background:"), THEME_TOKEN_TEXT_BACKGROUND, 3,
                                                parent, color_change_flag);

        theme_preferences_create_header (tab, 5, N_("Selected Text"));
        theme_preferences_create_other_color_l (tab, _("Foreground:"), THEME_TOKEN_SELECTION_FOREGROUND, 6,
                                                parent, color_change_flag);
        theme_preferences_create_other_color_r (tab, _("Background:"), THEME_TOKEN_SELECTION_BACKGROUND, 6,
                                                parent, color_change_flag);

        theme_preferences_create_header (tab, 8, N_("Interface Colors"));
        theme_preferences_create_other_color_l (tab, _("New data:"), THEME_TOKEN_TAB_NEW_DATA, 9,
                                                parent, color_change_flag);
        theme_preferences_create_other_color_r (tab, _("Marker line:"), THEME_TOKEN_MARKER, 9,
                                                parent, color_change_flag);
        theme_preferences_create_other_color_l (tab, _("New message:"), THEME_TOKEN_TAB_NEW_MESSAGE, 10,
                                                parent, color_change_flag);
        theme_preferences_create_other_color_r (tab, _("Away user:"), THEME_TOKEN_TAB_AWAY, 10,
                                                parent, color_change_flag);
        theme_preferences_create_other_color_l (tab, _("Highlight:"), THEME_TOKEN_TAB_HIGHLIGHT, 11,
                                                parent, color_change_flag);
        theme_preferences_create_other_color_r (tab, _("Spell checker:"), THEME_TOKEN_SPELL, 11,
                                                parent, color_change_flag);
        theme_preferences_create_dark_mode_menu (tab, 13, setup_prefs);

        theme_preferences_create_header (tab, 15, N_("Color Stripping"));
        theme_preferences_create_strip_toggle (tab, 16, _("Messages"), &setup_prefs->hex_text_stripcolor_msg);
        theme_preferences_create_strip_toggle (tab, 17, _("Scrollback"), &setup_prefs->hex_text_stripcolor_replay);
        theme_preferences_create_strip_toggle (tab, 18, _("Topic"), &setup_prefs->hex_text_stripcolor_topic);

        return box;
}

static void
theme_preferences_populate (theme_preferences_ui *ui)
{
        GStrv themes;
        int count = 0;
        guint i;

        gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (ui->combo));

        themes = zoitechat_theme_service_discover_themes ();
        for (i = 0; themes[i] != NULL; i++)
        {
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ui->combo), themes[i]);
                count++;
        }
        g_strfreev (themes);

        gtk_widget_set_sensitive (ui->apply_button, count > 0);
        gtk_label_set_text (GTK_LABEL (ui->status_label),
                            count > 0 ? _("Select a theme to apply.") : _("No themes found."));
}

static void
theme_preferences_refresh_cb (GtkWidget *button, gpointer user_data)
{
        theme_preferences_ui *ui = user_data;

        (void)button;
        theme_preferences_populate (ui);
}

static void
theme_preferences_open_folder_cb (GtkWidget *button, gpointer user_data)
{
        theme_preferences_ui *ui = user_data;
        GAppInfo *handler;
        char *themes_dir;

        (void)button;
        themes_dir = zoitechat_theme_service_get_themes_dir ();
        g_mkdir_with_parents (themes_dir, 0700);

        handler = g_app_info_get_default_for_uri_scheme ("file");
        if (!handler)
        {
                theme_preferences_show_message (ui,
                                                GTK_MESSAGE_ERROR,
                                                _("No application is configured to open folders."));
                g_free (themes_dir);
                return;
        }

        g_object_unref (handler);
        fe_open_url (themes_dir);
        g_free (themes_dir);
}

static void
theme_preferences_selection_changed (GtkComboBox *combo, gpointer user_data)
{
        theme_preferences_ui *ui = user_data;
        gboolean has_selection = gtk_combo_box_get_active (combo) >= 0;

        gtk_widget_set_sensitive (ui->apply_button, has_selection);
}

static void
theme_preferences_apply_cb (GtkWidget *button, gpointer user_data)
{
        theme_preferences_ui *ui = user_data;
        GtkWidget *dialog;
        gint response;
        char *theme;
        GError *error = NULL;

        (void)button;
        theme = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (ui->combo));
        if (!theme)
                return;

        dialog = gtk_message_dialog_new (ui->parent, GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
                                         "%s", _("Applying a theme will overwrite your current colors and event settings.\nContinue?"));
        response = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        if (response != GTK_RESPONSE_OK)
        {
                g_free (theme);
                return;
        }

        if (!zoitechat_apply_theme (theme, &error))
        {
                theme_preferences_show_message (ui, GTK_MESSAGE_ERROR,
                                                error ? error->message : _("Failed to apply theme."));
                g_clear_error (&error);
                goto cleanup;
        }

        if (ui->color_change_flag)
                *ui->color_change_flag = TRUE;

        theme_preferences_show_message (ui,
                                        GTK_MESSAGE_INFO,
                                        _("Theme applied. Some changes may require a restart to take full effect."));

cleanup:
        g_free (theme);
}

GtkWidget *
theme_preferences_create_page (GtkWindow *parent, gboolean *color_change_flag)
{
        theme_preferences_ui *ui;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *hbox;
        GtkWidget *button_box;
        char *themes_dir;
        char *markup;

        ui = g_new0 (theme_preferences_ui, 1);
        ui->parent = parent;
        ui->color_change_flag = color_change_flag;

        box = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (box), 6);

        themes_dir = zoitechat_theme_service_get_themes_dir ();
        markup = g_markup_printf_escaped (_("Theme files are loaded from <tt>%s</tt>."), themes_dir);
        label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (label), markup);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
        g_free (markup);
        g_free (themes_dir);

        hbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 6);
        gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 0);

        ui->combo = gtk_combo_box_text_new ();
        gtk_box_pack_start (GTK_BOX (hbox), ui->combo, TRUE, TRUE, 0);
        g_signal_connect (G_OBJECT (ui->combo), "changed",
                          G_CALLBACK (theme_preferences_selection_changed), ui);

        button_box = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 6);
        gtk_box_pack_start (GTK_BOX (hbox), button_box, FALSE, FALSE, 0);

        ui->apply_button = gtk_button_new_with_mnemonic (_("_Apply Theme"));
        gtk_box_pack_start (GTK_BOX (button_box), ui->apply_button, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (ui->apply_button), "clicked",
                          G_CALLBACK (theme_preferences_apply_cb), ui);

        label = gtk_button_new_with_mnemonic (_("_Refresh"));
        gtk_box_pack_start (GTK_BOX (button_box), label, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (label), "clicked",
                          G_CALLBACK (theme_preferences_refresh_cb), ui);

        label = gtk_button_new_with_mnemonic (_("_Open Folder"));
        gtk_box_pack_start (GTK_BOX (button_box), label, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (label), "clicked",
                          G_CALLBACK (theme_preferences_open_folder_cb), ui);

        ui->status_label = gtk_label_new (NULL);
        gtk_widget_set_halign (ui->status_label, GTK_ALIGN_START);
        gtk_widget_set_valign (ui->status_label, GTK_ALIGN_CENTER);
        gtk_box_pack_start (GTK_BOX (box), ui->status_label, FALSE, FALSE, 0);

        theme_preferences_populate (ui);

        g_object_set_data_full (G_OBJECT (box), "theme-preferences-ui", ui, g_free);

        return box;
}

static void
theme_preferences_apply_entry_style (GtkWidget *entry, InputStyle *input_style)
{
        ThemeWidgetStyleValues style_values;

        theme_get_widget_style_values (&style_values);
        gtkutil_apply_palette (entry, &style_values.background, &style_values.foreground,
                               input_style ? input_style->font_desc : NULL);
}

void
theme_preferences_apply_to_session (session_gui *gui, InputStyle *input_style)
{
        if (prefs.hex_gui_input_style)
        {
                theme_css_reload_input_style (TRUE, input_style ? input_style->font_desc : NULL);
                theme_preferences_apply_entry_style (gui->input_box, input_style);
                theme_preferences_apply_entry_style (gui->limit_entry, input_style);
                theme_preferences_apply_entry_style (gui->key_entry, input_style);
                theme_preferences_apply_entry_style (gui->topic_entry, input_style);
        }

        if (gui->user_tree)
        {
                theme_manager_apply_userlist_style (gui->user_tree,
                                                    theme_manager_get_userlist_palette_behavior (input_style ? input_style->font_desc : NULL));
        }
}
