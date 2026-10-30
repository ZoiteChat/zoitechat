/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fe-gtk.h"

#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/outbound.h"
#include "../common/fe.h"
#include "../common/text.h"
#include "gtkutil.h"
#include "xtext.h"
#include "maingui.h"
#include "theme/theme-access.h"
#include "theme/theme-manager.h"
#include "textgui.h"
#include "setup.h"
#include "preferences-persistence.h"

#define ICON_TEXTEVENT_SAVE_AS "document-save-as"
#define ICON_TEXTEVENT_OPEN "document-open"
#define ICON_TEXTEVENT_OK "dialog-ok"

extern struct text_event te[];
extern char *pntevts_text[];
extern char *pntevts[];

static GtkWidget *pevent_dialog = NULL, *pevent_dialog_twid,
	*pevent_dialog_list, *pevent_dialog_hlist;

static char *pevent_staged[NUM_XP];
static char *pevent_original[NUM_XP];
static gboolean pevent_pending;

#define PEVENT_THEME_LISTENER_ID_KEY "textgui.theme-listener-id"

enum
{
	EVENT_COLUMN,
	TEXT_COLUMN,
	ROW_COLUMN,
	N_COLUMNS
};


/* this is only used in xtext.c for indented timestamping */
int
xtext_get_stamp_str (time_t tim, char **ret)
{
	return get_stamp_str (prefs.hex_stamp_text_format, tim, ret);
}

static void
PrintTextLine (xtext_buffer *xtbuf, unsigned char *text, int len, int indent, time_t timet,
					const reply_item *message)
{
	unsigned char *tab, *new_text;
	int leftlen;

	if (len == 0)
		len = 1;

	if (!indent)
	{
		if (prefs.hex_stamp_text)
		{
			int stamp_size;
			char *stamp;

			if (timet == 0)
				timet = time (0);

			stamp_size = get_stamp_str (prefs.hex_stamp_text_format, timet, &stamp);
			new_text = g_malloc (len + stamp_size + 1);
			memcpy (new_text, stamp, stamp_size);
			g_free (stamp);
			memcpy (new_text + stamp_size, text, len);
			gtk_xtext_append_wrap_with_message (xtbuf, new_text, len + stamp_size,
													  timet, stamp_size, message);
			g_free (new_text);
		} else
			gtk_xtext_append_wrap_with_message (xtbuf, text, len, timet, 0, message);
		return;
	}

	tab = strchr (text, '\t');
	if (tab && tab < (text + len))
	{
		leftlen = tab - text;
		gtk_xtext_append_indent_with_message (xtbuf, text, leftlen,
													 tab + 1, len - (leftlen + 1), timet, message);
	} else
		gtk_xtext_append_indent_with_message (xtbuf, 0, 0, text, len, timet, message);
}

void
PrintTextRaw (void *xtbuf, unsigned char *text, int indent, time_t stamp)
{
	PrintTextRawWithMessage (xtbuf, text, indent, stamp, NULL);
}

void
PrintTextRawWithMessage (void *xtbuf, unsigned char *text, int indent, time_t stamp,
								 const reply_item *message)
{
	char *last_text = text;
	int len = 0;
	int beep_done = FALSE;

	/* split the text into separate lines */
	while (1)
	{
		switch (*text)
		{
		case 0:
			PrintTextLine (xtbuf, last_text, len, indent, stamp, message);
			return;
		case '\n':
			PrintTextLine (xtbuf, last_text, len, indent, stamp, message);
			text++;
			if (*text == 0)
				return;
			last_text = text;
			len = 0;
			break;
		case ATTR_BEEP:
			*text = ' ';
			if (!beep_done) /* beeps may be slow, so only do 1 per line */
			{
				beep_done = TRUE;
				if (!prefs.hex_input_filter_beep)
					gdk_display_beep (gdk_display_get_default ());
			}
		default:
			text++;
			len++;
		}
	}
}

static void
pevent_preferences_destroy (GtkWidget *widget, gpointer data)
{
	int i;
	(void)widget; (void)data;
	pevent_dialog = pevent_dialog_list = pevent_dialog_hlist = pevent_dialog_twid = NULL;
	for (i = 0; i < NUM_XP; i++)
	{
		g_clear_pointer (&pevent_staged[i], g_free);
		g_clear_pointer (&pevent_original[i], g_free);
	}
	pevent_pending = FALSE;
}

static void
pevent_dialog_theme_apply (GtkWidget *window)
{
	GtkWidget *xtext;
	XTextColor xtext_palette[XTEXT_COLS];

	if (!window)
		return;

	xtext = g_object_get_data (G_OBJECT (window), "xtext");
	if (!xtext)
		return;

	theme_get_xtext_colors_for_widget (xtext, xtext_palette, XTEXT_COLS);
	gtk_xtext_set_palette (GTK_XTEXT (xtext), xtext_palette);
}

static void
pevent_dialog_theme_changed (const ThemeChangedEvent *event, gpointer userdata)
{
	GtkWidget *window = userdata;

	if (!theme_changed_event_has_reason (event, THEME_CHANGED_REASON_PALETTE) &&
	    !theme_changed_event_has_reason (event, THEME_CHANGED_REASON_THEME_PACK) &&
	    !theme_changed_event_has_reason (event, THEME_CHANGED_REASON_MODE))
		return;

	pevent_dialog_theme_apply (window);
}

static void
pevent_dialog_theme_destroy_cb (GtkWidget *widget, gpointer userdata)
{
	guint listener_id;

	(void) userdata;

	listener_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), PEVENT_THEME_LISTENER_ID_KEY));
	if (listener_id)
	{
		theme_listener_unregister (listener_id);
		g_object_set_data (G_OBJECT (widget), PEVENT_THEME_LISTENER_ID_KEY, NULL);
	}
}

static void
pevent_edited (GtkCellRendererText *render, gchar *pathstr, gchar *new_text, gpointer data)
{
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (pevent_dialog_list));
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkXText *xtext = GTK_XTEXT (pevent_dialog_twid);
	int len, m;
	const char *text;
	char *out;
	int sig;

	path = gtk_tree_path_new_from_string (pathstr);
	if (!gtk_tree_model_get_iter (model, &iter, path))
	{
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_path_free (path);
	gtk_tree_model_get (model, &iter, ROW_COLUMN, &sig, -1);

	text = new_text;
	len = strlen (new_text);

	if (pevt_build_string (text, &out, &m) != 0)
	{
		fe_message (_("There was an error parsing the string"), FE_MSG_ERROR);
		return;
	}
	if (m > (te[sig].num_args & 0x7f))
	{
		g_free (out);
		out = g_strdup_printf (
			_("This signal is only passed %d args, $%d is invalid"),
			te[sig].num_args & 0x7f, m);
		fe_message (out, FE_MSG_WARN);
		g_free (out);
		return;
	}

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, TEXT_COLUMN, new_text, -1);

	g_free (pevent_staged[sig]);
	pevent_staged[sig] = g_strdup (text);
	g_free (out);

	out = g_malloc (len + 2);
	memcpy (out, text, len + 1);
	out[len] = '\n';
	out[len + 1] = 0;
	check_special_chars (out, TRUE);

	PrintTextRaw (xtext->buffer, out, 0, 0);
	g_free (out);

	/* Scroll to bottom */
	gtk_adjustment_set_value (xtext->adj, gtk_adjustment_get_upper (xtext->adj));

	/* The Preferences Save action commits this staged template. */
}

static void
pevent_dialog_hfill (GtkWidget *list, int e)
{
	int i = 0;
	char *text;
	GtkTreeIter iter;
	GtkListStore *store;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (list)));
	gtk_list_store_clear (store);

	while (i < (te[e].num_args & 0x7f))
	{
		text = _(te[e].help[i]);
		i++;
		if (text[0] == '\001')
			text++;
		gtk_list_store_insert_with_values (store, &iter, -1,
													  0, i,
													  1, text, -1);
	}
}

static void
pevent_selection_changed (GtkTreeSelection *sel, gpointer userdata)
{
	GtkTreeIter iter;
	int sig;

	if (!gtkutil_treeview_get_selected (GTK_TREE_VIEW (pevent_dialog_list),
										&iter, ROW_COLUMN, &sig, -1))
	{
		gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (pevent_dialog_hlist))));
		return;
	}

	pevent_dialog_hfill (pevent_dialog_hlist, sig);
}

static void
pevent_dialog_fill (GtkWidget *list)
{
	int i;
	GtkListStore *store;
	GtkTreeIter iter;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (list)));
	gtk_list_store_clear (store);

	i = NUM_XP;
	do
	{
		i--;
		gtk_list_store_insert_with_values (store, &iter, 0,
													  EVENT_COLUMN, te[i].name,
													  TEXT_COLUMN, pevent_staged[i],
													  ROW_COLUMN, i, -1);
	}
	while (i != 0);
}

static char *
pevent_preferences_serialize (void)
{
	GString *text = g_string_new ("");
	int i;
	for (i = 0; i < NUM_XP; i++)
		g_string_append_printf (text, "event_name=%s\nevent_text=%s\n\n", te[i].name, pevent_staged[i]);
	return g_string_free (text, FALSE);
}

static gboolean
pevent_preferences_validate (const char *text, int event, GError **error)
{
	char *compiled = NULL;
	int maximum;
	if (strpbrk (text, "\r\n") || pevt_build_string (text, &compiled, &maximum) != 0)
	{
		g_free (compiled);
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid message format for %s."), te[event].name);
		return FALSE;
	}
	g_free (compiled);
	if (maximum > (te[event].num_args & 0x7f))
	{
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("%s only supports %d arguments."), te[event].name, te[event].num_args & 0x7f);
		return FALSE;
	}
	return TRUE;
}

gboolean
pevent_preferences_import_text (const char *contents, GError **error)
{
	char **lines;
	char *values[NUM_XP] = {0};
	int i, event = -1, matched = 0;
	gboolean success = FALSE;
	if (!pevent_dialog_list)
	{
		g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Open message formatting before importing templates."));
		return FALSE;
	}
	lines = g_strsplit (contents, "\n", -1);
	for (i = 0; lines[i]; i++)
	{
		char *line = lines[i];
		gsize length = strlen (line);
		int n;
		if (length && line[length - 1] == '\r') line[length - 1] = 0;
		if (g_str_has_prefix (line, "event_name="))
		{
			event = -1;
			for (n = 0; n < NUM_XP; n++)
				if (!strcmp (te[n].name, line + 11)) { event = n; break; }
		}
		else if (event >= 0 && g_str_has_prefix (line, "event_text="))
		{
			if (!pevent_preferences_validate (line + 11, event, error)) goto done;
			g_free (values[event]);
			values[event] = g_strdup (line + 11);
			matched++;
		}
	}
	if (!matched)
	{
		g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("No recognized text events were found in this file."));
		goto done;
	}
	for (i = 0; i < NUM_XP; i++)
                if (values[i])
                {
                        g_free (pevent_staged[i]);
                        pevent_staged[i] = values[i];
                        values[i] = NULL;
                }
	pevent_dialog_fill (pevent_dialog_list);
	success = TRUE;
 done:
	for (i = 0; i < NUM_XP; i++) g_free (values[i]);
	g_strfreev (lines);
	return success;
}

static gboolean
pevent_preferences_import (const char *file, GError **error)
{
	char *contents;
	if (!g_file_get_contents (file, &contents, NULL, error)) return FALSE;
	gboolean result = pevent_preferences_import_text (contents, error);
	g_free (contents);
	return result;
}

typedef struct
{
	GWeakRef page;
	gboolean exporting;
} PeEventFileRequest;

static void
pevent_file_request_free (gpointer data)
{
	PeEventFileRequest *request = data;
	g_weak_ref_clear (&request->page);
	g_free (request);
}

static void
pevent_file_response (GtkNativeDialog *dialog, gint response, PeEventFileRequest *request)
{
	GtkWidget *page = g_weak_ref_get (&request->page);
	if (page && page == pevent_dialog && response == GTK_RESPONSE_ACCEPT)
	{
		char *file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		GError *error = NULL;
		if (file)
		{
			if (request->exporting)
			{
				char *contents = pevent_preferences_serialize ();
				g_file_set_contents (file, contents, -1, &error);
				g_free (contents);
			}
			else pevent_preferences_import (file, &error);
			if (error) { fe_message (error->message, FE_MSG_ERROR); g_clear_error (&error); }
			g_free (file);
		}
	}
	if (page) g_object_unref (page);
	g_object_unref (dialog);
}

static void
pevent_file_open (GtkWidget *button, gpointer exporting)
{
	GtkFileChooserNative *dialog;
	PeEventFileRequest *request = g_new0 (PeEventFileRequest, 1);
	(void)button;
	request->exporting = GPOINTER_TO_INT (exporting);
	g_weak_ref_init (&request->page, pevent_dialog);
	dialog = gtk_file_chooser_native_new (_("Text events file"),
		GTK_WINDOW (gtk_widget_get_toplevel (pevent_dialog)),
		request->exporting ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
		request->exporting ? _("_Export") : _("_Import"), _("_Cancel"));
	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (dialog), TRUE);
	g_object_set_data_full (G_OBJECT (dialog), "request", request, pevent_file_request_free);
	g_signal_connect (dialog, "response", G_CALLBACK (pevent_file_response), request);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

gboolean
pevent_preferences_stage (GError **error)
{
	int i;
	char *contents;
	pevent_pending = FALSE;
	if (!pevent_dialog) return TRUE;
	for (i = 0; i < NUM_XP; i++)
	{
		if (!pevent_preferences_validate (pevent_staged[i], i, error))
		{
			GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (pevent_dialog_list));
			GtkTreeIter iter;
			gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
			setup_reveal_widget (pevent_dialog_list);
			while (valid)
			{
				int event;
				gtk_tree_model_get (model, &iter, ROW_COLUMN, &event, -1);
				if (event == i)
				{
					GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
					gtk_widget_grab_focus (pevent_dialog_list);
					gtk_tree_view_set_cursor (GTK_TREE_VIEW (pevent_dialog_list), path, NULL, FALSE);
					gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (pevent_dialog_list), path, NULL, FALSE, 0, 0);
					gtk_tree_path_free (path);
					break;
				}
				valid = gtk_tree_model_iter_next (model, &iter);
			}
			return FALSE;
		}
		if (g_strcmp0 (pevent_staged[i], pevent_original[i]) != 0) pevent_pending = TRUE;
	}
	if (!pevent_pending) return TRUE;
	for (i = 0; i < NUM_XP; i++)
		if (g_strcmp0 (pntevts_text[i], pevent_original[i]) != 0)
		{
			setup_reveal_widget (pevent_dialog_list);
			g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_AGAIN, _("Text events changed outside Preferences. Reopen Preferences before editing them."));
			return FALSE;
		}
	contents = pevent_preferences_serialize ();
	preferences_persistence_stage_file ("pevents.conf", contents);
	g_free (contents);
	return TRUE;
}

void
pevent_preferences_commit (void)
{
	int i;
	if (!pevent_pending) return;
	for (i = 0; i < NUM_XP; i++)
	{
		g_free (pntevts_text[i]);
		pntevts_text[i] = g_strdup (pevent_staged[i]);
	}
	pevent_make_pntevts ();
	prefs.save_pevents = 1;
}

static void
pevent_test_cb (GtkWidget * wid, GtkWidget * twid)
{
	int len, n;
	char *out, *text;

	for (n = 0; n < NUM_XP; n++)
	{
		text = pevent_staged[n];
		len = strlen (text);

		out = g_malloc (len + 2);
		memcpy (out, text, len + 1);
		out[len] = '\n';
		out[len + 1] = 0;
		check_special_chars (out, TRUE);

		PrintTextRaw (GTK_XTEXT (twid)->buffer, out, 0, 0);
		g_free (out);
	}
}

static GtkWidget *
pevent_treeview_new (GtkWidget *box)
{
	GtkWidget *scroll;
	GtkListStore *store;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkWidget *view;
	GtkCellRenderer *render;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);

	store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	g_return_val_if_fail (store != NULL, NULL);

	view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (view), TRUE);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), TRUE);
	gtk_tree_view_set_grid_lines (GTK_TREE_VIEW (view), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_connect (G_OBJECT (sel), "changed",
		G_CALLBACK (pevent_selection_changed), NULL);

	render = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
			GTK_TREE_VIEW (view), EVENT_COLUMN,
			_("Event"), render,
			"text", EVENT_COLUMN,
			NULL);

	render = gtk_cell_renderer_text_new ();
	g_object_set (render, "editable", TRUE, NULL);
	g_signal_connect (G_OBJECT (render), "edited",
		G_CALLBACK (pevent_edited), NULL);
	gtk_tree_view_insert_column_with_attributes (
			GTK_TREE_VIEW (view), TEXT_COLUMN,
			_("Text"), render,
			"text", TEXT_COLUMN,
			NULL);

	col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), EVENT_COLUMN);
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 100);

	gtk_container_add (GTK_CONTAINER (scroll), view);
	gtk_paned_pack1 (GTK_PANED (box), scroll, TRUE, FALSE);

	return view;
}

static GtkWidget *
pevent_hlist_treeview_new (GtkWidget *box)
{
	GtkWidget *scroll;
	GtkListStore *store;
	GtkTreeViewColumn *col;
	GtkWidget *view;
	GtkCellRenderer *render;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);

	store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
	g_return_val_if_fail (store != NULL, NULL);

	view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (view), TRUE);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
	gtk_widget_set_can_focus (view, FALSE);

	render = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
			GTK_TREE_VIEW (view), 0,
			_("$ Number"), render,
			"text", 0,
			NULL);

	render = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
			GTK_TREE_VIEW (view), 1,
			_("Description"), render,
			"text", 1,
			NULL);

	col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), 0);
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	gtk_container_add (GTK_CONTAINER (scroll), view);
	gtk_paned_pack2 (GTK_PANED (box), scroll, TRUE, FALSE);

	return view;
}

static void
pevent_preferences_preview_realize (GtkWidget *widget, gpointer data)
{
	gtk_xtext_set_font (GTK_XTEXT (widget), prefs.hex_text_font);
}

GtkWidget *
pevent_preferences_create (void)
{
	GtkWidget *vbox, *hbox, *wid, *pane, *preview_pane;
	XTextColor xtext_palette[XTEXT_COLS];

	int i;
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	pevent_dialog = vbox;
	for (i = 0; i < NUM_XP; i++)
	{
		pevent_original[i] = g_strdup (pntevts_text[i]);
		pevent_staged[i] = g_strdup (pntevts_text[i]);
	}
	g_signal_connect (vbox, "destroy", G_CALLBACK (pevent_preferences_destroy), NULL);

	preview_pane = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start (GTK_BOX (vbox), preview_pane, TRUE, TRUE, 0);
	pane = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_paned_pack1 (GTK_PANED (preview_pane), pane, TRUE, FALSE);
	gtk_paned_set_position (GTK_PANED (pane), 220);
	gtk_paned_set_position (GTK_PANED (preview_pane), 330);
	
	pevent_dialog_list = pevent_treeview_new (pane);
	pevent_dialog_fill (pevent_dialog_list);

	pevent_dialog_hlist = pevent_hlist_treeview_new (pane);

	wid = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wid), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (wid), 75);
	gtk_paned_pack2 (GTK_PANED (preview_pane), wid, TRUE, FALSE);

	theme_get_xtext_colors_for_widget (wid, xtext_palette, XTEXT_COLS);
	pevent_dialog_twid = gtk_xtext_new (xtext_palette, 0);
	gtk_widget_set_sensitive (pevent_dialog_twid, FALSE);
	gtk_container_add (GTK_CONTAINER (wid), pevent_dialog_twid);
	g_signal_connect (pevent_dialog_twid, "realize", G_CALLBACK (pevent_preferences_preview_realize), NULL);
	g_object_set_data (G_OBJECT (pevent_dialog), "xtext", pevent_dialog_twid);
	g_object_set_data (G_OBJECT (pevent_dialog), PEVENT_THEME_LISTENER_ID_KEY,
				   GUINT_TO_POINTER (theme_listener_register ("textgui.events", pevent_dialog_theme_changed, pevent_dialog)));
	g_signal_connect (G_OBJECT (pevent_dialog), "destroy", G_CALLBACK (pevent_dialog_theme_destroy_cb), NULL);

	hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
	gtkutil_button (hbox, ICON_TEXTEVENT_SAVE_AS, NULL, pevent_file_open,
						 (void *) 1, _("Export..."));
	gtkutil_button (hbox, ICON_TEXTEVENT_OPEN, NULL, pevent_file_open,
						 NULL, _("Import..."));
	gtkutil_button (hbox, NULL, NULL, pevent_test_cb,
						pevent_dialog_twid, _("Test All"));

	gtk_widget_show_all (pevent_dialog);
	pevent_dialog_theme_apply (pevent_dialog);
	return vbox;
}

void
pevent_dialog_show (void)
{
	setup_open_page ("events");
}
