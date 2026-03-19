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

#include <stdio.h>
#include <string.h>

#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "../gtkutil.h"
#include "../../common/fe.h"
#include "../../common/util.h"
#include "../../common/gtk3-theme-service.h"
#include "theme-gtk3.h"
#include "theme-manager.h"
#include "theme-preferences.h"

typedef struct
{
        GtkWindow *parent;
        GtkWidget *gtk3_combo;
        GtkWidget *gtk3_remove;
        gboolean gtk3_populating;
        struct zoitechatprefs *setup_prefs;
} theme_preferences_ui;

typedef struct
{
        GtkWidget *button;
        ThemeSemanticToken token;
        gboolean *color_change_flag;
        gpointer manager_ui;
} theme_color_dialog_data;

typedef struct
{
        GtkWidget *row;
        GtkWidget *button;
        GtkWidget *entry;
        GtkWidget *preview;
        ThemeSemanticToken token;
        char *search_text;
        gboolean *color_change_flag;
        GtkWindow *parent;
        gpointer manager_ui;
} theme_color_manager_row;

typedef struct
{
        theme_color_manager_row *row;
        GdkRGBA original;
        gboolean has_original;
} theme_manager_live_picker_data;

typedef struct
{
        GPtrArray *rows;
        GtkWidget *search_entry;
        gboolean *color_change_flag;
        GtkWidget *preview_window;
        GtkWidget *preview_chat;
        GtkWidget *preview_selected;
        GtkWidget *preview_marker;
        GtkWidget *preview_tab_new_data;
        GtkWidget *preview_tab_new_message;
        GtkWidget *preview_tab_highlight;
        GtkWidget *preview_tab_away;
        GtkWidget *preview_spell;
} theme_color_manager_ui;

#define COLOR_MANAGER_RESPONSE_RESET 1

typedef struct
{
        gboolean active;
        gboolean changed;
        gboolean snapshot_valid[THEME_TOKEN_COUNT];
        gboolean staged_valid[THEME_TOKEN_COUNT];
        GdkRGBA snapshot[THEME_TOKEN_COUNT];
        GdkRGBA staged[THEME_TOKEN_COUNT];
} theme_preferences_stage_state;

static theme_preferences_stage_state theme_preferences_stage;

static gboolean
theme_preferences_staged_get_color (ThemeSemanticToken token, GdkRGBA *rgba)
{
        if (token < 0 || token >= THEME_TOKEN_COUNT || !rgba)
                return FALSE;

        if (theme_preferences_stage.active && theme_preferences_stage.staged_valid[token])
        {
                *rgba = theme_preferences_stage.staged[token];
                return TRUE;
        }

        return theme_get_color (token, rgba);
}

static void
theme_preferences_stage_recompute_changed (void)
{
        ThemeSemanticToken token;

        theme_preferences_stage.changed = FALSE;
        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
        {
                if (!theme_preferences_stage.snapshot_valid[token] || !theme_preferences_stage.staged_valid[token])
                        continue;
                if (!gdk_rgba_equal (&theme_preferences_stage.snapshot[token], &theme_preferences_stage.staged[token]))
                {
                        theme_preferences_stage.changed = TRUE;
                        return;
                }
        }
}

static void
theme_preferences_stage_sync_runtime_to_snapshot (void)
{
        ThemeSemanticToken token;

        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
        {
                if (theme_preferences_stage.snapshot_valid[token])
                        theme_manager_set_token_color (ZOITECHAT_DARK_MODE_LIGHT, token,
                                                       &theme_preferences_stage.snapshot[token], NULL);
        }
}

static void
theme_preferences_stage_sync_runtime_to_staged (void)
{
        ThemeSemanticToken token;

        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
        {
                if (theme_preferences_stage.staged_valid[token])
                        theme_manager_set_token_color (ZOITECHAT_DARK_MODE_LIGHT, token,
                                                       &theme_preferences_stage.staged[token], NULL);
        }
}

static void
theme_preferences_staged_set_color (ThemeSemanticToken token, const GdkRGBA *rgba,
                                    gboolean *color_change_flag, gboolean live_preview)
{
        const GdkRGBA *preview_color = rgba;

        if (token < 0 || token >= THEME_TOKEN_COUNT || !rgba)
                return;

        if (theme_preferences_stage.active)
        {
                theme_preferences_stage.staged[token] = *rgba;
                theme_preferences_stage.staged_valid[token] = TRUE;
                theme_preferences_stage_recompute_changed ();
                if (color_change_flag)
                        *color_change_flag = theme_preferences_stage.changed;

                preview_color = &theme_preferences_stage.staged[token];
        }

        if (live_preview)
                theme_manager_set_token_color (ZOITECHAT_DARK_MODE_LIGHT, token, preview_color, NULL);
}

void
theme_preferences_stage_begin (void)
{
        ThemeSemanticToken token;

        memset (&theme_preferences_stage, 0, sizeof (theme_preferences_stage));
        theme_preferences_stage.active = TRUE;

        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
        {
                GdkRGBA rgba;

                if (!theme_preferences_staged_get_color (token, &rgba))
                        continue;

                theme_preferences_stage.snapshot[token] = rgba;
                theme_preferences_stage.staged[token] = rgba;
                theme_preferences_stage.snapshot_valid[token] = TRUE;
                theme_preferences_stage.staged_valid[token] = TRUE;
        }
}

void
theme_preferences_stage_apply (void)
{
        if (!theme_preferences_stage.active)
                return;

        theme_preferences_stage_sync_runtime_to_staged ();
}

void
theme_preferences_stage_commit (void)
{
        if (!theme_preferences_stage.active)
                return;

        theme_preferences_stage_apply ();
        memset (&theme_preferences_stage, 0, sizeof (theme_preferences_stage));
}

void
theme_preferences_stage_discard (void)
{
        if (!theme_preferences_stage.active)
                return;

        theme_preferences_stage_sync_runtime_to_snapshot ();
        memset (&theme_preferences_stage, 0, sizeof (theme_preferences_stage));
}

static void
theme_preferences_show_import_error (GtkWidget *button, const char *message);

static void
theme_preferences_manager_row_free (gpointer data)
{
        theme_color_manager_row *row = data;

        if (!row)
                return;

        g_free (row->search_text);
        g_free (row);
}

static void
theme_preferences_manager_ui_free (gpointer data)
{
        theme_color_manager_ui *ui = data;

        if (!ui)
                return;

        if (ui->rows)
                g_ptr_array_unref (ui->rows);
        g_free (ui);
}

static void
theme_preferences_manager_update_preview (theme_color_manager_ui *ui)
{
        GdkRGBA text_fg;
        GdkRGBA text_bg;
        GdkRGBA sel_fg;
        GdkRGBA sel_bg;
        GdkRGBA marker;
        GdkRGBA tab_new_data;
        GdkRGBA tab_new_message;
        GdkRGBA tab_highlight;
        GdkRGBA tab_away;
        GdkRGBA spell;
        GtkWidget *label;

        if (!ui)
                return;

        if (!theme_preferences_staged_get_color (THEME_TOKEN_TEXT_FOREGROUND, &text_fg)
            || !theme_preferences_staged_get_color (THEME_TOKEN_TEXT_BACKGROUND, &text_bg)
            || !theme_preferences_staged_get_color (THEME_TOKEN_SELECTION_FOREGROUND, &sel_fg)
            || !theme_preferences_staged_get_color (THEME_TOKEN_SELECTION_BACKGROUND, &sel_bg)
            || !theme_preferences_staged_get_color (THEME_TOKEN_MARKER, &marker)
            || !theme_preferences_staged_get_color (THEME_TOKEN_TAB_NEW_DATA, &tab_new_data)
            || !theme_preferences_staged_get_color (THEME_TOKEN_TAB_NEW_MESSAGE, &tab_new_message)
            || !theme_preferences_staged_get_color (THEME_TOKEN_TAB_HIGHLIGHT, &tab_highlight)
            || !theme_preferences_staged_get_color (THEME_TOKEN_TAB_AWAY, &tab_away)
            || !theme_preferences_staged_get_color (THEME_TOKEN_SPELL, &spell))
                return;

        gtkutil_apply_palette (ui->preview_window, &text_bg, &text_fg, NULL);
        gtkutil_apply_palette (ui->preview_chat, &text_bg, &text_fg, NULL);
        label = g_object_get_data (G_OBJECT (ui->preview_chat), "zoitechat-preview-label");
        if (GTK_IS_WIDGET (label))
                gtkutil_apply_palette (label, NULL, &text_fg, NULL);

        gtkutil_apply_palette (ui->preview_selected, &sel_bg, &sel_fg, NULL);
        label = g_object_get_data (G_OBJECT (ui->preview_selected), "zoitechat-preview-label");
        if (GTK_IS_WIDGET (label))
                gtkutil_apply_palette (label, NULL, &sel_fg, NULL);

        gtkutil_apply_palette (ui->preview_marker, &marker, &text_fg, NULL);
        label = g_object_get_data (G_OBJECT (ui->preview_marker), "zoitechat-preview-label");
        if (GTK_IS_WIDGET (label))
                gtkutil_apply_palette (label, NULL, &text_fg, NULL);

        gtkutil_apply_palette (ui->preview_tab_new_data, &tab_new_data, &text_fg, NULL);
        label = g_object_get_data (G_OBJECT (ui->preview_tab_new_data), "zoitechat-preview-label");
        if (GTK_IS_WIDGET (label))
                gtkutil_apply_palette (label, NULL, &text_fg, NULL);

        gtkutil_apply_palette (ui->preview_tab_new_message, &tab_new_message, &text_fg, NULL);
        label = g_object_get_data (G_OBJECT (ui->preview_tab_new_message), "zoitechat-preview-label");
        if (GTK_IS_WIDGET (label))
                gtkutil_apply_palette (label, NULL, &text_fg, NULL);

        gtkutil_apply_palette (ui->preview_tab_highlight, &tab_highlight, &text_fg, NULL);
        label = g_object_get_data (G_OBJECT (ui->preview_tab_highlight), "zoitechat-preview-label");
        if (GTK_IS_WIDGET (label))
                gtkutil_apply_palette (label, NULL, &text_fg, NULL);

        gtkutil_apply_palette (ui->preview_tab_away, &tab_away, &text_fg, NULL);
        label = g_object_get_data (G_OBJECT (ui->preview_tab_away), "zoitechat-preview-label");
        if (GTK_IS_WIDGET (label))
                gtkutil_apply_palette (label, NULL, &text_fg, NULL);

        gtkutil_apply_palette (ui->preview_spell, &text_bg, &spell, NULL);
        label = g_object_get_data (G_OBJECT (ui->preview_spell), "zoitechat-preview-label");
        if (GTK_IS_WIDGET (label))
                gtkutil_apply_palette (label, NULL, &spell, NULL);
}

static GtkWidget *
theme_preferences_manager_preview_item_new (const char *text)
{
        GtkWidget *box;
        GtkWidget *label;

        box = gtk_event_box_new ();
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (box), TRUE);
        gtk_container_set_border_width (GTK_CONTAINER (box), 3);

        label = gtk_label_new (text);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_container_add (GTK_CONTAINER (box), label);
        g_object_set_data (G_OBJECT (box), "zoitechat-preview-label", label);

        return box;
}

static GtkWidget *
theme_preferences_manager_create_preview (theme_color_manager_ui *ui)
{
        GtkWidget *frame;
        GtkWidget *vbox;
        GtkWidget *header;
        GtkWidget *chat_box;
        GtkWidget *tabs_box;
        GtkWidget *label;

        frame = gtk_frame_new (_("Live preview"));
        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
        gtk_container_add (GTK_CONTAINER (frame), vbox);

        ui->preview_window = gtk_event_box_new ();
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (ui->preview_window), TRUE);
        gtk_container_set_border_width (GTK_CONTAINER (ui->preview_window), 8);
        gtk_box_pack_start (GTK_BOX (vbox), ui->preview_window, TRUE, TRUE, 0);

        chat_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add (GTK_CONTAINER (ui->preview_window), chat_box);

        header = gtk_label_new (_("#zoitechat-preview"));
        gtk_widget_set_halign (header, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (chat_box), header, FALSE, FALSE, 0);

        ui->preview_chat = theme_preferences_manager_preview_item_new (_("<alice> Example chat message"));
        gtk_box_pack_start (GTK_BOX (chat_box), ui->preview_chat, FALSE, FALSE, 0);

        ui->preview_selected = theme_preferences_manager_preview_item_new (_("Selected text example"));
        gtk_box_pack_start (GTK_BOX (chat_box), ui->preview_selected, FALSE, FALSE, 0);

        ui->preview_marker = theme_preferences_manager_preview_item_new (_("Marker line"));
        gtk_widget_set_hexpand (ui->preview_marker, TRUE);
        gtk_box_pack_start (GTK_BOX (chat_box), ui->preview_marker, FALSE, FALSE, 0);

        ui->preview_spell = theme_preferences_manager_preview_item_new (_("mispelled wrd"));
        gtk_box_pack_start (GTK_BOX (chat_box), ui->preview_spell, FALSE, FALSE, 0);

        label = gtk_label_new (_("Tab states"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        tabs_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_box_pack_start (GTK_BOX (vbox), tabs_box, FALSE, FALSE, 0);

        ui->preview_tab_new_data = theme_preferences_manager_preview_item_new (_("New data"));
        gtk_widget_set_hexpand (ui->preview_tab_new_data, TRUE);
        gtk_box_pack_start (GTK_BOX (tabs_box), ui->preview_tab_new_data, TRUE, TRUE, 0);

        ui->preview_tab_new_message = theme_preferences_manager_preview_item_new (_("New message"));
        gtk_widget_set_hexpand (ui->preview_tab_new_message, TRUE);
        gtk_box_pack_start (GTK_BOX (tabs_box), ui->preview_tab_new_message, TRUE, TRUE, 0);

        ui->preview_tab_highlight = theme_preferences_manager_preview_item_new (_("Highlight"));
        gtk_widget_set_hexpand (ui->preview_tab_highlight, TRUE);
        gtk_box_pack_start (GTK_BOX (tabs_box), ui->preview_tab_highlight, TRUE, TRUE, 0);

        ui->preview_tab_away = theme_preferences_manager_preview_item_new (_("Away"));
        gtk_widget_set_hexpand (ui->preview_tab_away, TRUE);
        gtk_box_pack_start (GTK_BOX (tabs_box), ui->preview_tab_away, TRUE, TRUE, 0);

        theme_preferences_manager_update_preview (ui);

        return frame;
}

enum
{
        GTK3_THEME_COL_ID = 0,
        GTK3_THEME_COL_LABEL,
        GTK3_THEME_COL_SOURCE,
        GTK3_THEME_COL_THUMBNAIL,
        GTK3_THEME_COL_COUNT
};

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
	theme_manager_attach_window (dialog);
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

                gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &rgba);
                theme_preferences_staged_set_color (data->token,
                                                    &rgba,
                                                    data->color_change_flag,
                                                    TRUE);
                theme_preferences_color_button_apply (data->button, &rgba);
                theme_preferences_manager_update_preview ((theme_color_manager_ui *) data->manager_ui);
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

        if (!theme_preferences_staged_get_color (token, &rgba))
                return;
        dialog = gtk_color_chooser_dialog_new (_("Select color"), GTK_WINDOW (userdata));
	theme_manager_attach_window (dialog);
        gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog), &rgba);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        data = g_new0 (theme_color_dialog_data, 1);
        data->button = button;
        data->token = token;
        data->color_change_flag = g_object_get_data (G_OBJECT (button), "zoitechat-theme-color-change");
        data->manager_ui = g_object_get_data (G_OBJECT (button), "zoitechat-theme-color-manager-ui");
        g_signal_connect (dialog, "response", G_CALLBACK (theme_preferences_color_response_cb), data);
        gtk_widget_show (dialog);
}

static char *
theme_preferences_format_hex (const GdkRGBA *color)
{
        return g_strdup_printf ("#%02X%02X%02X",
                                (guint) CLAMP (color->red * 255.0 + 0.5, 0.0, 255.0),
                                (guint) CLAMP (color->green * 255.0 + 0.5, 0.0, 255.0),
                                (guint) CLAMP (color->blue * 255.0 + 0.5, 0.0, 255.0));
}

static char *
theme_preferences_token_display_name (ThemeSemanticToken token)
{
        if (token >= THEME_TOKEN_MIRC_0 && token <= THEME_TOKEN_MIRC_15)
                return g_strdup_printf (_("mIRC color %d"), token - THEME_TOKEN_MIRC_0);

        if (token >= THEME_TOKEN_MIRC_16 && token <= THEME_TOKEN_MIRC_31)
                return g_strdup_printf (_("Local color %d"), token - THEME_TOKEN_MIRC_0);

        switch (token)
        {
        case THEME_TOKEN_SELECTION_FOREGROUND:
                return g_strdup (_("Selected text foreground"));
        case THEME_TOKEN_SELECTION_BACKGROUND:
                return g_strdup (_("Selected text background"));
        case THEME_TOKEN_TEXT_FOREGROUND:
                return g_strdup (_("Text foreground"));
        case THEME_TOKEN_TEXT_BACKGROUND:
                return g_strdup (_("Text background"));
        case THEME_TOKEN_MARKER:
                return g_strdup (_("Marker line"));
        case THEME_TOKEN_TAB_NEW_DATA:
                return g_strdup (_("Tab: new data"));
        case THEME_TOKEN_TAB_HIGHLIGHT:
                return g_strdup (_("Tab: highlight"));
        case THEME_TOKEN_TAB_NEW_MESSAGE:
                return g_strdup (_("Tab: new message"));
        case THEME_TOKEN_TAB_AWAY:
                return g_strdup (_("Tab: away"));
        case THEME_TOKEN_SPELL:
                return g_strdup (_("Spell checker"));
        default:
                return g_strdup (_("Unknown color"));
        }
}

static void
theme_preferences_manager_row_apply (theme_color_manager_row *row, const GdkRGBA *rgba)
{
        char *hex;

        theme_preferences_color_button_apply (row->button, rgba);
        gtkutil_apply_palette (row->preview, rgba, NULL, NULL);
        hex = theme_preferences_format_hex (rgba);
        gtk_entry_set_text (GTK_ENTRY (row->entry), hex);
        g_free (hex);
}

static void
theme_preferences_manager_row_commit (theme_color_manager_row *row, const GdkRGBA *rgba)
{
        theme_preferences_staged_set_color (row->token, rgba, row->color_change_flag, TRUE);
        theme_preferences_manager_row_apply (row, rgba);
        theme_preferences_manager_update_preview ((theme_color_manager_ui *) row->manager_ui);
}

static void
theme_preferences_manager_entry_commit (theme_color_manager_row *row)
{
        GdkRGBA rgba;
        const char *text = gtk_entry_get_text (GTK_ENTRY (row->entry));

        if (!gdk_rgba_parse (&rgba, text))
        {
                if (theme_preferences_staged_get_color (row->token, &rgba))
                        theme_preferences_manager_row_apply (row, &rgba);
                return;
        }

        theme_preferences_manager_row_commit (row, &rgba);
}

static void
theme_preferences_manager_entry_activate_cb (GtkEntry *entry, gpointer user_data)
{
        (void) entry;
        theme_preferences_manager_entry_commit ((theme_color_manager_row *) user_data);
}

static gboolean
theme_preferences_manager_entry_focus_out_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
        (void) widget;
        (void) event;
        theme_preferences_manager_entry_commit ((theme_color_manager_row *) user_data);
        return FALSE;
}

static void
theme_preferences_manager_picker_notify_rgba_cb (GObject *object, GParamSpec *pspec, gpointer user_data)
{
        theme_manager_live_picker_data *data = user_data;
        GdkRGBA rgba;

        (void) pspec;
        if (!data || !data->row)
                return;

        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (object), &rgba);
        theme_preferences_manager_row_commit (data->row, &rgba);
}

static void
theme_preferences_manager_picker_response_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
        theme_manager_live_picker_data *data = user_data;

        if (data && data->row && data->has_original && response_id != GTK_RESPONSE_OK)
                theme_preferences_manager_row_commit (data->row, &data->original);

        gtk_widget_destroy (GTK_WIDGET (dialog));
        g_free (data);
}

static void
theme_preferences_manager_pick_cb (GtkWidget *button, gpointer user_data)
{
        theme_color_manager_row *row = user_data;
        GtkWidget *dialog;
        GdkRGBA rgba;
        theme_manager_live_picker_data *data;

        if (!theme_preferences_staged_get_color (row->token, &rgba))
                return;

        dialog = gtk_color_chooser_dialog_new (_("Select color"), row->parent);
        theme_manager_attach_window (dialog);
        gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog), &rgba);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        data = g_new0 (theme_manager_live_picker_data, 1);
        data->row = row;
        data->original = rgba;
        data->has_original = TRUE;

        g_signal_connect (G_OBJECT (dialog), "notify::rgba",
                          G_CALLBACK (theme_preferences_manager_picker_notify_rgba_cb), data);
        g_signal_connect (G_OBJECT (dialog), "response",
                          G_CALLBACK (theme_preferences_manager_picker_response_cb), data);
        gtk_widget_show (dialog);
        (void) button;
}

static gboolean
theme_preferences_manager_row_matches (theme_color_manager_row *row, const char *needle)
{
        if (!needle || !needle[0])
                return TRUE;

        return strstr (row->search_text, needle) != NULL;
}

static void
theme_preferences_manager_search_changed_cb (GtkEditable *editable, gpointer user_data)
{
        theme_color_manager_ui *ui = user_data;
        char *needle_lower;
        size_t i;

        needle_lower = g_utf8_strdown (gtk_entry_get_text (GTK_ENTRY (editable)), -1);
        for (i = 0; i < ui->rows->len; i++)
        {
                theme_color_manager_row *row = g_ptr_array_index (ui->rows, i);
                gtk_widget_set_visible (row->row, theme_preferences_manager_row_matches (row, needle_lower));
        }
        g_free (needle_lower);
}

static void
theme_preferences_manager_refresh_rows (theme_color_manager_ui *ui)
{
        size_t i;

        if (!ui || !ui->rows)
                return;

        for (i = 0; i < ui->rows->len; i++)
        {
                theme_color_manager_row *row = g_ptr_array_index (ui->rows, i);
                GdkRGBA rgba;

                if (theme_preferences_staged_get_color (row->token, &rgba))
                        theme_preferences_manager_row_apply (row, &rgba);
        }

        theme_preferences_manager_update_preview (ui);
}

static void
theme_preferences_manager_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
        theme_color_manager_ui *ui = user_data;

        if (response_id != COLOR_MANAGER_RESPONSE_RESET)
                return;

        {
                gboolean changed = FALSE;

                theme_manager_reset_mode_colors (ZOITECHAT_DARK_MODE_LIGHT, &changed);
                if (theme_preferences_stage.active)
                {
                        ThemeSemanticToken token;

                        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
                        {
                                GdkRGBA rgba;

                                if (!theme_get_color (token, &rgba))
                                        continue;
                                theme_preferences_stage.staged[token] = rgba;
                                theme_preferences_stage.staged_valid[token] = TRUE;
                        }
                        theme_preferences_stage_recompute_changed ();
                        if (ui->color_change_flag)
                                *ui->color_change_flag = theme_preferences_stage.changed;
                }
                else if (ui->color_change_flag)
                        *ui->color_change_flag = *ui->color_change_flag || changed;
        }

        theme_preferences_manager_refresh_rows (ui);
        g_signal_stop_emission_by_name (dialog, "response");
}

static GtkWidget *
theme_preferences_create_color_manager_dialog (GtkWindow *parent, gboolean *color_change_flag)
{
        GtkWidget *dialog;
        GtkWidget *content;
        GtkWidget *vbox;
        GtkWidget *content_hbox;
        GtkWidget *left_box;
        GtkWidget *search;
        GtkWidget *scroller;
        GtkWidget *list;
        GtkWidget *preview_frame;
        theme_color_manager_ui *ui;
        ThemeSemanticToken token;

        dialog = gtk_dialog_new_with_buttons (_("Manage client colors"),
                                              parent,
                                              GTK_DIALOG_MODAL,
                                              _("_Reset to GTK3 defaults"),
                                              COLOR_MANAGER_RESPONSE_RESET,
                                              _("_Close"),
                                              GTK_RESPONSE_CLOSE,
                                              NULL);
        theme_manager_attach_window (dialog);
        gtk_window_set_default_size (GTK_WINDOW (dialog), 760, 560);

        ui = g_new0 (theme_color_manager_ui, 1);
        ui->rows = g_ptr_array_new_with_free_func (theme_preferences_manager_row_free);
        ui->color_change_flag = color_change_flag;
        g_object_set_data_full (G_OBJECT (dialog), "zoitechat-theme-color-manager", ui, theme_preferences_manager_ui_free);
        g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (theme_preferences_manager_dialog_response_cb), ui);

        content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
        gtk_widget_set_hexpand (vbox, TRUE);
        gtk_widget_set_vexpand (vbox, TRUE);
        gtk_box_pack_start (GTK_BOX (content), vbox, TRUE, TRUE, 0);

        content_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_hexpand (content_hbox, TRUE);
        gtk_widget_set_vexpand (content_hbox, TRUE);
        gtk_box_pack_start (GTK_BOX (vbox), content_hbox, TRUE, TRUE, 0);

        left_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_hexpand (left_box, TRUE);
        gtk_widget_set_vexpand (left_box, TRUE);
        gtk_box_pack_start (GTK_BOX (content_hbox), left_box, TRUE, TRUE, 0);

        search = gtk_search_entry_new ();
        gtk_entry_set_placeholder_text (GTK_ENTRY (search), _("Search colors by name"));
        gtk_box_pack_start (GTK_BOX (left_box), search, FALSE, FALSE, 0);
        ui->search_entry = search;

        scroller = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_set_hexpand (scroller, TRUE);
        gtk_widget_set_vexpand (scroller, TRUE);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX (left_box), scroller, TRUE, TRUE, 0);

        list = gtk_list_box_new ();
        gtk_widget_set_hexpand (list, TRUE);
        gtk_widget_set_vexpand (list, TRUE);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_container_add (GTK_CONTAINER (scroller), list);

        preview_frame = theme_preferences_manager_create_preview (ui);
        gtk_widget_set_size_request (preview_frame, 300, -1);
        gtk_widget_set_hexpand (preview_frame, FALSE);
        gtk_widget_set_vexpand (preview_frame, TRUE);
        gtk_box_pack_start (GTK_BOX (content_hbox), preview_frame, FALSE, TRUE, 0);

        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
        {
                theme_color_manager_row *row;
                GtkWidget *list_row;
                GtkWidget *hbox;
                GtkWidget *name;
                GtkWidget *preview;
                GtkWidget *button;
                GtkWidget *entry;
                GdkRGBA rgba;
                char *display;
                char *search_text;
                char *token_code;

                list_row = gtk_list_box_row_new ();
                hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
                gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
                gtk_container_add (GTK_CONTAINER (list_row), hbox);

                display = theme_preferences_token_display_name (token);
                name = gtk_label_new (display);
                gtk_widget_set_halign (name, GTK_ALIGN_START);
                gtk_widget_set_hexpand (name, TRUE);
                gtk_box_pack_start (GTK_BOX (hbox), name, TRUE, TRUE, 0);

                preview = gtk_label_new (_("Preview"));
                gtk_widget_set_size_request (preview, 90, -1);
                gtk_widget_set_halign (preview, GTK_ALIGN_CENTER);
                gtk_box_pack_start (GTK_BOX (hbox), preview, FALSE, FALSE, 0);

                button = gtk_button_new_with_label (_("Choose…"));
                gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

                entry = gtk_entry_new ();
                gtk_entry_set_width_chars (GTK_ENTRY (entry), 9);
                gtk_entry_set_max_length (GTK_ENTRY (entry), 9);
                gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);

                row = g_new0 (theme_color_manager_row, 1);
                row->row = list_row;
                row->button = button;
                row->entry = entry;
                row->preview = preview;
                row->token = token;
                row->color_change_flag = color_change_flag;
                row->parent = GTK_WINDOW (dialog);
                row->manager_ui = ui;

                token_code = g_strdup_printf ("token_%d", token);
                search_text = g_strconcat (display, " ", token_code, NULL);
                row->search_text = g_utf8_strdown (search_text, -1);
                g_free (token_code);
                g_free (search_text);

                if (theme_preferences_staged_get_color (token, &rgba))
                        theme_preferences_manager_row_apply (row, &rgba);

                g_signal_connect (G_OBJECT (button), "clicked",
                                  G_CALLBACK (theme_preferences_manager_pick_cb), row);
                g_object_set_data (G_OBJECT (button), "zoitechat-theme-color-manager-ui", ui);
                g_signal_connect (G_OBJECT (entry), "activate",
                                  G_CALLBACK (theme_preferences_manager_entry_activate_cb), row);
                g_signal_connect (G_OBJECT (entry), "focus-out-event",
                                  G_CALLBACK (theme_preferences_manager_entry_focus_out_cb), row);

                gtk_container_add (GTK_CONTAINER (list), list_row);
                g_ptr_array_add (ui->rows, row);
                g_free (display);
        }

        g_signal_connect (G_OBJECT (search), "changed",
                          G_CALLBACK (theme_preferences_manager_search_changed_cb), ui);

        theme_preferences_manager_update_preview (ui);

        gtk_widget_show_all (dialog);
        return dialog;
}

static void
theme_preferences_manage_colors_cb (GtkWidget *button, gpointer user_data)
{
        gboolean *color_change_flag = user_data;
        GtkWidget *dialog;

        dialog = theme_preferences_create_color_manager_dialog (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                                                color_change_flag);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        if (color_change_flag)
                *color_change_flag = theme_preferences_stage.active ? theme_preferences_stage.changed : *color_change_flag;
}

static void
theme_preferences_show_import_error (GtkWidget *button, const char *message)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s",
                                         message);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static gboolean
theme_preferences_parse_cfg_color (const char *cfg,
                                   const char *key,
                                   guint16 *red,
                                   guint16 *green,
                                   guint16 *blue)
{
        const char *line;
        size_t key_len;

        if (!cfg || !key || !red || !green || !blue)
                return FALSE;

        key_len = strlen (key);
        line = cfg;

        while (*line)
        {
                const char *line_end;
                const char *p;

                while (*line == '\n' || *line == '\r')
                        line++;
                if (!*line)
                        break;

                line_end = strchr (line, '\n');
                if (!line_end)
                        line_end = line + strlen (line);

                p = line;
                while (p < line_end && g_ascii_isspace (*p))
                        p++;

                if ((size_t) (line_end - p) > key_len &&
                    strncmp (p, key, key_len) == 0)
                {
                        unsigned int r;
                        unsigned int g;
                        unsigned int b;

                        p += key_len;
                        while (p < line_end && g_ascii_isspace (*p))
                                p++;
                        if (p < line_end && *p == '=')
                                p++;
                        while (p < line_end && g_ascii_isspace (*p))
                                p++;

                        if (sscanf (p, "%x %x %x", &r, &g, &b) == 3)
                        {
                                *red = (guint16) CLAMP (r, 0, 0xffff);
                                *green = (guint16) CLAMP (g, 0, 0xffff);
                                *blue = (guint16) CLAMP (b, 0, 0xffff);
                                return TRUE;
                        }
                }

                line = line_end;
        }

        return FALSE;
}

static gboolean
theme_preferences_read_import_color (const char *cfg,
                                     ThemeSemanticToken token,
                                     GdkRGBA *rgba)
{
        static const char *token_names[] = {
                "mirc_0", "mirc_1", "mirc_2", "mirc_3", "mirc_4", "mirc_5", "mirc_6", "mirc_7",
                "mirc_8", "mirc_9", "mirc_10", "mirc_11", "mirc_12", "mirc_13", "mirc_14", "mirc_15",
                "mirc_16", "mirc_17", "mirc_18", "mirc_19", "mirc_20", "mirc_21", "mirc_22", "mirc_23",
                "mirc_24", "mirc_25", "mirc_26", "mirc_27", "mirc_28", "mirc_29", "mirc_30", "mirc_31",
                "selection_foreground", "selection_background", "text_foreground", "text_background", "marker",
                "tab_new_data", "tab_highlight", "tab_new_message", "tab_away", "spell"
        };
        char key[256];
        guint16 red;
        guint16 green;
        guint16 blue;
        int legacy_key;

        if (token < 0 || token >= THEME_TOKEN_COUNT)
                return FALSE;

        g_snprintf (key, sizeof key, "theme.mode.light.token.%s", token_names[token]);
        if (!theme_preferences_parse_cfg_color (cfg, key, &red, &green, &blue))
        {
                legacy_key = token < 32 ? token : (token - 32) + 256;
                g_snprintf (key, sizeof key, "color_%d", legacy_key);
                if (!theme_preferences_parse_cfg_color (cfg, key, &red, &green, &blue))
                        return FALSE;
        }

        rgba->red = red / 65535.0;
        rgba->green = green / 65535.0;
        rgba->blue = blue / 65535.0;
        rgba->alpha = 1.0;
        return TRUE;
}

static void
theme_preferences_import_colors_conf_cb (GtkWidget *button, gpointer user_data)
{
        gboolean *color_change_flag = user_data;
        GtkWidget *dialog;
        char *path;
        char *cfg;
        GError *error = NULL;
        gboolean any_imported = FALSE;
        ThemeSemanticToken token;

        dialog = gtk_file_chooser_dialog_new (_("Import colors.conf colors"),
                                              GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                                              _("_Import"), GTK_RESPONSE_ACCEPT,
                                              NULL);
        gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
        gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), FALSE);

        if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
        {
                gtk_widget_destroy (dialog);
                return;
        }

        path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        gtk_widget_destroy (dialog);
        if (!path)
                return;

        if (!g_file_get_contents (path, &cfg, NULL, &error))
        {
                theme_preferences_show_import_error (button, _("Failed to read colors.conf file."));
                g_clear_error (&error);
                g_free (path);
                return;
        }

        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
        {
                GdkRGBA rgba;

                if (!theme_preferences_read_import_color (cfg, token, &rgba))
                        continue;

                theme_preferences_staged_set_color (token, &rgba, color_change_flag, TRUE);
                any_imported = TRUE;
        }

        if (!any_imported)
                theme_preferences_show_import_error (button, _("No importable colors were found in that colors.conf file."));
        else if (color_change_flag)
                *color_change_flag = theme_preferences_stage.active ? theme_preferences_stage.changed : *color_change_flag;

        g_free (cfg);
        g_free (path);
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
        if (theme_preferences_staged_get_color (token, &color))
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

GtkWidget *
theme_preferences_create_color_page (GtkWindow *parent,
                                     struct zoitechatprefs *setup_prefs,
                                     gboolean *color_change_flag)
{
        GtkWidget *tab;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *manage_colors_button;
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
        theme_preferences_create_header (tab, 15, N_("Color Stripping"));
        theme_preferences_create_strip_toggle (tab, 16, _("Messages"), &setup_prefs->hex_text_stripcolor_msg);
        theme_preferences_create_strip_toggle (tab, 17, _("Scrollback"), &setup_prefs->hex_text_stripcolor_replay);
        theme_preferences_create_strip_toggle (tab, 18, _("Topic"), &setup_prefs->hex_text_stripcolor_topic);

        manage_colors_button = gtk_button_new_with_label (_("Manage all client colors…"));
        gtk_widget_set_halign (manage_colors_button, GTK_ALIGN_START);
        gtk_widget_set_margin_start (manage_colors_button, LABEL_INDENT);
        gtk_widget_set_margin_top (manage_colors_button, 10);
        gtk_box_pack_start (GTK_BOX (box), manage_colors_button, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (manage_colors_button), "clicked",
                          G_CALLBACK (theme_preferences_manage_colors_cb), color_change_flag);

        return box;
}

static void
theme_preferences_open_gtk3_folder_cb (GtkWidget *button, gpointer user_data)
{
        char *themes_dir;

        (void)user_data;
        (void)button;
        themes_dir = zoitechat_gtk3_theme_service_get_user_themes_dir ();
        g_mkdir_with_parents (themes_dir, 0700);
        fe_open_url (themes_dir);
        g_free (themes_dir);
}

static char *
theme_preferences_gtk3_active_id (theme_preferences_ui *ui)
{
        GtkTreeIter iter;
        GtkTreeModel *model;
        char *id = NULL;

        if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (ui->gtk3_combo), &iter))
                return NULL;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (ui->gtk3_combo));
        gtk_tree_model_get (model, &iter, GTK3_THEME_COL_ID, &id, -1);
        return id;
}

static void
theme_preferences_gtk3_sync_remove_state (theme_preferences_ui *ui)
{
        GtkTreeIter iter;
        GtkTreeModel *model;
        int source = -1;

        if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (ui->gtk3_combo), &iter))
        {
                gtk_widget_set_sensitive (ui->gtk3_remove, FALSE);
                return;
        }

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (ui->gtk3_combo));
        gtk_tree_model_get (model, &iter, GTK3_THEME_COL_SOURCE, &source, -1);
        gtk_widget_set_sensitive (ui->gtk3_remove, source == ZOITECHAT_GTK3_THEME_SOURCE_USER);
}

static void
theme_preferences_gtk3_changed_cb (GtkComboBox *combo, gpointer user_data)
{
        theme_preferences_ui *ui = user_data;
        char *id;
        gboolean selection_changed;
        ThemeGtk3Variant variant;
        GError *error = NULL;

        (void) combo;
        theme_preferences_gtk3_sync_remove_state (ui);
        if (ui->gtk3_populating)
                return;

        id = theme_preferences_gtk3_active_id (ui);
        if (!id)
                return;

        variant = theme_gtk3_variant_for_theme (id);
        selection_changed = g_strcmp0 (prefs.hex_gui_gtk3_theme, id) != 0
                            || prefs.hex_gui_gtk3_variant != variant;
        g_strlcpy (prefs.hex_gui_gtk3_theme, id, sizeof (prefs.hex_gui_gtk3_theme));
        prefs.hex_gui_gtk3_variant = variant;

        if (ui->setup_prefs)
        {
                g_strlcpy (ui->setup_prefs->hex_gui_gtk3_theme, id, sizeof (ui->setup_prefs->hex_gui_gtk3_theme));
                ui->setup_prefs->hex_gui_gtk3_variant = prefs.hex_gui_gtk3_variant;
        }

        if (selection_changed && !theme_gtk3_apply_current (&error))
        {
                theme_preferences_show_message (ui, GTK_MESSAGE_ERROR,
                                                error ? error->message : _("Failed to apply GTK3 theme."));
                g_clear_error (&error);
        }

        g_free (id);
}

static GdkPixbuf *
theme_preferences_load_thumbnail (const char *path)
{
	char *data = NULL;
	gsize length = 0;
	GdkPixbufLoader *loader;
	GError *error = NULL;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled;
	int width;
	int height;

	if (!path || !g_file_get_contents (path, &data, &length, &error))
	{
		g_clear_error (&error);
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();
	if (!gdk_pixbuf_loader_write (loader, (const guchar *) data, length, &error))
	{
		g_clear_error (&error);
		g_object_unref (loader);
		g_free (data);
		return NULL;
	}

	g_free (data);

	if (!gdk_pixbuf_loader_close (loader, &error))
	{
		g_clear_error (&error);
		g_object_unref (loader);
		return NULL;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (!pixbuf)
	{
		g_object_unref (loader);
		return NULL;
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	if (width > 48 || height > 48)
		scaled = gdk_pixbuf_scale_simple (pixbuf, 48, 48, GDK_INTERP_BILINEAR);
	else
		scaled = gdk_pixbuf_copy (pixbuf);

	g_object_unref (loader);
	return scaled;
}

static int
theme_preferences_gtk3_find_system_theme_index (GPtrArray *themes)
{
        GtkSettings *settings;
        char *system_theme = NULL;
        guint i;
        int found = -1;

        settings = gtk_settings_get_default ();
        if (!settings || !themes)
                return -1;

        g_object_get (G_OBJECT (settings), "gtk-theme-name", &system_theme, NULL);
        if (!system_theme || system_theme[0] == '\0')
        {
                g_free (system_theme);
                return -1;
        }

        for (i = 0; i < themes->len; i++)
        {
                ZoitechatGtk3Theme *theme = g_ptr_array_index (themes, i);

                if (theme && g_strcmp0 (theme->id, system_theme) == 0)
                {
                        found = (int) i;
                        break;
                }
        }

        g_free (system_theme);
        return found;
}

static void
theme_preferences_populate_gtk3 (theme_preferences_ui *ui)
{
        GPtrArray *themes;
        guint i;
        GtkTreeStore *store;
        GtkTreeIter iter;
        int active = -1;
        gboolean removed_selected_theme = FALSE;
        gboolean using_system_default = prefs.hex_gui_gtk3_theme[0] == '\0';
        gboolean should_apply = FALSE;
        char *final_id;
        ThemeGtk3Variant final_variant = THEME_GTK3_VARIANT_PREFER_LIGHT;
        GError *error = NULL;

        store = GTK_TREE_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (ui->gtk3_combo)));
        ui->gtk3_populating = TRUE;
        gtk_tree_store_clear (store);
        themes = zoitechat_gtk3_theme_service_discover ();
        for (i = 0; i < themes->len; i++)
        {
                ZoitechatGtk3Theme *theme = g_ptr_array_index (themes, i);
                char *label = g_strdup_printf ("%s (%s)", theme->display_name,
                                               theme->source == ZOITECHAT_GTK3_THEME_SOURCE_USER ? _("user") : _("system"));
                GdkPixbuf *thumbnail = NULL;

		if (theme->thumbnail_path && g_file_test (theme->thumbnail_path, G_FILE_TEST_IS_REGULAR))
			thumbnail = theme_preferences_load_thumbnail (theme->thumbnail_path);

                gtk_tree_store_append (store, &iter, NULL);
                gtk_tree_store_set (store, &iter,
                                    GTK3_THEME_COL_ID, theme->id,
                                    GTK3_THEME_COL_LABEL, label,
                                    GTK3_THEME_COL_SOURCE, theme->source,
                                    GTK3_THEME_COL_THUMBNAIL, thumbnail,
                                    -1);
                if (g_strcmp0 (prefs.hex_gui_gtk3_theme, theme->id) == 0)
                        active = i;
                if (thumbnail)
                        g_object_unref (thumbnail);
                g_free (label);
        }
        if (active < 0 && using_system_default)
                active = theme_preferences_gtk3_find_system_theme_index (themes);
        if (active >= 0)
                gtk_combo_box_set_active (GTK_COMBO_BOX (ui->gtk3_combo), active);
        else if (themes->len > 0)
        {
                gtk_combo_box_set_active (GTK_COMBO_BOX (ui->gtk3_combo), 0);
                if (!using_system_default)
                        removed_selected_theme = TRUE;
        }
        else if (prefs.hex_gui_gtk3_theme[0] != '\0')
                removed_selected_theme = TRUE;
        gtk_widget_set_sensitive (ui->gtk3_combo, themes->len > 0);
        theme_preferences_gtk3_sync_remove_state (ui);
        ui->gtk3_populating = FALSE;

        final_id = theme_preferences_gtk3_active_id (ui);
        if (final_id)
        {
                final_variant = theme_gtk3_variant_for_theme (final_id);
                if (!using_system_default || removed_selected_theme)
                {
                        should_apply = g_strcmp0 (prefs.hex_gui_gtk3_theme, final_id) != 0
                                       || prefs.hex_gui_gtk3_variant != final_variant
                                       || removed_selected_theme;
                        g_strlcpy (prefs.hex_gui_gtk3_theme, final_id, sizeof (prefs.hex_gui_gtk3_theme));
                        if (ui->setup_prefs)
                                g_strlcpy (ui->setup_prefs->hex_gui_gtk3_theme,
                                           final_id,
                                           sizeof (ui->setup_prefs->hex_gui_gtk3_theme));
                        prefs.hex_gui_gtk3_variant = final_variant;
                        if (ui->setup_prefs)
                                ui->setup_prefs->hex_gui_gtk3_variant = final_variant;
                }
                g_free (final_id);
        }

        if (should_apply && !theme_gtk3_apply_current (&error))
        {
                theme_preferences_show_message (ui, GTK_MESSAGE_ERROR,
                                                error ? error->message : _("Failed to apply GTK3 theme."));
                g_clear_error (&error);
        }

        g_ptr_array_unref (themes);
}

static void
theme_preferences_gtk3_import_path (theme_preferences_ui *ui, char *path)
{
        char *id = NULL;
        GError *error = NULL;

        if (!zoitechat_gtk3_theme_service_import (path, &id, &error))
                theme_preferences_show_message (ui, GTK_MESSAGE_ERROR,
                                                error ? error->message : _("Failed to import GTK3 theme."));
        g_clear_error (&error);
        g_free (id);
        g_free (path);
        theme_preferences_populate_gtk3 (ui);
}

static void
theme_preferences_gtk3_import_cb (GtkWidget *button, gpointer user_data)
{
        theme_preferences_ui *ui = user_data;
        GtkFileChooserNative *dialog;
        GtkFileFilter *filter;
        char *path;

        (void)button;
        dialog = gtk_file_chooser_native_new (_("Import GTK3 Theme"), ui->parent,
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              _("_Import"),
                                              _("_Cancel"));
        gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
        gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), FALSE);
        filter = gtk_file_filter_new ();
        gtk_file_filter_set_name (filter, _("Theme archives (*.zip, *.tar, *.tar.xz, *.tgz, *.tar.gz, *.tar.bz2)"));
        gtk_file_filter_add_pattern (filter, "*.zip");
        gtk_file_filter_add_pattern (filter, "*.tar");
        gtk_file_filter_add_pattern (filter, "*.tar.xz");
        gtk_file_filter_add_pattern (filter, "*.txz");
        gtk_file_filter_add_pattern (filter, "*.tar.gz");
        gtk_file_filter_add_pattern (filter, "*.tgz");
        gtk_file_filter_add_pattern (filter, "*.tar.bz2");
        gtk_file_filter_add_pattern (filter, "*.tbz");
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

        if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
        {
                g_object_unref (dialog);
                return;
        }

        path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        g_object_unref (dialog);
        theme_preferences_gtk3_import_path (ui, path);
}

static void
theme_preferences_gtk3_remove_cb (GtkWidget *button, gpointer user_data)
{
        theme_preferences_ui *ui = user_data;
        char *id;
        GError *error = NULL;

        (void)button;
        id = theme_preferences_gtk3_active_id (ui);
        if (!id)
                return;

        if (!zoitechat_gtk3_theme_service_remove_user_theme (id, &error))
                theme_preferences_show_message (ui, GTK_MESSAGE_ERROR,
                                                error ? error->message : _("Failed to remove GTK3 theme."));
        g_clear_error (&error);
        g_free (id);
        theme_preferences_populate_gtk3 (ui);
}

GtkWidget *
theme_preferences_create_page (GtkWindow *parent,
                               struct zoitechatprefs *setup_prefs,
                               gboolean *color_change_flag)
{
        theme_preferences_ui *ui;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *colors_frame;
        GtkWidget *colors_box;
        GtkWidget *manage_colors_button;
        GtkWidget *import_colors_button;
        GtkWidget *gtk3_frame;
        GtkWidget *gtk3_grid;
        GtkWidget *gtk3_button;
        GtkTreeStore *gtk3_store;
        GtkCellRenderer *renderer;

        ui = g_new0 (theme_preferences_ui, 1);
        ui->parent = parent;
        ui->setup_prefs = setup_prefs;

        box = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (box), 6);

        colors_frame = gtk_frame_new (_("Colors"));
        gtk_box_pack_start (GTK_BOX (box), colors_frame, FALSE, FALSE, 0);
        colors_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width (GTK_CONTAINER (colors_box), 6);
        gtk_container_add (GTK_CONTAINER (colors_frame), colors_box);

        label = gtk_label_new (_("GTK3 theme colors are used by default. Open the color manager to set custom colors."));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
        gtk_box_pack_start (GTK_BOX (colors_box), label, FALSE, FALSE, 0);

        manage_colors_button = gtk_button_new_with_label (_("Manage all client colors…"));
        gtk_widget_set_halign (manage_colors_button, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (colors_box), manage_colors_button, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (manage_colors_button), "clicked",
                          G_CALLBACK (theme_preferences_manage_colors_cb), color_change_flag);

        import_colors_button = gtk_button_new_with_label (_("Import colors.conf colors…"));
        gtk_widget_set_halign (import_colors_button, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (colors_box), import_colors_button, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (import_colors_button), "clicked",
                          G_CALLBACK (theme_preferences_import_colors_conf_cb), color_change_flag);

        gtk3_frame = gtk_frame_new (_("GTK3 Theme"));
        gtk_box_pack_start (GTK_BOX (box), gtk3_frame, FALSE, FALSE, 0);
        gtk3_grid = gtk_grid_new ();
        gtk_container_set_border_width (GTK_CONTAINER (gtk3_grid), 6);
        gtk_grid_set_row_spacing (GTK_GRID (gtk3_grid), 6);
        gtk_grid_set_column_spacing (GTK_GRID (gtk3_grid), 6);
        gtk_container_add (GTK_CONTAINER (gtk3_frame), gtk3_grid);

        label = gtk_label_new (_("GTK3 theme:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_grid_attach (GTK_GRID (gtk3_grid), label, 0, 0, 1, 1);
        gtk3_store = gtk_tree_store_new (GTK3_THEME_COL_COUNT,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_INT,
                                         GDK_TYPE_PIXBUF);
        ui->gtk3_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (gtk3_store));
        g_object_unref (gtk3_store);
        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ui->gtk3_combo), renderer, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (ui->gtk3_combo), renderer, "pixbuf", GTK3_THEME_COL_THUMBNAIL);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ui->gtk3_combo), renderer, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (ui->gtk3_combo), renderer, "text", GTK3_THEME_COL_LABEL);
        gtk_grid_attach (GTK_GRID (gtk3_grid), ui->gtk3_combo, 1, 0, 3, 1);
        g_signal_connect (G_OBJECT (ui->gtk3_combo), "changed", G_CALLBACK (theme_preferences_gtk3_changed_cb), ui);

        gtk3_button = gtk_button_new_with_mnemonic (_("Import"));
        gtk_grid_attach (GTK_GRID (gtk3_grid), gtk3_button, 0, 1, 1, 1);
        g_signal_connect (G_OBJECT (gtk3_button), "clicked", G_CALLBACK (theme_preferences_gtk3_import_cb), ui);

        ui->gtk3_remove = gtk_button_new_with_mnemonic (_("Remove"));
        gtk_grid_attach (GTK_GRID (gtk3_grid), ui->gtk3_remove, 1, 1, 1, 1);
        g_signal_connect (G_OBJECT (ui->gtk3_remove), "clicked", G_CALLBACK (theme_preferences_gtk3_remove_cb), ui);

        gtk3_button = gtk_button_new_with_mnemonic (_("Open theme folder"));
        gtk_grid_attach (GTK_GRID (gtk3_grid), gtk3_button, 2, 1, 2, 1);
        g_signal_connect (G_OBJECT (gtk3_button), "clicked", G_CALLBACK (theme_preferences_open_gtk3_folder_cb), ui);

        theme_preferences_populate_gtk3 (ui);

        g_object_set_data_full (G_OBJECT (box), "theme-preferences-ui", ui, g_free);

        return box;
}

void
theme_preferences_apply_to_session (session_gui *gui, InputStyle *input_style)
{
        if (gui->user_tree)
        {
                theme_manager_apply_userlist_style (gui->user_tree,
                                                    theme_manager_get_userlist_palette_behavior (input_style ? input_style->font_desc : NULL));
        }
}
