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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gdk/gdkkeysyms.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "fe-gtk.h"

#include "../common/zoitechat.h"
#include "../common/cfgfiles.h"
#include "../common/zoitechatc.h"
#include "../common/fe.h"
#include "menu.h"
#include "gtkutil.h"
#include "maingui.h"
#include "editlist.h"
#include "preferences-persistence.h"
#include "setup.h"

#define ICON_EDITLIST_NEW "document-new"
#define ICON_EDITLIST_DELETE "edit-delete"
#define ICON_EDITLIST_CANCEL "dialog-cancel"
#define ICON_EDITLIST_SAVE "document-save"

enum
{
	NAME_COLUMN,
	CMD_COLUMN,
	N_COLUMNS
};

static GtkWidget *editlist_win = NULL;
static GSList *editlist_list = NULL;

static GtkTreeModel *
get_store (void)
{
	return gtk_tree_view_get_model (g_object_get_data (G_OBJECT (editlist_win), "view"));
}

static void
editlist_save (GtkWidget *igad, gchar *file)
{
	GtkTreeModel *store = get_store ();
	GtkTreeIter iter;
	char buf[512];
	char *name, *cmd;
	int fh;

	fh = zoitechat_open_file (file, O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
	if (fh != -1)
	{
		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
		{
			do
			{
				name = NULL;
				cmd = NULL;
				gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, NAME_COLUMN, &name, CMD_COLUMN, &cmd, -1);
				g_snprintf (buf, sizeof (buf), "NAME %s\nCMD %s\n\n", name, cmd);
				write (fh, buf, strlen (buf));
				g_free (name);
				g_free (cmd);
			} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
		}

		close (fh);
		gtk_widget_destroy (editlist_win);
		if (editlist_list == replace_list)
		{
			list_free (&replace_list);
			list_loadconf (file, &replace_list, 0);
		} else if (editlist_list == popup_list)
		{
			list_free (&popup_list);
			list_loadconf (file, &popup_list, 0);
		} else if (editlist_list == button_list)
		{
			GSList *list = sess_list;
			struct session *sess;
			list_free (&button_list);
			list_loadconf (file, &button_list, 0);
			while (list)
			{
				sess = (struct session *) list->data;
				fe_buttons_update (sess);
				list = list->next;
			}
		} else if (editlist_list == dlgbutton_list)
		{
			GSList *list = sess_list;
			struct session *sess;
			list_free (&dlgbutton_list);
			list_loadconf (file, &dlgbutton_list, 0);
			while (list)
			{
				sess = (struct session *) list->data;
				fe_dlgbuttons_update (sess);
				list = list->next;
			}
		} else if (editlist_list == ctcp_list)
		{
			list_free (&ctcp_list);
			list_loadconf (file, &ctcp_list, 0);
		} else if (editlist_list == command_list)
		{
			list_free (&command_list);
			list_loadconf (file, &command_list, 0);
		} else if (editlist_list == usermenu_list)
		{
			list_free (&usermenu_list);
			list_loadconf (file, &usermenu_list, 0);
			usermenu_update ();
		} else
		{
			list_free (&urlhandler_list);
			list_loadconf (file, &urlhandler_list, 0);
		}
	}
}

static void
editlist_load (GtkListStore *store, GSList *list)
{
	struct popup *pop;
	gchar *name, *cmd;
	GtkTreeIter iter;

	while (list)
	{
		pop = (struct popup *) list->data;
		name = pop->name;
		cmd = pop->cmd;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
						NAME_COLUMN, name,
						CMD_COLUMN, cmd, -1);

		list = list->next;
	}
}

static void
editlist_delete (GtkWidget *wid, gpointer userdata)
{
	GtkTreeView *view = g_object_get_data (G_OBJECT (editlist_win), "view");
	GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
	GtkTreeIter iter;
	GtkTreePath *path;

	if (gtkutil_treeview_get_selected (view, &iter, -1))
	{
		/* delete this row, select next one */
		if (gtk_list_store_remove (store, &iter))
		{
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
			gtk_tree_view_scroll_to_cell (view, path, NULL, TRUE, 1.0, 0.0);
			gtk_tree_view_set_cursor (view, path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
	}
}

static void
editlist_add (GtkWidget *wid, gpointer userdata)
{
	GtkTreeView *view = g_object_get_data (G_OBJECT (editlist_win), "view");
	GtkTreeViewColumn *col;
	GtkListStore *store = GTK_LIST_STORE (get_store ());
	GtkTreeIter iter;
	GtkTreePath *path;

	gtk_list_store_append (store, &iter);

	/* make sure the new row is visible and selected */
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
	col = gtk_tree_view_get_column (view, NAME_COLUMN);
	gtk_tree_view_scroll_to_cell (view, path, NULL, FALSE, 0.0, 0.0);
	gtk_tree_view_set_cursor (view, path, col, TRUE);
	gtk_tree_path_free (path);
}

static void
editlist_close (GtkWidget *wid, gpointer userdata)
{
	gtk_widget_destroy (editlist_win);
	editlist_win = NULL;
}

static void
editlist_edited (GtkCellRendererText *render, gchar *pathstr, gchar *new_text, gpointer data)
{
	GtkTreeModel *model = get_store ();
	GtkTreePath *path = gtk_tree_path_new_from_string (pathstr);
	GtkTreeIter iter;
	gint column = GPOINTER_TO_INT (data);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, new_text, -1);

	gtk_tree_path_free (path);
}

static gboolean
editlist_keypress (GtkWidget *wid, GdkEventKey *evt, gpointer userdata)
{
	GtkTreeView *view = g_object_get_data (G_OBJECT (editlist_win), "view");
	GtkTreeModel *store;
	GtkTreeIter iter1, iter2;
	GtkTreeSelection *sel;
	GtkTreePath *path;
	gboolean handled = FALSE;
	int delta;

	if (evt->state & GDK_SHIFT_MASK)
	{
		if (evt->keyval == GDK_KEY_Up)
		{
			handled = TRUE;
			delta = -1;
		}
		else if (evt->keyval == GDK_KEY_Down)
		{
			handled = TRUE;
			delta = 1;
		}
	}

	if (handled)
	{
		sel = gtk_tree_view_get_selection (view);
		gtk_tree_selection_get_selected (sel, &store, &iter1);
		path = gtk_tree_model_get_path (store, &iter1);
		if (delta == 1)
			gtk_tree_path_next (path);
		else
			gtk_tree_path_prev (path);
		gtk_tree_model_get_iter (store, &iter2, path);
		gtk_tree_path_free (path);
		gtk_list_store_swap (GTK_LIST_STORE (store), &iter1, &iter2);
	}

	return handled;
}

static GtkWidget *
editlist_treeview_new (GtkWidget *box, char *title1, char *title2)
{
	GtkWidget *scroll;
	GtkListStore *store;
	GtkTreeViewColumn *col;
	GtkWidget *view;
	GtkCellRenderer *render;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);

	store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	g_return_val_if_fail (store != NULL, NULL);

	view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (view), TRUE);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (view), TRUE);
	
	g_signal_connect (G_OBJECT (view), "key-press-event",
						G_CALLBACK (editlist_keypress), NULL);

	gtk_tree_view_set_grid_lines (GTK_TREE_VIEW (view), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

	render = gtk_cell_renderer_text_new ();
	g_object_set (render, "editable", TRUE, NULL);
	g_signal_connect (G_OBJECT (render), "edited",
					G_CALLBACK (editlist_edited), GINT_TO_POINTER(NAME_COLUMN));
	gtk_tree_view_insert_column_with_attributes (
					GTK_TREE_VIEW (view), NAME_COLUMN,
					title1, render,
					"text", NAME_COLUMN,
					NULL);

	render = gtk_cell_renderer_text_new ();
	g_object_set (render, "editable", TRUE, NULL);
	g_signal_connect (G_OBJECT (render), "edited",
					G_CALLBACK (editlist_edited), GINT_TO_POINTER(CMD_COLUMN));
	gtk_tree_view_insert_column_with_attributes (
					GTK_TREE_VIEW (view), CMD_COLUMN,
					title2, render,
					"text", CMD_COLUMN,
					NULL);

	col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), NAME_COLUMN);
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 100);

	gtk_container_add (GTK_CONTAINER (scroll), view);
	gtk_box_pack_start (GTK_BOX (box), scroll, TRUE, TRUE, 0);
	gtk_widget_show_all (box);

	return view;
}


void
editlist_gui_open (char *title1, char *title2, GSList *list, char *title, char *wmclass,
					char *file, char *help)
{
	GtkWidget *vbox, *box;
	GtkWidget *view;
	GtkListStore *store;

	if (editlist_win)
	{
		mg_bring_tofront (editlist_win);
		return;
	}

	editlist_win = mg_create_generic_tab (wmclass, title, TRUE, FALSE,
												editlist_close, NULL, 450, 250, &vbox, 0);

	editlist_list = list;

	view = editlist_treeview_new (vbox, title1, title2);
	g_object_set_data (G_OBJECT (editlist_win), "view", view);

	if (help)
		gtk_widget_set_tooltip_text (view, help);

	box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (box), 5);
	gtk_widget_show (box);

	gtkutil_button (box, ICON_EDITLIST_NEW, 0, editlist_add,
					NULL, _("Add"));
	gtkutil_button (box, ICON_EDITLIST_DELETE, 0, editlist_delete,
					NULL, _("Delete"));
	gtkutil_button (box, ICON_EDITLIST_CANCEL, 0, editlist_close,
					NULL, _("Cancel"));
	gtkutil_button (box, ICON_EDITLIST_SAVE, 0, editlist_save,
					file, _("Save"));

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
	editlist_load (store, list);

	gtk_widget_show (editlist_win);
}

/* Preferences editors are independent instances: several lists may be open
 * at once, and their models must never share the legacy dialog globals. */
typedef struct
{
        GtkWidget *view;
        GtkWidget *delete_button;
        GtkWidget *up_button;
        GtkWidget *down_button;
        GSList **target;
        const char *file;
        char *original;
        char *pending;
} PreferencesList;

static GSList *preferences_lists;

static void
preferences_list_update_buttons (PreferencesList *editor)
{
        GtkTreeModel *model;
        GtkTreeIter iter, next;
        GtkTreePath *path;
        gboolean selected = gtk_tree_selection_get_selected (
                gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->view)), &model, &iter);
        gboolean up = FALSE, down = FALSE;
        if (selected)
        {
                path = gtk_tree_model_get_path (model, &iter);
                up = gtk_tree_path_get_indices (path)[0] > 0;
                gtk_tree_path_free (path);
                next = iter;
                down = gtk_tree_model_iter_next (model, &next);
        }
        gtk_widget_set_sensitive (editor->delete_button, selected);
        gtk_widget_set_sensitive (editor->up_button, up);
        gtk_widget_set_sensitive (editor->down_button, down);
}

static void
preferences_list_selection_changed (GtkTreeSelection *selection, PreferencesList *editor)
{
        preferences_list_update_buttons (editor);
}

static void
preferences_list_rows_reordered (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
                                gint *order, PreferencesList *editor)
{
        preferences_list_update_buttons (editor);
}

static char *
preferences_list_serialize (GSList *list)
{
        GString *text = g_string_new ("");
        for (; list; list = list->next)
        {
                struct popup *item = list->data;
                g_string_append_printf (text, "NAME %s\nCMD %s\n\n", item->name, item->cmd);
        }
        return g_string_free (text, FALSE);
}

static void
preferences_list_free (gpointer data)
{
        PreferencesList *editor = data;
        preferences_lists = g_slist_remove (preferences_lists, editor);
        g_free (editor->original);
        g_free (editor->pending);
        g_free (editor);
}

static void
preferences_list_edited (GtkCellRenderer *renderer, char *path, char *text, PreferencesList *editor)
{
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (editor->view));
        if (gtk_tree_model_get_iter_from_string (model, &iter, path))
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "column")), text, -1);
}

static void
preferences_list_add (GtkButton *button, PreferencesList *editor)
{
        GtkTreeIter iter;
        GtkTreePath *path;
        GtkTreeView *view = GTK_TREE_VIEW (editor->view);
        GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
        (void)button;
        gtk_list_store_insert_with_values (store, &iter, -1, NAME_COLUMN, "", CMD_COLUMN, "", -1);
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
        gtk_tree_view_set_cursor (view, path, gtk_tree_view_get_column (view, 0), TRUE);
        gtk_tree_view_scroll_to_cell (view, path, NULL, FALSE, 0, 0);
        gtk_tree_path_free (path);
        preferences_list_update_buttons (editor);
}

static void
preferences_list_delete (GtkButton *button, PreferencesList *editor)
{
        GtkTreeIter iter;
        GtkTreeModel *model;
        (void)button;
        if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->view)), &model, &iter))
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
preferences_list_move (GtkButton *button, PreferencesList *editor)
{
        GtkTreeModel *model;
        GtkTreeIter iter, other;
        GtkTreePath *path;
        gboolean valid;
        if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->view)), &model, &iter))
                return;
        path = gtk_tree_model_get_path (model, &iter);
        if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "direction")) < 0)
                valid = gtk_tree_path_prev (path);
        else { gtk_tree_path_next (path); valid = TRUE; }
        if (valid && gtk_tree_model_get_iter (model, &other, path))
        {
                gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &other);
                gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (editor->view), path, NULL, FALSE, 0, 0);
        }
        gtk_tree_path_free (path);
}

static gboolean
preferences_list_key (GtkWidget *view, GdkEventKey *event, PreferencesList *editor)
{
        GtkWidget *button;
        int direction;
        (void)view;
        if (!(event->state & GDK_SHIFT_MASK) || (event->keyval != GDK_KEY_Up && event->keyval != GDK_KEY_Down))
                return FALSE;
        direction = event->keyval == GDK_KEY_Up ? -1 : 1;
        button = gtk_button_new ();
        g_object_ref_sink (button);
        g_object_set_data (G_OBJECT (button), "direction", GINT_TO_POINTER (direction));
        preferences_list_move (GTK_BUTTON (button), editor);
        g_object_unref (button);
        return TRUE;
}

GtkWidget *
editlist_preferences_create (GSList **list, const char *file, const char *title1,
                             const char *title2, const char *help)
{
        PreferencesList *editor = g_new0 (PreferencesList, 1);
        GtkWidget *box, *scroll, *buttons, *button;
        GtkListStore *store;
        GtkCellRenderer *renderer;
        int column;
        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
        editor->target = list;
        editor->file = file;
        editor->original = preferences_list_serialize (*list);
        preferences_lists = g_slist_append (preferences_lists, editor);
        g_object_set_data_full (G_OBJECT (box), "preferences-list", editor, preferences_list_free);
        store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
        editor->view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
        gtk_tree_view_set_reorderable (GTK_TREE_VIEW (editor->view), TRUE);
        gtk_tree_view_set_search_column (GTK_TREE_VIEW (editor->view), NAME_COLUMN);
        for (column = 0; column < N_COLUMNS; column++)
        {
                GtkTreeViewColumn *col;
                renderer = gtk_cell_renderer_text_new ();
                g_object_set (renderer, "editable", TRUE, NULL);
                g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (column));
                g_signal_connect (renderer, "edited", G_CALLBACK (preferences_list_edited), editor);
                col = gtk_tree_view_column_new_with_attributes (column ? title2 : title1, renderer, "text", column, NULL);
                gtk_tree_view_column_set_resizable (col, TRUE);
                gtk_tree_view_column_set_min_width (col, column ? 240 : 140);
                gtk_tree_view_column_set_expand (col, column != 0);
                gtk_tree_view_append_column (GTK_TREE_VIEW (editor->view), col);
        }
        editlist_load (store, *list);
        g_object_unref (store);
        g_signal_connect (editor->view, "key-press-event", G_CALLBACK (preferences_list_key), editor);
        scroll = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll), 220);
        gtk_container_add (GTK_CONTAINER (scroll), editor->view);
        gtk_box_pack_start (GTK_BOX (box), scroll, TRUE, TRUE, 0);
        buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_pack_start (GTK_BOX (box), buttons, FALSE, FALSE, 0);
        button = gtk_button_new_with_mnemonic (_("_Add"));
        g_signal_connect (button, "clicked", G_CALLBACK (preferences_list_add), editor);
        gtk_box_pack_start (GTK_BOX (buttons), button, FALSE, FALSE, 0);
        button = gtk_button_new_with_mnemonic (_("_Delete"));
        editor->delete_button = button;
        g_signal_connect (button, "clicked", G_CALLBACK (preferences_list_delete), editor);
        gtk_box_pack_start (GTK_BOX (buttons), button, FALSE, FALSE, 0);
        for (column = -1; column <= 1; column += 2)
        {
                button = gtk_button_new_with_label (column < 0 ? _("Move up") : _("Move down"));
                if (column < 0) editor->up_button = button;
                else editor->down_button = button;
                g_object_set_data (G_OBJECT (button), "direction", GINT_TO_POINTER (column));
                g_signal_connect (button, "clicked", G_CALLBACK (preferences_list_move), editor);
                gtk_box_pack_start (GTK_BOX (buttons), button, FALSE, FALSE, 0);
        }
        g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->view)), "changed",
                G_CALLBACK (preferences_list_selection_changed), editor);
        g_signal_connect (gtk_tree_view_get_model (GTK_TREE_VIEW (editor->view)), "rows-reordered",
                G_CALLBACK (preferences_list_rows_reordered), editor);
        preferences_list_update_buttons (editor);
        if (help)
        {
                GtkWidget *expander = gtk_expander_new (_("Formatting codes and examples"));
                GtkWidget *label = gtk_label_new (help);
                gtk_label_set_xalign (GTK_LABEL (label), 0);
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_label_set_selectable (GTK_LABEL (label), TRUE);
                gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
                gtk_widget_set_margin_top (label, 10);
                gtk_container_add (GTK_CONTAINER (expander), label);
                gtk_box_pack_start (GTK_BOX (box), expander, FALSE, FALSE, 0);
        }
        return box;
}

gboolean
editlist_preferences_stage (GError **error)
{
        GSList *entry;
        for (entry = preferences_lists; entry; entry = entry->next)
        {
                PreferencesList *editor = entry->data;
                GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (editor->view));
                GtkTreeIter iter;
                GString *text = g_string_new ("");
                gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
                char *live;
                g_clear_pointer (&editor->pending, g_free);
                while (valid)
                {
                        char *name, *cmd;
                        gtk_tree_model_get (model, &iter, NAME_COLUMN, &name, CMD_COLUMN, &cmd, -1);
                        if ((name && strpbrk (name, "\r\n")) || (cmd && strpbrk (cmd, "\r\n")) ||
                            ((!name || !*name) && cmd && *cmd))
                        {
                                GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
                                int column = !name || !*name || strpbrk (name, "\r\n") ? NAME_COLUMN : CMD_COLUMN;
                                setup_reveal_widget (editor->view);
                                gtk_widget_grab_focus (editor->view);
                                gtk_tree_view_set_cursor (GTK_TREE_VIEW (editor->view), path,
                                        gtk_tree_view_get_column (GTK_TREE_VIEW (editor->view), column), TRUE);
                                gtk_tree_path_free (path);
                                g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                        _("Give this entry a name and keep each field on one line."));
                                g_free (name); g_free (cmd); g_string_free (text, TRUE);
                                return FALSE;
                        }
                        if (name && *name)
                                g_string_append_printf (text, "NAME %s\nCMD %s\n\n", name, cmd ? cmd : "");
                        g_free (name); g_free (cmd);
                        valid = gtk_tree_model_iter_next (model, &iter);
                }
                if (strcmp (text->str, editor->original) == 0) { g_string_free (text, TRUE); continue; }
                live = preferences_list_serialize (*editor->target);
                if (strcmp (live, editor->original) != 0)
                {
                        setup_reveal_widget (editor->view);
                        g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_AGAIN,
                                _("These entries changed outside Preferences. Reopen Preferences before editing them."));
                        g_free (live); g_string_free (text, TRUE);
                        return FALSE;
                }
                g_free (live);
                editor->pending = g_string_free (text, FALSE);
                preferences_persistence_stage_file (editor->file, editor->pending);
        }
        return TRUE;
}

void
editlist_preferences_commit (void)
{
        GSList *entry, *sessions;
        for (entry = preferences_lists; entry; entry = entry->next)
        {
                PreferencesList *editor = entry->data;
                if (!editor->pending) continue;
                list_free (editor->target);
                list_loadconf ((char *)editor->file, editor->target, NULL);
                if (editor->target == &usermenu_list) usermenu_update ();
                for (sessions = sess_list; sessions; sessions = sessions->next)
                {
                        if (editor->target == &button_list) fe_buttons_update (sessions->data);
                        if (editor->target == &dlgbutton_list) fe_dlgbuttons_update (sessions->data);
                }
        }
}
