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
#include <math.h>

#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "../gtkutil.h"
#include "../../common/fe.h"
#include "../../common/cfgfiles.h"
#include "../../common/util.h"
#include "../../common/gtk3-theme-service.h"
#include "theme-gtk3.h"
#include "theme-manager.h"
#include "theme-preferences.h"
#include "../setup.h"
#include "theme-runtime.h"
#include "../textgui.h"

extern void load_text_events (void);
extern char **text_color_event_names (int color, int *count);

typedef struct
{
        GtkWindow *parent;
        GtkWidget *gtk3_combo;
        GtkWidget *gtk3_remove;
        GtkWidget *theme_cards;
        GtkWidget *theme_search;
        GtkWidget *style_buttons[3];
        GtkWidget *color_editor;
        GtkWidget *theme_expander;
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
        GtkWidget *status;
        GtkWidget *reset;
        GtkWidget *expander;
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
        gboolean original_custom;
} theme_manager_live_picker_data;

typedef struct
{
        GPtrArray *rows;
        GPtrArray *preview_parts;
        GtkWidget *root;
        GtkWidget *source;
        GtkWidget *advanced;
        GtkWidget *contrast;
        GtkWidget *contrast_fix;
        GtkWidget *scheme_buttons[4];
        GtkWidget *scheme_status;
        GtkWidget *match_swatches[4];
        guint listener;
        guint refresh_idle;
        gboolean syncing;
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
        /* Only explicitly edited tokens are written back to the runtime
         * palette; untouched tokens keep following the GTK3 theme. */
        gboolean staged_dirty[THEME_TOKEN_COUNT];
        gboolean snapshot_custom[THEME_TOKEN_COUNT];
        gboolean staged_custom[THEME_TOKEN_COUNT];
        GtkWidget *source;
        int dark_mode_snapshot;
        GdkRGBA snapshot[THEME_TOKEN_COUNT];
        GdkRGBA staged[THEME_TOKEN_COUNT];
        /* the GTK3 theme selection is previewed live from the combo, so
         * remember what was active when the dialog opened in order to
         * restore it when the dialog is cancelled */
        char gtk3_theme_snapshot[sizeof ((struct zoitechatprefs *) 0)->hex_gui_gtk3_theme];
        int gtk3_variant_snapshot;
} theme_preferences_stage_state;

static theme_preferences_stage_state theme_preferences_stage;

static gboolean
theme_preferences_staged_get_color (ThemeSemanticToken token, GdkRGBA *rgba)
{
        if (token < 0 || token >= THEME_TOKEN_COUNT || !rgba)
                return FALSE;

        if (theme_preferences_stage.active && theme_preferences_stage.staged_valid[token])
        {
                if (theme_preferences_stage.staged_custom[token])
                        *rgba = theme_preferences_stage.staged[token];
                else
                        return theme_get_default_color_for_widget (theme_preferences_stage.source, token, rgba);
                return TRUE;
        }
        if (theme_runtime_get_user_color (token, rgba)) return TRUE;
        return theme_get_default_color_for_widget (theme_preferences_stage.source, token, rgba);
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
                if (theme_preferences_stage.snapshot_custom[token] != theme_preferences_stage.staged_custom[token] ||
                    (theme_preferences_stage.staged_custom[token] &&
                     !gdk_rgba_equal (&theme_preferences_stage.snapshot[token], &theme_preferences_stage.staged[token])))
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
                if (theme_preferences_stage.snapshot_valid[token] &&
                    theme_preferences_stage.staged_dirty[token])
                {
                        if (theme_preferences_stage.snapshot_custom[token])
                                theme_manager_set_token_color (ZOITECHAT_DARK_MODE_LIGHT, token,
                                                               &theme_preferences_stage.snapshot[token], NULL);
                        else theme_runtime_reset_user_color (token);
                }
        }
        theme_manager_apply_and_dispatch (prefs.hex_gui_dark_mode,
                THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST, NULL);
}

static void
theme_preferences_stage_sync_runtime_to_staged (void)
{
        ThemeSemanticToken token;

        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
        {
                if (theme_preferences_stage.staged_valid[token] &&
                    theme_preferences_stage.staged_dirty[token])
                {
                        if (theme_preferences_stage.staged_custom[token])
                                theme_manager_set_token_color (ZOITECHAT_DARK_MODE_LIGHT, token,
                                                               &theme_preferences_stage.staged[token], NULL);
                        else theme_runtime_reset_user_color (token);
                }
        }
        theme_manager_apply_and_dispatch (prefs.hex_gui_dark_mode,
                THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST, NULL);
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
                theme_preferences_stage.staged_dirty[token] = TRUE;
                theme_preferences_stage.staged_custom[token] = TRUE;
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

        g_strlcpy (theme_preferences_stage.gtk3_theme_snapshot,
                   prefs.hex_gui_gtk3_theme,
                   sizeof (theme_preferences_stage.gtk3_theme_snapshot));
        theme_preferences_stage.gtk3_variant_snapshot = prefs.hex_gui_gtk3_variant;
        theme_preferences_stage.dark_mode_snapshot = prefs.hex_gui_dark_mode;

        for (token = THEME_TOKEN_MIRC_0; token < THEME_TOKEN_COUNT; token++)
        {
                GdkRGBA rgba;

                if (!theme_preferences_staged_get_color (token, &rgba))
                        continue;

                theme_preferences_stage.snapshot_custom[token] = theme_runtime_get_user_color (token, &rgba);
                theme_preferences_stage.staged_custom[token] = theme_preferences_stage.snapshot_custom[token];
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

/* undo a live GTK3 theme preview: put the selection that was active when
 * the dialog opened back and re-apply it */
static void
theme_preferences_stage_restore_gtk3 (void)
{
        if (g_strcmp0 (prefs.hex_gui_gtk3_theme, theme_preferences_stage.gtk3_theme_snapshot) == 0 &&
            prefs.hex_gui_gtk3_variant == theme_preferences_stage.gtk3_variant_snapshot &&
            prefs.hex_gui_dark_mode == theme_preferences_stage.dark_mode_snapshot)
                return;

        g_strlcpy (prefs.hex_gui_gtk3_theme,
                   theme_preferences_stage.gtk3_theme_snapshot,
                   sizeof (prefs.hex_gui_gtk3_theme));
        prefs.hex_gui_gtk3_variant = theme_preferences_stage.gtk3_variant_snapshot;
        prefs.hex_gui_dark_mode = theme_preferences_stage.dark_mode_snapshot;
        theme_gtk3_apply_current (NULL);
        theme_manager_apply_mode (prefs.hex_gui_dark_mode, NULL);
        theme_manager_dispatch_changed (THEME_CHANGED_REASON_THEME_PACK |
                                        THEME_CHANGED_REASON_PALETTE |
                                        THEME_CHANGED_REASON_WIDGET_STYLE |
                                        THEME_CHANGED_REASON_USERLIST |
                                        THEME_CHANGED_REASON_MODE);
}

void
theme_preferences_stage_revert_runtime (void)
{
        if (!theme_preferences_stage.active) return;
        theme_preferences_stage_restore_gtk3 ();
        theme_preferences_stage_sync_runtime_to_snapshot ();
}

void
theme_preferences_stage_discard (void)
{
        if (!theme_preferences_stage.active)
                return;

        /* restore the GTK3 theme first so the color snapshot below wins */
        theme_preferences_stage_restore_gtk3 ();
        theme_preferences_stage_sync_runtime_to_snapshot ();
        memset (&theme_preferences_stage, 0, sizeof (theme_preferences_stage));
}

static void
theme_preferences_show_import_error (GtkWidget *button, const char *message);

static void
theme_preferences_color_set_tooltip (GtkWidget *widget, ThemeSemanticToken token);

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

        if (ui->listener) theme_listener_unregister (ui->listener);
        if (ui->refresh_idle) g_source_remove (ui->refresh_idle);
        if (theme_preferences_stage.source == ui->source) theme_preferences_stage.source = NULL;
        if (ui->preview_parts) g_ptr_array_unref (ui->preview_parts);
        if (ui->rows) g_ptr_array_unref (ui->rows);
        g_free (ui);
}

static void theme_preferences_manager_focus_color (theme_color_manager_ui *ui, ThemeSemanticToken token);
static void theme_preferences_manager_sync_schemes (theme_color_manager_ui *ui);
static void theme_preferences_manager_refresh_rows (theme_color_manager_ui *ui);

static double
theme_preferences_luminance (const GdkRGBA *color)
{
        double values[] = { color->red, color->green, color->blue };
        for (guint i = 0; i < 3; i++)
                values[i] = values[i] <= 0.04045 ? values[i] / 12.92 : pow ((values[i] + 0.055) / 1.055, 2.4);
        return values[0] * 0.2126 + values[1] * 0.7152 + values[2] * 0.0722;
}

static double
theme_preferences_contrast_ratio (const GdkRGBA *foreground, const GdkRGBA *background)
{
        GdkRGBA visible = *foreground;
        visible.red = foreground->red * foreground->alpha + background->red * (1 - foreground->alpha);
        visible.green = foreground->green * foreground->alpha + background->green * (1 - foreground->alpha);
        visible.blue = foreground->blue * foreground->alpha + background->blue * (1 - foreground->alpha);
        double first = theme_preferences_luminance (&visible), second = theme_preferences_luminance (background);
        return (MAX (first, second) + 0.05) / (MIN (first, second) + 0.05);
}

static void
theme_preferences_manager_update_preview (theme_color_manager_ui *ui)
{
        GdkRGBA fg, bg, selected_fg, selected_bg;
        if (!ui || !ui->preview_parts) return;
        theme_preferences_staged_get_color (THEME_TOKEN_TEXT_FOREGROUND, &fg);
        theme_preferences_staged_get_color (THEME_TOKEN_TEXT_BACKGROUND, &bg);
        theme_preferences_staged_get_color (THEME_TOKEN_SELECTION_FOREGROUND, &selected_fg);
        theme_preferences_staged_get_color (THEME_TOKEN_SELECTION_BACKGROUND, &selected_bg);
        gtkutil_apply_palette (ui->preview_window, &bg, &fg, NULL);
        for (guint i = 0; i < ui->preview_parts->len; i++)
        {
                GtkWidget *part = g_ptr_array_index (ui->preview_parts, i);
                GdkRGBA part_fg, part_bg;
                ThemeSemanticToken foreground = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (part), "preview-foreground"));
                ThemeSemanticToken background = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (part), "preview-background"));
                theme_preferences_staged_get_color (foreground, &part_fg);
                theme_preferences_staged_get_color (background, &part_bg);
                gtkutil_apply_palette (part, &part_bg, &part_fg, NULL);
                gtkutil_apply_palette (gtk_bin_get_child (GTK_BIN (part)), NULL, &part_fg, NULL);
        }
        if (ui->contrast)
        {
                double normal = theme_preferences_contrast_ratio (&fg, &bg);
                double selected = theme_preferences_contrast_ratio (&selected_fg, &selected_bg);
                gboolean warning = normal < 4.5 || selected < 4.5;
                char *message = warning ? g_strdup_printf (_("Some text may be difficult to read (%.1f:1). Aim for at least 4.5:1. Your choice is still allowed."), MIN (normal, selected)) :
                        g_strdup ("");
                gtk_label_set_text (GTK_LABEL (ui->contrast), message);
                gtk_widget_set_visible (ui->contrast, warning);
                gtk_widget_set_visible (ui->contrast_fix, warning);
                g_free (message);
        }
        const ThemeSemanticToken swatches[] = { THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TEXT_FOREGROUND, THEME_TOKEN_TAB_HIGHLIGHT, THEME_TOKEN_TAB_NEW_MESSAGE };
        for (guint i = 0; i < G_N_ELEMENTS (swatches); i++)
                if (ui->match_swatches[i])
                {
                        GdkRGBA color;
                        theme_get_default_color_for_widget (theme_preferences_stage.source, swatches[i], &color);
                        gtkutil_apply_palette (ui->match_swatches[i], &color, NULL, NULL);
                }
        theme_preferences_manager_sync_schemes (ui);
}

static void
theme_preferences_preview_clicked (GtkButton *button, gpointer data)
{
        ThemeSemanticToken token = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "edit-token"));
        theme_preferences_manager_focus_color (data, token);
}

static GtkWidget *
theme_preferences_preview_part (theme_color_manager_ui *ui, const char *text,
                                ThemeSemanticToken foreground, ThemeSemanticToken background, ThemeSemanticToken edit)
{
        GtkWidget *button = gtk_button_new_with_label (text);
        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        gtk_widget_set_halign (button, GTK_ALIGN_FILL);
        gtk_label_set_xalign (GTK_LABEL (gtk_bin_get_child (GTK_BIN (button))), 0);
        gtk_label_set_line_wrap (GTK_LABEL (gtk_bin_get_child (GTK_BIN (button))), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (gtk_bin_get_child (GTK_BIN (button))), 25);
        g_object_set_data (G_OBJECT (button), "preview-foreground", GINT_TO_POINTER (foreground));
        g_object_set_data (G_OBJECT (button), "preview-background", GINT_TO_POINTER (background));
        g_object_set_data (G_OBJECT (button), "edit-token", GINT_TO_POINTER (edit));
        g_signal_connect (button, "clicked", G_CALLBACK (theme_preferences_preview_clicked), ui);
        gtk_widget_set_tooltip_text (button, _("Select to edit this colour. The same setting is available below the preview."));
        g_ptr_array_add (ui->preview_parts, button);
        return button;
}

static GtkWidget *
theme_preferences_manager_create_preview (theme_color_manager_ui *ui)
{
        GtkWidget *frame = gtk_frame_new (_("Preview"));
        GtkWidget *inside = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *body = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *channels = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *chat = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *line = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_tooltip_text (frame, _("Select part of the preview to edit its colour."));
        ui->preview_parts = g_ptr_array_new ();
        ui->preview_window = gtk_event_box_new ();
        gtk_container_set_border_width (GTK_CONTAINER (ui->preview_window), 8);
        gtk_container_add (GTK_CONTAINER (frame), ui->preview_window);
        gtk_container_add (GTK_CONTAINER (ui->preview_window), inside);
        gtk_box_pack_start (GTK_BOX (inside), theme_preferences_preview_part (ui, _("#zoitechat · Chat background"), THEME_TOKEN_TEXT_FOREGROUND, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TEXT_BACKGROUND), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (inside), body, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (body), channels, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (body), gtk_separator_new (GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (body), chat, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (channels), theme_preferences_preview_part (ui, _("#new messages"), THEME_TOKEN_TAB_NEW_MESSAGE, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TAB_NEW_MESSAGE), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (channels), theme_preferences_preview_part (ui, _("#mentions"), THEME_TOKEN_TAB_HIGHLIGHT, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TAB_HIGHLIGHT), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (channels), theme_preferences_preview_part (ui, _("#activity"), THEME_TOKEN_TAB_NEW_DATA, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TAB_NEW_DATA), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (channels), theme_preferences_preview_part (ui, _("alice (away)"), THEME_TOKEN_TAB_AWAY, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TAB_AWAY), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (chat), line, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (line), theme_preferences_preview_part (ui, "12:34 <alice>", THEME_TOKEN_MIRC_12, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_MIRC_12), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (line), theme_preferences_preview_part (ui, _("Hello! Welcome to ZoiteChat."), THEME_TOKEN_TEXT_FOREGROUND, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TEXT_FOREGROUND), TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (chat), theme_preferences_preview_part (ui, _("This is how your messages will look."), THEME_TOKEN_TEXT_FOREGROUND, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TEXT_FOREGROUND), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (chat), theme_preferences_preview_part (ui, _("Selected text · edit text colour"), THEME_TOKEN_SELECTION_FOREGROUND, THEME_TOKEN_SELECTION_BACKGROUND, THEME_TOKEN_SELECTION_FOREGROUND), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (chat), theme_preferences_preview_part (ui, _("Selection background"), THEME_TOKEN_SELECTION_FOREGROUND, THEME_TOKEN_SELECTION_BACKGROUND, THEME_TOKEN_SELECTION_BACKGROUND), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (inside), theme_preferences_preview_part (ui, _("Unread marker line"), THEME_TOKEN_MARKER, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_MARKER), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (inside), theme_preferences_preview_part (ui, _("Spelling warning"), THEME_TOKEN_SPELL, THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_SPELL), FALSE, FALSE, 0);
        return frame;
}

enum
{
        GTK3_THEME_COL_ID = 0,
        GTK3_THEME_COL_LABEL,
        GTK3_THEME_COL_SOURCE,
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
                theme_preferences_color_set_tooltip (data->button, data->token);
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
                return g_strdup_printf (_("IRC colour %d"), token - THEME_TOKEN_MIRC_0);

        if (token >= THEME_TOKEN_MIRC_16 && token <= THEME_TOKEN_MIRC_31)
                return g_strdup_printf (_("Nickname / local colour %d"), token - THEME_TOKEN_MIRC_0);

        switch (token)
        {
        case THEME_TOKEN_SELECTION_FOREGROUND:
                return g_strdup (_("Selected text"));
        case THEME_TOKEN_SELECTION_BACKGROUND:
                return g_strdup (_("Selected text background"));
        case THEME_TOKEN_TEXT_FOREGROUND:
                return g_strdup (_("Message text"));
        case THEME_TOKEN_TEXT_BACKGROUND:
                return g_strdup (_("Chat background"));
        case THEME_TOKEN_MARKER:
                return g_strdup (_("Marker line"));
        case THEME_TOKEN_TAB_NEW_DATA:
                return g_strdup (_("Channel activity"));
        case THEME_TOKEN_TAB_HIGHLIGHT:
                return g_strdup (_("Mentions"));
        case THEME_TOKEN_TAB_NEW_MESSAGE:
                return g_strdup (_("Unread messages"));
        case THEME_TOKEN_TAB_AWAY:
                return g_strdup (_("Away users"));
        case THEME_TOKEN_SPELL:
                return g_strdup (_("Spelling warnings"));
        default:
                return g_strdup (_("Unknown color"));
        }
}

static char *
theme_preferences_color_tooltip_markup (ThemeSemanticToken token)
{
        GString *tip;
        GdkRGBA rgba;
        char *display;
        char *hex;
        char *escaped;
        char **names;
        int count = 0;

        if (token < THEME_TOKEN_MIRC_0 || token > THEME_TOKEN_MIRC_31)
                return NULL;

        if (!theme_preferences_staged_get_color (token, &rgba))
                return NULL;

        tip = g_string_new (NULL);

        display = theme_preferences_token_display_name (token);
        hex = theme_preferences_format_hex (&rgba);
        escaped = g_markup_escape_text (display, -1);
        g_string_append_printf (tip,
                                "<span background='%s'>\xe2\x80\x83\xe2\x80\x83</span> <b>%s</b>  <tt><small>%s</small></tt>",
                                hex, escaped, hex);
        g_free (escaped);
        g_free (display);

        names = text_color_event_names (token - THEME_TOKEN_MIRC_0, &count);
        if (names)
        {
                int line_target;
                int line_len = 0;
                int i;

                display = g_strdup_printf (ngettext ("Used by %d text event",
                                                     "Used by %d text events", count), count);
                escaped = g_markup_escape_text (display, -1);
                g_string_append_printf (tip,
                                        "\n<small><span alpha='60%%'>%s</span></small>\n\n<small>",
                                        escaped);
                g_free (escaped);
                g_free (display);

                /* Explicit line breaks keep the tooltip geometry stable;
                 * heavily-used colors get wider lines so the full list
                 * stays on screen. */
                if (count <= 12)
                        line_target = 44;
                else if (count <= 40)
                        line_target = 60;
                else
                        line_target = 76;

                for (i = 0; i < count; i++)
                {
                        int name_len = (int) g_utf8_strlen (names[i], -1);

                        if (i)
                        {
                                if (line_len + name_len + 5 > line_target)
                                {
                                        g_string_append_c (tip, '\n');
                                        line_len = 0;
                                }
                                else
                                {
                                        g_string_append (tip, "  \xc2\xb7  ");
                                        line_len += 5;
                                }
                        }
                        escaped = g_markup_escape_text (names[i], -1);
                        g_string_append (tip, escaped);
                        line_len += name_len;
                        g_free (escaped);
                }
                g_string_append (tip, "</small>");
                g_strfreev (names);
        }
        else
        {
                escaped = g_markup_escape_text (_("Not used by any text events"), -1);
                g_string_append_printf (tip,
                                        "\n<small><span alpha='60%%'><i>%s</i></span></small>",
                                        escaped);
                g_free (escaped);
        }

        g_free (hex);
        return g_string_free (tip, FALSE);
}

/* Static markup tooltips only: building tooltip widgets from a
 * query-tooltip handler makes GtkTooltip flicker on some setups, so the
 * markup is set once and refreshed whenever the color changes. */
static void
theme_preferences_color_set_tooltip (GtkWidget *widget, ThemeSemanticToken token)
{
        char *markup = theme_preferences_color_tooltip_markup (token);

        if (!markup)
                return;

        gtk_widget_set_tooltip_markup (widget, markup);
        g_free (markup);
}

static gboolean theme_preferences_is_custom (ThemeSemanticToken token);

static void
theme_preferences_manager_row_apply (theme_color_manager_row *row, const GdkRGBA *rgba)
{
        char *hex;

        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (row->entry), GTK_ENTRY_ICON_SECONDARY, NULL);
        theme_preferences_color_button_apply (row->button, rgba);
        GdkRGBA black, white;
        gdk_rgba_parse (&black, "#000000"); gdk_rgba_parse (&white, "#ffffff");
        const GdkRGBA *ink = theme_preferences_contrast_ratio (&black, rgba) > theme_preferences_contrast_ratio (&white, rgba) ? &black : &white;
        gtkutil_apply_palette (row->preview, rgba, ink, NULL);
        hex = theme_preferences_format_hex (rgba);
        gtk_entry_set_text (GTK_ENTRY (row->entry), hex);
        g_object_set_data_full (G_OBJECT (row->entry), "applied-color", hex, g_free);
        theme_preferences_color_set_tooltip (row->row, row->token);
        if (row->status) gtk_label_set_text (GTK_LABEL (row->status), theme_preferences_is_custom (row->token) ? _("Custom") : _("Theme"));
        if (row->reset) gtk_widget_set_sensitive (row->reset, theme_preferences_is_custom (row->token));
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

        if (g_strcmp0 (text, g_object_get_data (G_OBJECT (row->entry), "applied-color")) == 0)
                return;

        if (!gdk_rgba_parse (&rgba, text))
        {
                gtk_entry_set_icon_from_icon_name (GTK_ENTRY (row->entry), GTK_ENTRY_ICON_SECONDARY, "dialog-warning-symbolic");
                gtk_entry_set_icon_tooltip_text (GTK_ENTRY (row->entry), GTK_ENTRY_ICON_SECONDARY, _("Enter a colour such as #336699, or choose one using the colour button."));
                return;
        }

        theme_preferences_manager_row_commit (row, &rgba);
}

gboolean
theme_preferences_color_editor_stage (GtkWidget *editor, GError **error)
{
        theme_color_manager_ui *ui = g_object_get_data (G_OBJECT (editor), "zoitechat-theme-color-manager");
        guint i;
        if (!ui)
        {
                GtkWidget *child = g_object_get_data (G_OBJECT (editor), "appearance-color-editor");
                if (child) return theme_preferences_color_editor_stage (child, error);
        }
        for (i = 0; ui && i < ui->rows->len; i++)
        {
                theme_color_manager_row *row = g_ptr_array_index (ui->rows, i);
                const char *text = gtk_entry_get_text (GTK_ENTRY (row->entry));
                GdkRGBA rgba;
                if (g_strcmp0 (text, g_object_get_data (G_OBJECT (row->entry), "applied-color")) == 0)
                        continue;
                if (!gdk_rgba_parse (&rgba, text))
                {
                        g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                _("Enter a valid color value before saving."));
                        theme_preferences_manager_focus_color (ui, row->token);
                        setup_reveal_widget (row->entry);
                        gtk_widget_grab_focus (row->entry);
                        return FALSE;
                }
                theme_preferences_manager_row_commit (row, &rgba);
        }
        return TRUE;
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

static void theme_preferences_use_theme_color (ThemeSemanticToken token);

static void
theme_preferences_manager_picker_response_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
        theme_manager_live_picker_data *data = user_data;

        if (data && data->row && data->has_original && response_id != GTK_RESPONSE_OK)
        {
                if (data->original_custom) theme_preferences_manager_row_commit (data->row, &data->original);
                else
                {
                        theme_preferences_use_theme_color (data->row->token);
                        if (data->row->color_change_flag) *data->row->color_change_flag = theme_preferences_stage.changed;
                        theme_manager_apply_and_dispatch (prefs.hex_gui_dark_mode, THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST, NULL);
                        theme_preferences_manager_refresh_rows (data->row->manager_ui);
                }
        }

        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
theme_preferences_manager_picker_owner_destroyed (GtkWidget *dialog)
{
        theme_manager_live_picker_data *data = g_object_get_data (G_OBJECT (dialog), "picker-data");
        data->row = NULL;
        gtk_widget_destroy (dialog);
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
        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

        data = g_new0 (theme_manager_live_picker_data, 1);
        data->row = row;
        data->original = rgba;
        data->has_original = TRUE;
        data->original_custom = theme_preferences_is_custom (row->token);
        g_object_set_data_full (G_OBJECT (dialog), "picker-data", data, g_free);
        g_signal_connect_object (row->row, "destroy",
                                  G_CALLBACK (theme_preferences_manager_picker_owner_destroyed),
                                  dialog, G_CONNECT_SWAPPED);

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
                gboolean matches = theme_preferences_manager_row_matches (row, needle_lower);
                gtk_widget_set_visible (row->row, matches);
                if (matches && needle_lower[0] && row->expander)
                        gtk_expander_set_expanded (GTK_EXPANDER (row->expander), TRUE);
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

                const char *applied = g_object_get_data (G_OBJECT (row->entry), "applied-color");
                if (applied && g_strcmp0 (applied, gtk_entry_get_text (GTK_ENTRY (row->entry)))) continue;
                if (theme_preferences_staged_get_color (row->token, &rgba))
                        theme_preferences_manager_row_apply (row, &rgba);
        }

        theme_preferences_manager_update_preview (ui);
}

static gboolean
theme_preferences_is_custom (ThemeSemanticToken token)
{
        GdkRGBA color;
        return theme_preferences_stage.active ? theme_preferences_stage.staged_custom[token] :
                theme_runtime_get_user_color (token, &color);
}

static void
theme_preferences_use_theme_color (ThemeSemanticToken token)
{
        if (theme_preferences_stage.active)
        {
                theme_preferences_stage.staged_custom[token] = FALSE;
                theme_preferences_stage.staged_dirty[token] = TRUE;
                theme_preferences_stage_recompute_changed ();
        }
        theme_runtime_reset_user_color (token);
}

static void
theme_preferences_manager_row_reset (GtkButton *button, gpointer data)
{
        theme_color_manager_row *row = data;
        GdkRGBA color;
        (void)button;
        theme_preferences_use_theme_color (row->token);
        theme_manager_apply_and_dispatch (prefs.hex_gui_dark_mode,
                THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST, NULL);
        if (row->color_change_flag) *row->color_change_flag = theme_preferences_stage.changed;
        theme_preferences_staged_get_color (row->token, &color);
        theme_preferences_manager_row_apply (row, &color);
        theme_preferences_manager_update_preview (row->manager_ui);
}

static void
theme_preferences_manager_focus_color (theme_color_manager_ui *ui, ThemeSemanticToken token)
{
        gtk_entry_set_text (GTK_ENTRY (ui->search_entry), "");
        for (guint i = 0; i < ui->rows->len; i++)
        {
                theme_color_manager_row *row = g_ptr_array_index (ui->rows, i);
                if (row->token != token) continue;
                if (row->expander) gtk_expander_set_expanded (GTK_EXPANDER (row->expander), TRUE);
                gtk_widget_grab_focus (row->button);
                return;
        }
}

/* Schemes are editable templates. Their identity is derived from the palette,
 * so existing colours.conf files require no new configuration format. */
static gboolean
theme_preferences_scheme_color (int scheme, ThemeSemanticToken token, GdkRGBA *color)
{
        static const char *soft[] = {
                "#e6edf3", "#17212b", "#ffffff", "#315c82", "#79c0ff",
                "#a5d6ff", "#ffa657", "#7ee787", "#a6adb4", "#ff7b72"
        };
        static const char *contrast[] = {
                "#ffffff", "#000000", "#000000", "#ffff00", "#ffffff",
                "#00ffff", "#ffff00", "#00ff00", "#c0c0c0", "#ff8080"
        };
        static const ThemeSemanticToken targets[] = {
                THEME_TOKEN_TEXT_FOREGROUND, THEME_TOKEN_TEXT_BACKGROUND,
                THEME_TOKEN_SELECTION_FOREGROUND, THEME_TOKEN_SELECTION_BACKGROUND,
                THEME_TOKEN_MARKER, THEME_TOKEN_TAB_NEW_DATA, THEME_TOKEN_TAB_HIGHLIGHT,
                THEME_TOKEN_TAB_NEW_MESSAGE, THEME_TOKEN_TAB_AWAY, THEME_TOKEN_SPELL
        };
        if (!scheme) return FALSE;
        theme_runtime_get_default_color (token, NULL, color);
        if (scheme == 1) return TRUE;
        for (guint i = 0; i < G_N_ELEMENTS (targets); i++)
                if (token == targets[i]) return gdk_rgba_parse (color, scheme == 2 ? soft[i] : contrast[i]);
        /* Keep the IRC colour identities while making their dark-theme
         * variants readable. Users can still edit every entry individually. */
        color->red = 0.58 + color->red * 0.42;
        color->green = 0.58 + color->green * 0.42;
        color->blue = 0.58 + color->blue * 0.42;
        return TRUE;
}

static void
theme_preferences_choice_selected (GtkWidget *button, gboolean selected)
{
        GtkWidget *title = g_object_get_data (G_OBJECT (button), "choice-title");
        const char *name = g_object_get_data (G_OBJECT (button), "choice-name");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), selected);
        if (title && name)
        {
                gtk_label_set_text (GTK_LABEL (title), name);
        }
}

static void
theme_preferences_manager_sync_schemes (theme_color_manager_ui *ui)
{
        if (!ui->scheme_buttons[0]) return;
        int matched = -1;
        gboolean custom = FALSE;
        for (int token = 0; token < THEME_TOKEN_COUNT; token++) custom |= theme_preferences_is_custom (token);
        if (!custom) matched = 0;
        for (int scheme = 1; matched < 0 && scheme < 4; scheme++)
        {
                gboolean same = TRUE;
                for (int token = 0; same && token < THEME_TOKEN_COUNT; token++)
                {
                        GdkRGBA actual, expected;
                        theme_preferences_staged_get_color (token, &actual);
                        theme_preferences_scheme_color (scheme, token, &expected);
                        same = fabs (actual.red - expected.red) < 0.005 && fabs (actual.green - expected.green) < 0.005 &&
                               fabs (actual.blue - expected.blue) < 0.005;
                }
                if (same) matched = scheme;
        }
        ui->syncing = TRUE;
        for (int i = 0; i < 4; i++) theme_preferences_choice_selected (ui->scheme_buttons[i], i == matched);
        ui->syncing = FALSE;
        gtk_label_set_text (GTK_LABEL (ui->scheme_status), matched < 0 ? _("Custom colours") :
                matched == 0 ? _("Follows application theme") : "");
}

static void
theme_preferences_manager_scheme_clicked (GtkButton *button, gpointer data)
{
        theme_color_manager_ui *ui = data;
        if (ui->syncing) return;
        int scheme = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "scheme"));
        for (int token = 0; token < THEME_TOKEN_COUNT; token++)
        {
                GdkRGBA color;
                if (!scheme) theme_preferences_use_theme_color (token);
                else
                {
                        theme_preferences_scheme_color (scheme, token, &color);
                        theme_preferences_staged_set_color (token, &color, ui->color_change_flag, FALSE);
                        theme_runtime_user_set_color (token, &color);
                }
        }
        theme_manager_apply_and_dispatch (prefs.hex_gui_dark_mode,
                THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST, NULL);
        if (ui->color_change_flag) *ui->color_change_flag = theme_preferences_stage.changed;
        for (guint i = 0; i < ui->rows->len; i++)
        {
                theme_color_manager_row *row = g_ptr_array_index (ui->rows, i);
                GdkRGBA color;
                theme_preferences_staged_get_color (row->token, &color);
                theme_preferences_manager_row_apply (row, &color);
        }
        theme_preferences_manager_update_preview (ui);
}

static void
theme_preferences_manager_reset (GtkWidget *button, gpointer data)
{
        /* Reset is the same inheritance operation as Match application. */
        theme_color_manager_ui *ui = data;
        (void)button;
        theme_preferences_manager_scheme_clicked (GTK_BUTTON (ui->scheme_buttons[0]), ui);
}

static void
theme_preferences_contrast_fix (GtkButton *button, gpointer data)
{
        theme_color_manager_ui *ui = data;
        const ThemeSemanticToken texts[] = { THEME_TOKEN_TEXT_FOREGROUND, THEME_TOKEN_SELECTION_FOREGROUND };
        const ThemeSemanticToken backgrounds[] = { THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_SELECTION_BACKGROUND };
        (void)button;
        for (guint n = 0; n < G_N_ELEMENTS (texts); n++)
        {
                GdkRGBA text, bg, black, white;
                theme_preferences_staged_get_color (texts[n], &text);
                theme_preferences_staged_get_color (backgrounds[n], &bg);
                if (theme_preferences_contrast_ratio (&text, &bg) >= 4.5) continue;
                gdk_rgba_parse (&black, "#000000"); gdk_rgba_parse (&white, "#ffffff");
                for (guint i = 0; i < ui->rows->len; i++)
                {
                        theme_color_manager_row *row = g_ptr_array_index (ui->rows, i);
                        if (row->token == texts[n])
                                theme_preferences_manager_row_commit (row, theme_preferences_contrast_ratio (&black, &bg) >
                                        theme_preferences_contrast_ratio (&white, &bg) ? &black : &white);
                }
        }
}

static gboolean
theme_preferences_manager_refresh_idle (gpointer data)
{
        theme_color_manager_ui *ui = data;
        ui->refresh_idle = 0;
        theme_preferences_manager_refresh_rows (ui);
        return G_SOURCE_REMOVE;
}

static void
theme_preferences_manager_theme_changed (const ThemeChangedEvent *event, gpointer data)
{
        theme_color_manager_ui *ui = data;
        (void)event;
        if (!ui->refresh_idle) ui->refresh_idle = g_idle_add (theme_preferences_manager_refresh_idle, ui);
}

static GtkWidget *
theme_preferences_note (const char *text)
{
        GtkWidget *label = gtk_label_new (text);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (label), 62);
        gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
        return label;
}

static void
theme_preferences_flow_activate (GtkFlowBox *box, GtkFlowBoxChild *child, gpointer data)
{
        GtkWidget *button = gtk_bin_get_child (GTK_BIN (child));
        (void)box; (void)data;
        if (GTK_IS_BUTTON (button)) gtk_button_clicked (GTK_BUTTON (button));
}

static GtkWidget *
theme_preferences_heading (const char *text)
{
        GtkWidget *heading = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget *label = gtk_label_new (NULL);
        GtkWidget *line = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        char *markup = g_markup_printf_escaped ("<b>%s</b>", text);
        gtk_label_set_markup (GTK_LABEL (label), markup);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        g_free (markup);
        gtk_box_pack_start (GTK_BOX (heading), label, FALSE, FALSE, 0);
        gtk_widget_set_valign (line, GTK_ALIGN_CENTER);
        gtk_box_pack_start (GTK_BOX (heading), line, TRUE, TRUE, 0);
        return heading;
}

GtkWidget *
theme_preferences_create_color_editor (GtkWindow *parent, gboolean *color_change_flag)
{
        theme_color_manager_ui *ui = g_new0 (theme_color_manager_ui, 1);
        GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        GtkWidget *schemes = gtk_flow_box_new ();
        GtkWidget *common = gtk_list_box_new (), *advanced = gtk_list_box_new ();
        const char *scheme_names[] = { N_("Match application"), N_("Classic IRC"), N_("Soft dark"), N_("High contrast") };
        const char *scheme_notes[] = { N_("Follow the window theme"), N_("Traditional IRC palette"), N_("Gentle colours on dark blue"), N_("Bright text on black") };
        ui->rows = g_ptr_array_new_with_free_func (theme_preferences_manager_row_free);
        ui->root = box; ui->color_change_flag = color_change_flag;
        ui->source = parent ? GTK_WIDGET (parent) : box;
        theme_preferences_stage.source = ui->source;
        g_object_set_data_full (G_OBJECT (box), "zoitechat-theme-color-manager", ui, theme_preferences_manager_ui_free);
        gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (schemes), GTK_SELECTION_NONE);
        gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (schemes), FALSE);
        g_signal_connect (schemes, "child-activated", G_CALLBACK (theme_preferences_flow_activate), NULL);
        gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (schemes), 2);
        gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (schemes), 4);
        gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (schemes), 8);
        gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (schemes), 8);
        for (int i = 0; i < 4; i++)
        {
                GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
                GtkWidget *button = gtk_toggle_button_new ();
                GtkWidget *title = gtk_label_new (_(scheme_names[i]));
                gtk_label_set_line_wrap (GTK_LABEL (title), TRUE);
                gtk_label_set_max_width_chars (GTK_LABEL (title), 14);
                GtkWidget *swatches = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_container_set_border_width (GTK_CONTAINER (content), 4);
                gtk_box_pack_start (GTK_BOX (content), title, FALSE, FALSE, 0);
                for (int swatch = 0; swatch < 4; swatch++)
                {
                        GdkRGBA color;
                        const ThemeSemanticToken tokens[] = { THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TEXT_FOREGROUND, THEME_TOKEN_TAB_HIGHLIGHT, THEME_TOKEN_TAB_NEW_MESSAGE };
                        if (i) theme_preferences_scheme_color (i, tokens[swatch], &color);
                        else theme_get_default_color_for_widget (GTK_WIDGET (parent), tokens[swatch], &color);
                        GtkWidget *sample = gtk_event_box_new ();
                        if (!i) ui->match_swatches[swatch] = sample;
                        gtk_widget_set_size_request (sample, 26, 12);
                        gtkutil_apply_palette (sample, &color, NULL, NULL);
                        gtk_box_pack_start (GTK_BOX (swatches), sample, TRUE, TRUE, 0);
                }
                gtk_box_pack_start (GTK_BOX (content), swatches, FALSE, FALSE, 2);
                gtk_container_add (GTK_CONTAINER (button), content);
                g_object_set_data (G_OBJECT (button), "choice-title", title);
                g_object_set_data (G_OBJECT (button), "choice-name", (gpointer)_(scheme_names[i]));
                gtk_widget_set_tooltip_text (button, _(scheme_notes[i]));
                atk_object_set_name (gtk_widget_get_accessible (button), _(scheme_names[i]));
                g_object_set_data (G_OBJECT (button), "scheme", GINT_TO_POINTER (i));
                g_signal_connect (button, "clicked", G_CALLBACK (theme_preferences_manager_scheme_clicked), ui);
                gtk_flow_box_insert (GTK_FLOW_BOX (schemes), button, -1);
                ui->scheme_buttons[i] = button;
        }
        gtk_box_pack_start (GTK_BOX (box), schemes, FALSE, FALSE, 0);
        ui->scheme_status = theme_preferences_note ("");
        gtk_box_pack_start (GTK_BOX (box), ui->scheme_status, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), theme_preferences_manager_create_preview (ui), FALSE, FALSE, 0);
        GtkWidget *feedback = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        ui->contrast = theme_preferences_note ("");
        gtk_widget_set_no_show_all (ui->contrast, TRUE);
        ui->contrast_fix = gtk_button_new_with_label (_("Improve text contrast"));
        gtk_widget_set_halign (ui->contrast_fix, GTK_ALIGN_START);
        gtk_widget_set_no_show_all (ui->contrast_fix, TRUE);
        gtk_box_pack_start (GTK_BOX (feedback), ui->contrast, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (feedback), ui->contrast_fix, FALSE, FALSE, 0);
        g_signal_connect (ui->contrast_fix, "clicked", G_CALLBACK (theme_preferences_contrast_fix), ui);
        gtk_box_pack_start (GTK_BOX (box), feedback, FALSE, FALSE, 0);
        ui->search_entry = gtk_search_entry_new ();
        gtk_entry_set_placeholder_text (GTK_ENTRY (ui->search_entry), _("Find a colour to adjust…"));
        atk_object_set_name (gtk_widget_get_accessible (ui->search_entry), _("Find a colour to adjust"));
        gtk_box_pack_start (GTK_BOX (box), ui->search_entry, FALSE, FALSE, 0);
        gtk_widget_set_tooltip_text (common, _("Theme uses the application appearance. Set a colour to override it."));
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (common), GTK_SELECTION_NONE);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (advanced), GTK_SELECTION_NONE);
        gtk_box_pack_start (GTK_BOX (box), common, FALSE, FALSE, 0);
        ui->advanced = gtk_expander_new (_("More colours"));
        gtk_container_add (GTK_CONTAINER (ui->advanced), advanced);
        gtk_box_pack_start (GTK_BOX (box), ui->advanced, FALSE, FALSE, 0);
        for (ThemeSemanticToken token = 0; token < THEME_TOKEN_COUNT; token++)
        {
                theme_color_manager_row *row = g_new0 (theme_color_manager_row, 1);
                GtkWidget *line = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
                GtkWidget *names = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
                char *display = theme_preferences_token_display_name (token);
                GtkWidget *name = gtk_label_new (display);
                gtk_label_set_xalign (GTK_LABEL (name), 0);
                gtk_label_set_line_wrap (GTK_LABEL (name), TRUE);
                gtk_label_set_max_width_chars (GTK_LABEL (name), 22);
                gtk_container_set_border_width (GTK_CONTAINER (line), 6);
                row->row = gtk_list_box_row_new ();
                gtk_container_add (GTK_CONTAINER (row->row), line);
                row->status = theme_preferences_note ("");
                gtk_box_pack_start (GTK_BOX (names), name, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (names), row->status, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (line), names, TRUE, TRUE, 0);
                row->button = gtk_button_new ();
                row->preview = gtk_event_box_new ();
                GtkWidget *swatch = gtk_label_new (" ");
                gtk_widget_set_size_request (swatch, 42, 20);
                gtk_container_add (GTK_CONTAINER (row->preview), swatch);
                gtk_container_add (GTK_CONTAINER (row->button), row->preview);
                g_object_set_data (G_OBJECT (row->button), "zoitechat-color-box", row->preview);
                char *choose_name = g_strdup_printf (_("Choose colour for %s"), display);
                atk_object_set_name (gtk_widget_get_accessible (row->button), choose_name);
                gtk_widget_set_tooltip_text (row->button, choose_name);
                g_free (choose_name);
                row->entry = gtk_entry_new ();
                gtk_entry_set_width_chars (GTK_ENTRY (row->entry), 9);
                gtk_entry_set_max_width_chars (GTK_ENTRY (row->entry), 9);
                gtk_entry_set_max_length (GTK_ENTRY (row->entry), 32);
                char *entry_name = g_strdup_printf (_("%s colour value"), display);
                atk_object_set_name (gtk_widget_get_accessible (row->entry), entry_name);
                g_free (entry_name);
                gtk_widget_set_tooltip_text (row->entry, _("Optional: enter a colour name or a value such as #336699."));
                row->reset = gtk_button_new_from_icon_name ("edit-undo-symbolic", GTK_ICON_SIZE_BUTTON);
                char *reset_name = g_strdup_printf (_("Use theme colour for %s"), display);
                gtk_widget_set_tooltip_text (row->reset, reset_name);
                atk_object_set_name (gtk_widget_get_accessible (row->reset), reset_name);
                g_free (reset_name);
                gtk_box_pack_start (GTK_BOX (line), row->button, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (line), row->entry, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (line), row->reset, FALSE, FALSE, 0);
                row->token = token; row->color_change_flag = color_change_flag;
                row->manager_ui = ui; row->parent = parent;
                char *search_text = g_strdup_printf ("%s token_%d %s", display, token,
                        token < 32 ? "mIRC IRC palette nickname" : token == THEME_TOKEN_TEXT_FOREGROUND ? "foreground text" :
                        token == THEME_TOKEN_SELECTION_FOREGROUND ? "foreground selection" : "background tab channel interface");
                row->search_text = g_utf8_strdown (search_text, -1);
                g_free (search_text);
                gboolean extra = token < THEME_TOKEN_SELECTION_FOREGROUND || token == THEME_TOKEN_MARKER || token == THEME_TOKEN_SPELL;
                row->expander = extra ? ui->advanced : NULL;
                gtk_container_add (GTK_CONTAINER (extra ? advanced : common), row->row);
                g_ptr_array_add (ui->rows, row);
                GdkRGBA color;
                theme_preferences_staged_get_color (token, &color);
                theme_preferences_manager_row_apply (row, &color);
                g_signal_connect (row->button, "clicked", G_CALLBACK (theme_preferences_manager_pick_cb), row);
                g_signal_connect (row->reset, "clicked", G_CALLBACK (theme_preferences_manager_row_reset), row);
                g_signal_connect (row->entry, "activate", G_CALLBACK (theme_preferences_manager_entry_activate_cb), row);
                g_signal_connect (row->entry, "focus-out-event", G_CALLBACK (theme_preferences_manager_entry_focus_out_cb), row);
                g_free (display);
        }
        const ThemeSemanticToken common_order[] = {
                THEME_TOKEN_TEXT_BACKGROUND, THEME_TOKEN_TEXT_FOREGROUND,
                THEME_TOKEN_SELECTION_BACKGROUND, THEME_TOKEN_SELECTION_FOREGROUND,
                THEME_TOKEN_TAB_HIGHLIGHT, THEME_TOKEN_TAB_NEW_MESSAGE, THEME_TOKEN_TAB_NEW_DATA, THEME_TOKEN_TAB_AWAY
        };
        for (guint i = 0; i < G_N_ELEMENTS (common_order); i++)
        {
                theme_color_manager_row *row = g_ptr_array_index (ui->rows, common_order[i]);
                g_object_ref (row->row);
                gtk_container_remove (GTK_CONTAINER (common), row->row);
                gtk_list_box_insert (GTK_LIST_BOX (common), row->row, -1);
                g_object_unref (row->row);
        }
        GtkWidget *reset = gtk_button_new_with_label (_("Reset chat colours"));
        gtk_widget_set_halign (reset, GTK_ALIGN_START);
        gtk_widget_set_tooltip_text (reset, _("Remove all custom chat colours and follow the application again. Cancel restores your previous choices."));
        g_signal_connect (reset, "clicked", G_CALLBACK (theme_preferences_manager_reset), ui);
        gtk_box_pack_start (GTK_BOX (box), reset, FALSE, FALSE, 0);
        g_object_set_data (G_OBJECT (box), "reset-button", reset);
        g_signal_connect (ui->search_entry, "changed", G_CALLBACK (theme_preferences_manager_search_changed_cb), ui);
        ui->listener = theme_listener_register ("preferences.appearance", theme_preferences_manager_theme_changed, ui);
        theme_preferences_manager_update_preview (ui);
        return box;
}

static void
theme_preferences_manage_colors_cb (GtkWidget *button, gpointer user_data)
{
        (void)button; (void)user_data;
        setup_open_page ("colors");
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

static void
theme_preferences_show_import_info (GtkWidget *button, const char *message)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_INFO,
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

static gboolean
theme_preferences_import_scheme_path (const char *path, gboolean include_formatting,
                                      gboolean *color_change_flag, gboolean *formatting_imported, GError **error)
{
        char *cfg = NULL, *pevents = NULL, *lower = g_ascii_strdown (path, -1);
        GdkRGBA colors[THEME_TOKEN_COUNT];
        gboolean present[THEME_TOKEN_COUNT] = { FALSE }, any = FALSE, success = FALSE;
        gboolean archive = g_str_has_suffix (lower, ".hct");
        if (formatting_imported) *formatting_imported = FALSE;
        if (archive)
        {
                if (!zoitechat_gtk3_theme_service_read_archive_text_file (path, "colors.conf", &cfg, error)) goto done;
        }
        else if (!g_file_get_contents (path, &cfg, NULL, error)) goto done;
        for (ThemeSemanticToken token = 0; token < THEME_TOKEN_COUNT; token++)
        {
                present[token] = theme_preferences_read_import_color (cfg, token, &colors[token]);
                any |= present[token];
        }
        if (!any)
        {
                g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("No recognised colours were found in this scheme."));
                goto done;
        }
        if (archive && include_formatting)
        {
                GError *optional_error = NULL;
                zoitechat_gtk3_theme_service_read_archive_text_file (path, "pevents.conf", &pevents, &optional_error);
                g_clear_error (&optional_error);
        }
        if (pevents)
        {
                /* Import into the same staged model as Message formatting.
                 * Invalid templates leave both models untouched. */
                setup_open_page ("events");
                gboolean imported = pevent_preferences_import_text (pevents, error);
                setup_open_page ("appearance");
                if (!imported) goto done;
                if (formatting_imported) *formatting_imported = TRUE;
        }
        for (ThemeSemanticToken token = 0; token < THEME_TOKEN_COUNT; token++)
                if (present[token])
                {
                        theme_preferences_staged_set_color (token, &colors[token], color_change_flag, FALSE);
                        theme_runtime_user_set_color (token, &colors[token]);
                }
        theme_manager_apply_and_dispatch (prefs.hex_gui_dark_mode,
                THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST, NULL);
        success = TRUE;
 done:
        g_free (pevents); g_free (cfg); g_free (lower);
        return success;
}

static void
theme_preferences_import_colors_conf_cb (GtkWidget *button, gpointer user_data)
{
        gboolean *color_change_flag = user_data;
        GtkFileChooserNative *dialog = gtk_file_chooser_native_new (_("Import colour scheme"),
                GTK_WINDOW (gtk_widget_get_toplevel (button)), GTK_FILE_CHOOSER_ACTION_OPEN, _("_Import"), _("_Cancel"));
        GtkFileFilter *filter = gtk_file_filter_new ();
        gtk_file_filter_set_name (filter, _("Colour schemes (*.conf, *.hct)"));
        gtk_file_filter_add_pattern (filter, "*.conf");
        gtk_file_filter_add_pattern (filter, "*.hct");
        gtk_file_filter_add_pattern (filter, "*.HCT");
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
        gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), FALSE);
        if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
        {
                g_object_unref (dialog);
                return;
        }
        char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        g_object_unref (dialog);
        if (!path) return;
        GtkWidget *include = g_object_get_data (G_OBJECT (button), "include-message-formatting");
        gboolean formatting = FALSE;
        GError *error = NULL;
        if (!theme_preferences_import_scheme_path (path, !include || gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (include)),
                                                   color_change_flag, &formatting, &error))
                theme_preferences_show_import_error (button, error ? error->message : _("Could not import this colour scheme."));
        else
                theme_preferences_show_import_info (button, formatting ?
                        _("Colours and message formatting are ready to preview. Save keeps them; Cancel discards them.") :
                        _("Colours are ready to preview. Save keeps them; Cancel discards them."));
        g_clear_error (&error);
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
        theme_preferences_color_set_tooltip (but, token);
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

static gboolean
theme_preferences_gtk3_apply_and_refresh (theme_preferences_ui *ui, GError **error)
{
	if (!theme_gtk3_apply_current (error))
		return FALSE;

	if (ui && ui->parent)
		theme_manager_apply_to_window (GTK_WIDGET (ui->parent));

	theme_manager_apply_mode (prefs.hex_gui_dark_mode, NULL);

	theme_manager_dispatch_changed (THEME_CHANGED_REASON_THEME_PACK |
	                                THEME_CHANGED_REASON_PALETTE |
	                                THEME_CHANGED_REASON_WIDGET_STYLE |
	                                THEME_CHANGED_REASON_USERLIST |
	                                THEME_CHANGED_REASON_MODE);
	return TRUE;
}

static void theme_preferences_sync_cards (theme_preferences_ui *ui);

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

        if (id[0] == '\0')
                variant = THEME_GTK3_VARIANT_FOLLOW_SYSTEM;
        else
                variant = theme_gtk3_variant_for_theme (id);
        char previous_id[sizeof prefs.hex_gui_gtk3_theme];
        g_strlcpy (previous_id, prefs.hex_gui_gtk3_theme, sizeof previous_id);
        int previous_variant = prefs.hex_gui_gtk3_variant;
        selection_changed = g_strcmp0 (prefs.hex_gui_gtk3_theme, id) != 0
                            || prefs.hex_gui_gtk3_variant != variant;
        g_strlcpy (prefs.hex_gui_gtk3_theme, id, sizeof (prefs.hex_gui_gtk3_theme));
        prefs.hex_gui_gtk3_variant = variant;

        if (ui->setup_prefs)
        {
                g_strlcpy (ui->setup_prefs->hex_gui_gtk3_theme, id, sizeof (ui->setup_prefs->hex_gui_gtk3_theme));
                ui->setup_prefs->hex_gui_gtk3_variant = prefs.hex_gui_gtk3_variant;
        }

        if (selection_changed && !theme_preferences_gtk3_apply_and_refresh (ui, &error))
        {
                g_strlcpy (prefs.hex_gui_gtk3_theme, previous_id, sizeof prefs.hex_gui_gtk3_theme);
                prefs.hex_gui_gtk3_variant = previous_variant;
                if (ui->setup_prefs)
                {
                        g_strlcpy (ui->setup_prefs->hex_gui_gtk3_theme, previous_id, sizeof ui->setup_prefs->hex_gui_gtk3_theme);
                        ui->setup_prefs->hex_gui_gtk3_variant = previous_variant;
                }
                GtkTreeModel *model = gtk_combo_box_get_model (combo);
                GtkTreeIter iter;
                gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
                ui->gtk3_populating = TRUE;
                while (valid)
                {
                        char *candidate;
                        gtk_tree_model_get (model, &iter, GTK3_THEME_COL_ID, &candidate, -1);
                        gboolean match = !g_strcmp0 (candidate, previous_id);
                        g_free (candidate);
                        if (match) { gtk_combo_box_set_active_iter (combo, &iter); break; }
                        valid = gtk_tree_model_iter_next (model, &iter);
                }
                ui->gtk3_populating = FALSE;
                theme_preferences_gtk3_sync_remove_state (ui);
                theme_preferences_show_message (ui, GTK_MESSAGE_ERROR,
                                                error ? error->message : _("Failed to apply GTK3 theme."));
                g_clear_error (&error);
        }

        theme_preferences_sync_cards (ui);
        g_free (id);
}

static void theme_preferences_rebuild_cards (theme_preferences_ui *ui);
static void theme_preferences_sync_cards (theme_preferences_ui *ui);

static void
theme_preferences_populate_gtk3 (theme_preferences_ui *ui)
{
        GPtrArray *themes;
        guint i;
        GtkTreeStore *store;
        GtkTreeIter iter;
        int active = -1;
        gboolean removed_selected_theme = FALSE;
        gboolean using_system_default = prefs.hex_gui_gtk3_theme[0] == '\0' ||
                !strcmp (prefs.hex_gui_gtk3_theme, THEME_GTK3_BUILTIN_LIGHT) || !strcmp (prefs.hex_gui_gtk3_theme, THEME_GTK3_BUILTIN_DARK);
        gboolean should_apply = FALSE;
        char *final_id;
        ThemeGtk3Variant final_variant = THEME_GTK3_VARIANT_PREFER_LIGHT;
        GError *error = NULL;

        store = GTK_TREE_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (ui->gtk3_combo)));
        ui->gtk3_populating = TRUE;
        gtk_tree_store_clear (store);
        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter,
                            GTK3_THEME_COL_ID, "",
                            GTK3_THEME_COL_LABEL, _("Follow system"),
                            GTK3_THEME_COL_SOURCE, ZOITECHAT_GTK3_THEME_SOURCE_SYSTEM,
                            -1);
        themes = zoitechat_gtk3_theme_service_discover ();
        for (i = 0; i < themes->len; i++)
        {
                ZoitechatGtk3Theme *theme = g_ptr_array_index (themes, i);
                char *label = g_strdup_printf ("%s (%s)", theme->display_name,
                                               theme->source == ZOITECHAT_GTK3_THEME_SOURCE_USER ? _("user") : _("system"));

                gtk_tree_store_append (store, &iter, NULL);
                gtk_tree_store_set (store, &iter,
                                    GTK3_THEME_COL_ID, theme->id,
                                    GTK3_THEME_COL_LABEL, label,
                                    GTK3_THEME_COL_SOURCE, theme->source,
                                    -1);
                if (g_strcmp0 (prefs.hex_gui_gtk3_theme, theme->id) == 0)
                        active = (int)i + 1;
                g_free (label);
        }
        if (active < 0 && using_system_default)
                active = 0;
        if (active >= 0)
                gtk_combo_box_set_active (GTK_COMBO_BOX (ui->gtk3_combo), active);
        else
        {
                gtk_combo_box_set_active (GTK_COMBO_BOX (ui->gtk3_combo), 0);
                if (!using_system_default)
                        removed_selected_theme = TRUE;
        }
        gtk_widget_set_sensitive (ui->gtk3_combo, TRUE);
        theme_preferences_gtk3_sync_remove_state (ui);
        ui->gtk3_populating = FALSE;

        final_id = theme_preferences_gtk3_active_id (ui);
        if (final_id)
        {
                if (final_id[0] == '\0')
                        final_variant = THEME_GTK3_VARIANT_FOLLOW_SYSTEM;
                else
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

        if (should_apply && !theme_preferences_gtk3_apply_and_refresh (ui, &error))
        {
                theme_preferences_show_message (ui, GTK_MESSAGE_ERROR,
                                                error ? error->message : _("Failed to apply GTK3 theme."));
                g_clear_error (&error);
        }

        g_ptr_array_unref (themes);
        theme_preferences_rebuild_cards (ui);
}

static void
theme_preferences_gtk3_import_path (theme_preferences_ui *ui, char *path)
{
        char *id = NULL;
        GError *error = NULL;
        gboolean imported = zoitechat_gtk3_theme_service_import (path, &id, &error);
        if (!imported)
                theme_preferences_show_message (ui, GTK_MESSAGE_ERROR,
                                                error ? error->message : _("Failed to install theme."));
        g_clear_error (&error);
        g_free (path);
        theme_preferences_populate_gtk3 (ui);
        if (imported && id)
        {
                GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (ui->gtk3_combo));
                GtkTreeIter iter;
                gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
                while (valid)
                {
                        char *candidate;
                        gtk_tree_model_get (model, &iter, GTK3_THEME_COL_ID, &candidate, -1);
                        gboolean match = !g_strcmp0 (candidate, id);
                        g_free (candidate);
                        if (match) { gtk_combo_box_set_active_iter (GTK_COMBO_BOX (ui->gtk3_combo), &iter); break; }
                        valid = gtk_tree_model_iter_next (model, &iter);
                }
                gtk_expander_set_expanded (GTK_EXPANDER (ui->theme_expander), TRUE);
        }
        g_free (id);
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

static void
theme_preferences_sync_cards (theme_preferences_ui *ui)
{
        if (!ui->theme_cards) return;
        gboolean populating = ui->gtk3_populating;
        ui->gtk3_populating = TRUE;
        for (int i = 0; i < 3; i++)
        {
                int mode = i == 0 ? ZOITECHAT_DARK_MODE_AUTO : i == 1 ? ZOITECHAT_DARK_MODE_LIGHT : ZOITECHAT_DARK_MODE_DARK;
                theme_preferences_choice_selected (ui->style_buttons[i],
                        (!prefs.hex_gui_gtk3_theme[0] || g_str_has_prefix (prefs.hex_gui_gtk3_theme, "builtin:")) && prefs.hex_gui_dark_mode == mode);
        }
        GList *children = gtk_container_get_children (GTK_CONTAINER (ui->theme_cards));
        for (GList *i = children; i; i = i->next)
        {
                GtkWidget *button = gtk_bin_get_child (GTK_BIN (i->data));
                const char *id = g_object_get_data (G_OBJECT (button), "theme-id");
                theme_preferences_choice_selected (button, !g_strcmp0 (id, prefs.hex_gui_gtk3_theme));
        }
        g_list_free (children);
        ui->gtk3_populating = populating;
}

static void
theme_preferences_style_clicked (GtkButton *button, gpointer data)
{
        theme_preferences_ui *ui = data;
        if (ui->gtk3_populating) return;
        int mode = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "appearance-mode"));
        prefs.hex_gui_dark_mode = mode;
        if (ui->setup_prefs) ui->setup_prefs->hex_gui_dark_mode = mode;
        ui->gtk3_populating = TRUE;
        gtk_combo_box_set_active (GTK_COMBO_BOX (ui->gtk3_combo), 0);
        ui->gtk3_populating = FALSE;
        const char *id = mode == ZOITECHAT_DARK_MODE_AUTO ? "" : mode == ZOITECHAT_DARK_MODE_DARK ? THEME_GTK3_BUILTIN_DARK : THEME_GTK3_BUILTIN_LIGHT;
        ThemeGtk3Variant variant = mode == ZOITECHAT_DARK_MODE_AUTO ? THEME_GTK3_VARIANT_FOLLOW_SYSTEM :
                mode == ZOITECHAT_DARK_MODE_DARK ? THEME_GTK3_VARIANT_PREFER_DARK : THEME_GTK3_VARIANT_PREFER_LIGHT;
        g_strlcpy (prefs.hex_gui_gtk3_theme, id, sizeof prefs.hex_gui_gtk3_theme);
        prefs.hex_gui_gtk3_variant = variant;
        if (ui->setup_prefs)
        {
                g_strlcpy (ui->setup_prefs->hex_gui_gtk3_theme, id, sizeof ui->setup_prefs->hex_gui_gtk3_theme);
                ui->setup_prefs->hex_gui_gtk3_variant = variant;
        }
        theme_preferences_gtk3_apply_and_refresh (ui, NULL);
        theme_preferences_sync_cards (ui);
}

static void
theme_preferences_theme_card_clicked (GtkButton *button, gpointer data)
{
        theme_preferences_ui *ui = data;
        if (ui->gtk3_populating) return;
        const char *id = g_object_get_data (G_OBJECT (button), "theme-id");
        GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (ui->gtk3_combo));
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
        while (valid)
        {
                char *candidate;
                gtk_tree_model_get (model, &iter, GTK3_THEME_COL_ID, &candidate, -1);
                gboolean match = !g_strcmp0 (candidate, id);
                g_free (candidate);
                if (match) { gtk_combo_box_set_active_iter (GTK_COMBO_BOX (ui->gtk3_combo), &iter); break; }
                valid = gtk_tree_model_iter_next (model, &iter);
        }
        theme_preferences_sync_cards (ui);
}

static gboolean
theme_preferences_theme_card_filter (GtkFlowBoxChild *child, gpointer data)
{
        theme_preferences_ui *ui = data;
        GtkWidget *button = gtk_bin_get_child (GTK_BIN (child));
        char *needle = g_utf8_casefold (gtk_entry_get_text (GTK_ENTRY (ui->theme_search)), -1);
        const char *name = g_object_get_data (G_OBJECT (button), "theme-search");
        gboolean visible = !needle[0] || (name && strstr (name, needle));
        g_free (needle);
        return visible;
}

static void
theme_preferences_theme_search_changed (GtkEditable *entry, gpointer data)
{
        theme_preferences_ui *ui = data;
        (void)entry;
        gtk_flow_box_invalidate_filter (GTK_FLOW_BOX (ui->theme_cards));
}

static void
theme_preferences_rebuild_cards (theme_preferences_ui *ui)
{
        if (!ui->theme_cards) return;
        GList *children = gtk_container_get_children (GTK_CONTAINER (ui->theme_cards));
        for (GList *i = children; i; i = i->next) gtk_widget_destroy (i->data);
        g_list_free (children);
        GPtrArray *themes = zoitechat_gtk3_theme_service_discover ();
        for (guint i = 0; i < themes->len; i++)
        {
                ZoitechatGtk3Theme *theme = g_ptr_array_index (themes, i);
                GtkWidget *button = gtk_toggle_button_new ();
                GtkWidget *content = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
                GtkWidget *picture = NULL;
                if (theme->thumbnail_path)
                {
                        GdkPixbuf *image = gdk_pixbuf_new_from_file_at_scale (theme->thumbnail_path, 48, 28, TRUE, NULL);
                        if (image) { picture = gtk_image_new_from_pixbuf (image); g_object_unref (image); }
                }
                if (!picture) picture = gtk_image_new_from_icon_name ("preferences-desktop-theme", GTK_ICON_SIZE_MENU);
                gtk_container_set_border_width (GTK_CONTAINER (content), 4);
                gtk_box_pack_start (GTK_BOX (content), picture, FALSE, FALSE, 0);
                GtkWidget *label = gtk_label_new (theme->display_name);
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_label_set_max_width_chars (GTK_LABEL (label), 24);
                gtk_box_pack_start (GTK_BOX (content), label, FALSE, FALSE, 0);
                g_object_set_data (G_OBJECT (button), "choice-title", label);
                g_object_set_data_full (G_OBJECT (button), "choice-name", g_strdup (theme->display_name), g_free);
                gtk_widget_set_tooltip_text (button, theme->source == ZOITECHAT_GTK3_THEME_SOURCE_USER ? _("Installed by you") : _("Available on this computer"));
                gtk_container_add (GTK_CONTAINER (button), content);
                atk_object_set_name (gtk_widget_get_accessible (button), theme->display_name);
                g_object_set_data_full (G_OBJECT (button), "theme-id", g_strdup (theme->id), g_free);
                g_object_set_data_full (G_OBJECT (button), "theme-search", g_utf8_casefold (theme->display_name, -1), g_free);
                g_signal_connect (button, "clicked", G_CALLBACK (theme_preferences_theme_card_clicked), ui);
                gtk_flow_box_insert (GTK_FLOW_BOX (ui->theme_cards), button, -1);
        }
        g_ptr_array_unref (themes);
        gtk_widget_show_all (ui->theme_cards);
        theme_preferences_sync_cards (ui);
}

GtkWidget *
theme_preferences_create_page (GtkWindow *parent, struct zoitechatprefs *setup_prefs, gboolean *color_change_flag)
{
        theme_preferences_ui *ui = g_new0 (theme_preferences_ui, 1);
        GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        ui->parent = parent; ui->setup_prefs = setup_prefs;
        g_object_set_data_full (G_OBJECT (box), "theme-preferences-ui", ui, g_free);
        GtkWidget *modes = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        const char *names[] = { N_("Follow system"), N_("Light"), N_("Dark") };
        gtk_box_pack_start (GTK_BOX (box), theme_preferences_heading (_("Application theme")), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), modes, FALSE, FALSE, 0);
        gtk_widget_set_tooltip_text (modes, _("Follow system uses your desktop theme. Light and Dark use the built-in appearance."));
        gtk_style_context_add_class (gtk_widget_get_style_context (modes), "linked");
        for (int i = 0; i < 3; i++)
        {
                GtkWidget *button = gtk_toggle_button_new_with_label (_(names[i]));
                atk_object_set_name (gtk_widget_get_accessible (button), _(names[i]));
                int mode = i == 0 ? ZOITECHAT_DARK_MODE_AUTO : i == 1 ? ZOITECHAT_DARK_MODE_LIGHT : ZOITECHAT_DARK_MODE_DARK;
                g_object_set_data (G_OBJECT (button), "appearance-mode", GINT_TO_POINTER (mode));
                g_signal_connect (button, "clicked", G_CALLBACK (theme_preferences_style_clicked), ui);
                gtk_box_pack_start (GTK_BOX (modes), button, TRUE, TRUE, 0);
                ui->style_buttons[i] = button;
        }
        ui->theme_expander = gtk_expander_new (_("Installed themes"));
        GtkWidget *installed = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_add (GTK_CONTAINER (ui->theme_expander), installed);
        gtk_expander_set_expanded (GTK_EXPANDER (ui->theme_expander), prefs.hex_gui_gtk3_theme[0] != '\0');
        ui->theme_search = gtk_search_entry_new ();
        gtk_entry_set_placeholder_text (GTK_ENTRY (ui->theme_search), _("Find an installed theme…"));
        gtk_box_pack_start (GTK_BOX (installed), ui->theme_search, FALSE, FALSE, 0);
        GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroller), 96);
        gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroller), 180);
        gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroller), TRUE);
        ui->theme_cards = gtk_flow_box_new ();
        gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (ui->theme_cards), GTK_SELECTION_NONE);
        gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (ui->theme_cards), FALSE);
        g_signal_connect (ui->theme_cards, "child-activated", G_CALLBACK (theme_preferences_flow_activate), NULL);
        gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (ui->theme_cards), 1);
        gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (ui->theme_cards), 1);
        gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (ui->theme_cards), 8);
        gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (ui->theme_cards), 8);
        gtk_flow_box_set_filter_func (GTK_FLOW_BOX (ui->theme_cards), theme_preferences_theme_card_filter, ui, NULL);
        gtk_container_add (GTK_CONTAINER (scroller), ui->theme_cards);
        gtk_box_pack_start (GTK_BOX (installed), scroller, FALSE, FALSE, 0);
        g_signal_connect (ui->theme_search, "changed", G_CALLBACK (theme_preferences_theme_search_changed), ui);
        GtkWidget *tools = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *import = gtk_button_new_with_label (_("Install theme…"));
        ui->gtk3_remove = gtk_button_new_with_label (_("Remove selected theme"));
        GtkWidget *folder = gtk_button_new_with_label (_("Open theme folder"));
        gtk_box_pack_start (GTK_BOX (tools), import, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (tools), ui->gtk3_remove, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (tools), folder, FALSE, FALSE, 0);
        g_signal_connect (import, "clicked", G_CALLBACK (theme_preferences_gtk3_import_cb), ui);
        g_signal_connect (ui->gtk3_remove, "clicked", G_CALLBACK (theme_preferences_gtk3_remove_cb), ui);
        g_signal_connect (folder, "clicked", G_CALLBACK (theme_preferences_open_gtk3_folder_cb), ui);
        gtk_box_pack_start (GTK_BOX (installed), tools, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (installed), theme_preferences_note (_("Installing and removing theme files takes effect immediately.")), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), ui->theme_expander, FALSE, FALSE, 0);
        GtkTreeStore *store = gtk_tree_store_new (GTK3_THEME_COL_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
        /* Keep one selection model shared by import/remove and the visual cards. */
        ui->gtk3_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
        g_object_unref (store);
        g_object_ref_sink (ui->gtk3_combo);
        g_object_set_data_full (G_OBJECT (box), "theme-selection-model", ui->gtk3_combo, g_object_unref);
        g_signal_connect (ui->gtk3_combo, "changed", G_CALLBACK (theme_preferences_gtk3_changed_cb), ui);
        theme_preferences_populate_gtk3 (ui);
        theme_preferences_rebuild_cards (ui);
        gtk_box_pack_start (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), theme_preferences_heading (_("Chat colours")), FALSE, FALSE, 0);
        ui->color_editor = theme_preferences_create_color_editor (parent, color_change_flag);
        gtk_box_pack_start (GTK_BOX (box), ui->color_editor, FALSE, FALSE, 0);
        g_object_set_data (G_OBJECT (box), "appearance-color-editor", ui->color_editor);
        GtkWidget *files = gtk_expander_new (_("Import colours and message formatting"));
        GtkWidget *file_tools = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_add (GTK_CONTAINER (files), file_tools);
        import = gtk_button_new_with_label (_("Import colour scheme…"));
        gtk_widget_set_halign (import, GTK_ALIGN_START);
        GtkWidget *formatting = gtk_check_button_new_with_label (_("Include message formatting when the theme file provides it"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (formatting), TRUE);
        g_object_set_data (G_OBJECT (import), "include-message-formatting", formatting);
        g_signal_connect (import, "clicked", G_CALLBACK (theme_preferences_import_colors_conf_cb), color_change_flag);
        gtk_box_pack_start (GTK_BOX (file_tools), import, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (file_tools), formatting, FALSE, FALSE, 0);
        gtk_widget_set_tooltip_text (import, _("Import colours.conf / colors.conf or HexChat themes (.hct). Changes apply when you save Preferences."));
        gtk_box_pack_start (GTK_BOX (box), files, FALSE, FALSE, 0);
        GtkWidget *stripping = gtk_expander_new (_("Ignore colours sent by other people"));
        GtkWidget *strip_grid = gtk_grid_new ();
        gtk_grid_set_row_spacing (GTK_GRID (strip_grid), 6);
        theme_preferences_create_strip_toggle (strip_grid, 0, _("Messages"), &setup_prefs->hex_text_stripcolor_msg);
        theme_preferences_create_strip_toggle (strip_grid, 1, _("Scrollback"), &setup_prefs->hex_text_stripcolor_replay);
        theme_preferences_create_strip_toggle (strip_grid, 2, _("Topic"), &setup_prefs->hex_text_stripcolor_topic);
        gtk_container_add (GTK_CONTAINER (stripping), strip_grid);
        gtk_box_pack_start (GTK_BOX (box), stripping, FALSE, FALSE, 0);
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
