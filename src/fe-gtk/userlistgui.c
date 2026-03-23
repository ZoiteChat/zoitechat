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
#include <string.h>
#include <stdlib.h>

#include "fe-gtk.h"

#include <gdk/gdkkeysyms.h>

#include "../common/zoitechat.h"
#include "../common/util.h"
#include "../common/userlist.h"
#include "../common/modes.h"
#include "../common/text.h"
#include "../common/notify.h"
#include "../common/zoitechatc.h"
#include "../common/fe.h"
#include "gtkutil.h"
#include "theme/theme-gtk.h"
#include "maingui.h"
#include "menu.h"
#include "pixmaps.h"
#include "theme/theme-access.h"
#include "userlistgui.h"
#include "fkeys.h"

enum
{
	COL_PIX=0,		/* GdkPixbuf * */
	COL_PREFIX=1,	/* char * */
	COL_NICK=2,		/* char * */
	COL_HOST=3,		/* char * */
	COL_USER=4,		/* struct User * */
	COL_GDKCOLOR=5	/* GdkRGBA */
};

static void userlist_store_color (GtkListStore *store, GtkTreeIter *iter, ThemeSemanticToken token, gboolean has_token);

static const char *
userlist_prefix_color (char prefix)
{
	switch (prefix)
	{
		case '~': return "#d46a6a";
		case '@': return "#5ea36a";
		case '%': return "#d39a5f";
		case '&': return "#79aecd";
		case '+': return "#d2bf6a";
		default: return NULL;
	}
}

static void
userlist_column_width_notify_cb (GtkTreeViewColumn *column, GParamSpec *pspec, gpointer userdata)
{
	(void)pspec;

	int width = gtk_tree_view_column_get_width (column);
	int *target = (int *)userdata;

	if (!target || width < 1 || *target == width)
		return;

	*target = width;
}

static void
userlist_apply_saved_column_width (GtkTreeViewColumn *column, int width)
{
	if (!column || width < 1)
		return;

	gtk_tree_view_column_set_fixed_width (column, width);
}

static void
userlist_update_min_width (session *sess)
{
	GtkRequisition minimum;
	GtkRequisition natural;
	GtkWidget *scrolled_window;
	int width;

	if (!sess || !sess->gui || !sess->gui->user_box || !sess->gui->namelistinfo || !sess->gui->user_tree)
		return;

	gtk_widget_get_preferred_size (sess->gui->namelistinfo, &minimum, &natural);
	width = MAX (minimum.width, natural.width);
	if (width < 1)
		width = 1;

	scrolled_window = gtk_widget_get_parent (sess->gui->user_tree);
	if (GTK_IS_SCROLLED_WINDOW (scrolled_window))
		gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolled_window), width);
	gtk_widget_set_size_request (sess->gui->user_box, width, -1);
}

GdkPixbuf *
get_user_icon (server *serv, struct User *user)
{
	char *pre;
	int level;

	if (!user)
		return NULL;

	/* these ones are hardcoded */
	switch (user->prefix[0])
	{
		case 0: return NULL;
		case '+': return pix_ulist_voice;
		case '%': return pix_ulist_halfop;
		case '@': return pix_ulist_op;
	}

	/* find out how many levels above Op this user is */
	pre = strchr (serv->nick_prefixes, '@');
	if (pre && pre != serv->nick_prefixes)
	{
		pre--;
		level = 0;
		while (1)
		{
			if (pre[0] == user->prefix[0])
			{
				switch (level)
				{
					case 0: return pix_ulist_owner;		/* 1 level above op */
					case 1: return pix_ulist_founder;	/* 2 levels above op */
					case 2: return pix_ulist_netop;		/* 3 levels above op */
				}
				break;	/* 4+, no icons */
			}
			level++;
			if (pre == serv->nick_prefixes)
				break;
			pre--;
		}
	}

	return NULL;
}

void
fe_userlist_numbers (session *sess)
{
	char tbuf[256];

	if (sess == current_tab || !sess->gui->is_tab)
	{
		if (sess->total)
		{
			g_snprintf (tbuf, sizeof (tbuf), _("%d ops, %d total"), sess->ops, sess->total);
			tbuf[sizeof (tbuf) - 1] = 0;
			gtk_label_set_text (GTK_LABEL (sess->gui->namelistinfo), tbuf);
			userlist_update_min_width (sess);
		} else
		{
			gtk_label_set_text (GTK_LABEL (sess->gui->namelistinfo), NULL);
			userlist_update_min_width (sess);
		}

		if (sess->type == SESS_CHANNEL && prefs.hex_gui_win_ucount)
			fe_set_title (sess);
	}
}

static void
scroll_to_iter (GtkTreeIter *iter, GtkTreeView *treeview, GtkTreeModel *model)
{
	GtkTreePath *path = gtk_tree_model_get_path (model, iter);
	if (path)
	{
		gtk_tree_view_scroll_to_cell (treeview, path, NULL, TRUE, 0.5, 0.5);
		gtk_tree_path_free (path);
	}
}

static GHashTable *
userlist_row_map_ensure (session *sess)
{
	if (!sess->res->user_row_refs)
		sess->res->user_row_refs = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) gtk_tree_row_reference_free);

	return sess->res->user_row_refs;
}

static void
userlist_row_map_remove (session *sess, struct User *user)
{
	if (!sess->res->user_row_refs)
		return;

	g_hash_table_remove (sess->res->user_row_refs, user);
}

static void
userlist_row_map_set (session *sess, GtkTreeModel *model, struct User *user, GtkTreeIter *iter)
{
	GtkTreePath *path;
	GtkTreeRowReference *ref;

	path = gtk_tree_model_get_path (model, iter);
	if (!path)
		return;

	ref = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);
	if (!ref)
		return;

	g_hash_table_replace (userlist_row_map_ensure (sess), user, ref);
}

static gboolean
userlist_row_map_get_iter (session *sess, GtkTreeModel *model, struct User *user, GtkTreeIter *iter)
{
	GtkTreeRowReference *ref;
	GtkTreePath *path;
	struct User *row_user;

	if (!sess->res->user_row_refs)
		return FALSE;

	ref = g_hash_table_lookup (sess->res->user_row_refs, user);
	if (!ref)
		return FALSE;

	path = gtk_tree_row_reference_get_path (ref);
	if (!path)
	{
		g_hash_table_remove (sess->res->user_row_refs, user);
		return FALSE;
	}

	if (!gtk_tree_model_get_iter (model, iter, path))
	{
		gtk_tree_path_free (path);
		g_hash_table_remove (sess->res->user_row_refs, user);
		return FALSE;
	}
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, iter, COL_USER, &row_user, -1);
	if (row_user != user)
	{
		g_hash_table_remove (sess->res->user_row_refs, user);
		return FALSE;
	}

	return TRUE;
}

/* select a row in the userlist by nick-name */

void
userlist_select (session *sess, char *name)
{
	GtkTreeIter iter;
	GtkTreeView *treeview = GTK_TREE_VIEW (sess->gui->user_tree);
	GtkTreeModel *model = gtk_tree_view_get_model (treeview);
	GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
	struct User *user = userlist_find (sess, name);
	struct User *row_user;

	if (user && userlist_row_map_get_iter (sess, model, user, &iter))
	{
		if (gtk_tree_selection_iter_is_selected (selection, &iter))
			gtk_tree_selection_unselect_iter (selection, &iter);
		else
			gtk_tree_selection_select_iter (selection, &iter);

		scroll_to_iter (&iter, treeview, model);
		return;
	}

	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		do
		{
			gtk_tree_model_get (model, &iter, COL_USER, &row_user, -1);
			if (sess->server->p_cmp (row_user->nick, name) == 0)
			{
				userlist_row_map_set (sess, model, row_user, &iter);
				if (gtk_tree_selection_iter_is_selected (selection, &iter))
					gtk_tree_selection_unselect_iter (selection, &iter);
				else
					gtk_tree_selection_select_iter (selection, &iter);

				/* and make sure it's visible */
				scroll_to_iter (&iter, treeview, model);
				return;
			}
		}
		while (gtk_tree_model_iter_next (model, &iter));
	}
}

char **
userlist_selection_list (GtkWidget *widget, int *num_ret)
{
	GtkTreeIter iter;
	GtkTreeView *treeview = (GtkTreeView *) widget;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
	GtkTreeModel *model = gtk_tree_view_get_model (treeview);
	struct User *user;
	int i, num_sel;
	char **nicks;

	*num_ret = 0;
	/* first, count the number of selections */
	num_sel = 0;
	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		do
		{
			if (gtk_tree_selection_iter_is_selected (selection, &iter))
				num_sel++;
		}
		while (gtk_tree_model_iter_next (model, &iter));
	}

	if (num_sel < 1)
		return NULL;

	nicks = g_new (char *, num_sel + 1);

	i = 0;
	gtk_tree_model_get_iter_first (model, &iter);
	do
	{
		if (gtk_tree_selection_iter_is_selected (selection, &iter))
		{
			gtk_tree_model_get (model, &iter, COL_USER, &user, -1);
			nicks[i] = g_strdup (user->nick);
			i++;
			nicks[i] = NULL;
		}
	}
	while (gtk_tree_model_iter_next (model, &iter));

	*num_ret = i;
	return nicks;
}

void
fe_userlist_set_selected (struct session *sess)
{
	GtkListStore *store = sess->res->user_model;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sess->gui->user_tree));
	GtkTreeIter iter;
	struct User *user;

	/* if it's not front-most tab it doesn't own the GtkTreeView! */
	if (store != (GtkListStore*) gtk_tree_view_get_model (GTK_TREE_VIEW (sess->gui->user_tree)))
		return;

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
	{
		do
		{
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_USER, &user, -1);

			if (gtk_tree_selection_iter_is_selected (selection, &iter))
				user->selected = 1;
			else
				user->selected = 0;
				
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
	}
}

static GtkTreeIter *
find_row (session *sess, GtkTreeView *treeview, GtkTreeModel *model, struct User *user,
			 int *selected)
{
	static GtkTreeIter iter;
	struct User *row_user;

	*selected = FALSE;
	if (userlist_row_map_get_iter (sess, model, user, &iter))
	{
		if (gtk_tree_view_get_model (treeview) == model)
		{
			if (gtk_tree_selection_iter_is_selected (gtk_tree_view_get_selection (treeview), &iter))
				*selected = TRUE;
		}
		return &iter;
	}

	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		do
		{
			gtk_tree_model_get (model, &iter, COL_USER, &row_user, -1);
			if (row_user == user)
			{
				userlist_row_map_set (sess, model, row_user, &iter);
				if (gtk_tree_view_get_model (treeview) == model)
				{
					if (gtk_tree_selection_iter_is_selected (gtk_tree_view_get_selection (treeview), &iter))
						*selected = TRUE;
				}
				return &iter;
			}
		}
		while (gtk_tree_model_iter_next (model, &iter));
	}

	return NULL;
}

void
userlist_set_value (GtkWidget *treeview, gfloat val)
{
	gtk_adjustment_set_value (
			gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (treeview)), val);
}

gfloat
userlist_get_value (GtkWidget *treeview)
{
	return gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (treeview)));
}

int
fe_userlist_remove (session *sess, struct User *user)
{
	GtkTreeIter *iter;
/*	GtkAdjustment *adj;
	gfloat val, end;*/
	int sel;

	iter = find_row (sess, GTK_TREE_VIEW (sess->gui->user_tree),
					  GTK_TREE_MODEL(sess->res->user_model), user, &sel);
	if (!iter)
		return 0;
	userlist_row_map_remove (sess, user);

/*	adj = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (sess->gui->user_tree));
	val = adj->value;*/

	gtk_list_store_remove (sess->res->user_model, iter);

	/* is it the front-most tab? */
/*	if (gtk_tree_view_get_model (GTK_TREE_VIEW (sess->gui->user_tree))
		 == sess->res->user_model)
	{
		end = adj->upper - adj->lower - adj->page_size;
		if (val > end)
			val = end;
		gtk_adjustment_set_value (adj, val);
	}*/

	return sel;
}

void
fe_userlist_rehash (session *sess, struct User *user)
{
	GtkTreeIter *iter;
	int sel;
	ThemeSemanticToken nick_token = THEME_TOKEN_TEXT_FOREGROUND;
	gboolean have_nick_token = FALSE;

	iter = find_row (sess, GTK_TREE_VIEW (sess->gui->user_tree),
					  GTK_TREE_MODEL(sess->res->user_model), user, &sel);
	if (!iter)
		return;
	userlist_row_map_set (sess, GTK_TREE_MODEL (sess->res->user_model), user, iter);

	if (prefs.hex_away_track && user->away)
	{
		nick_token = THEME_TOKEN_TAB_AWAY;
		have_nick_token = TRUE;
	}
	else if (prefs.hex_gui_ulist_color || prefs.hex_text_color_nicks)
	{
		int mirc_index = text_color_of (user->nick);

		if (mirc_index >= 0 && mirc_index < 32)
		{
			nick_token = (ThemeSemanticToken) (THEME_TOKEN_MIRC_0 + mirc_index);
			have_nick_token = TRUE;
		}
	}

	gtk_list_store_set (GTK_LIST_STORE (sess->res->user_model), iter,
							  COL_HOST, user->hostname,
							  -1);
	userlist_store_color (GTK_LIST_STORE (sess->res->user_model), iter, nick_token, have_nick_token);
}

void
fe_userlist_insert (session *sess, struct User *newuser, gboolean sel)
{
	GtkTreeModel *model = GTK_TREE_MODEL(sess->res->user_model);
	GdkPixbuf *pix = get_user_icon (sess->server, newuser);
	GtkTreeIter iter;
	char *nick;
	char *nick_escaped;
	char *prefix = NULL;
	char *prefix_escaped;
	char prefix_text[2];
	const char *prefix_color;
	ThemeSemanticToken nick_token = THEME_TOKEN_TEXT_FOREGROUND;
	gboolean have_nick_token = FALSE;

	if (prefs.hex_away_track && newuser->away)
	{
		nick_token = THEME_TOKEN_TAB_AWAY;
		have_nick_token = TRUE;
	}
	else if (prefs.hex_gui_ulist_color || prefs.hex_text_color_nicks)
	{
		int mirc_index = text_color_of (newuser->nick);

		if (mirc_index >= 0 && mirc_index < 32)
		{
			nick_token = (ThemeSemanticToken) (THEME_TOKEN_MIRC_0 + mirc_index);
			have_nick_token = TRUE;
		}
	}

	nick_escaped = g_markup_escape_text (newuser->nick, -1);
	nick = nick_escaped;
	if (!prefs.hex_gui_ulist_icons)
	{
		if (newuser->prefix[0] != '\0' && newuser->prefix[0] != ' ')
		{
			prefix_text[0] = newuser->prefix[0];
			prefix_text[1] = '\0';
			prefix_escaped = g_markup_escape_text (prefix_text, -1);
			prefix_color = userlist_prefix_color (newuser->prefix[0]);
			if (prefix_color)
				prefix = g_strdup_printf ("<b><span foreground=\"%s\">%s</span></b>", prefix_color, prefix_escaped);
			else
				prefix = g_strdup_printf ("<b>%s</b>", prefix_escaped);
			g_free (prefix_escaped);
		}
		pix = NULL;
	}

	gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter, 0,
									COL_PIX, pix,
									COL_PREFIX, prefix,
									COL_NICK, nick,
									COL_HOST, newuser->hostname,
									COL_USER, newuser,
								  -1);
	userlist_store_color (GTK_LIST_STORE (model), &iter, nick_token, have_nick_token);

	g_free (prefix);
	g_free (nick_escaped);

	userlist_row_map_set (sess, model, newuser, &iter);

	/* is it me? */
	if (newuser->me && sess->gui->nick_box)
	{
		if (!sess->gui->is_tab || sess == current_tab)
			mg_set_access_icon (sess->gui, pix, sess->server->is_away);
	}

	/* is it the front-most tab? */
	if (gtk_tree_view_get_model (GTK_TREE_VIEW (sess->gui->user_tree))
		 == model)
	{
		if (sel)
			gtk_tree_selection_select_iter (gtk_tree_view_get_selection
										(GTK_TREE_VIEW (sess->gui->user_tree)), &iter);
	}
}

void
fe_userlist_clear (session *sess)
{
	if (sess->res->user_row_refs)
		g_hash_table_remove_all (sess->res->user_row_refs);
	gtk_list_store_clear (sess->res->user_model);
}

static void
userlist_dnd_drop (GtkTreeView *widget, GdkDragContext *context,
						 gint x, gint y, GtkSelectionData *selection_data,
						 guint info, guint ttime, gpointer userdata)
{
	struct User *user;
	gchar *data;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (!gtk_tree_view_get_path_at_pos (widget, x, y, &path, NULL, NULL, NULL))
		return;

	model = gtk_tree_view_get_model (widget);
	if (!gtk_tree_model_get_iter (model, &iter, path))
		return;
	gtk_tree_model_get (model, &iter, COL_USER, &user, -1);

	data = (char *)gtk_selection_data_get_data (selection_data);

	if (data)
		mg_dnd_drop_file (current_sess, user->nick, data);
}

static gboolean
userlist_dnd_motion (GtkTreeView *widget, GdkDragContext *context, gint x,
							gint y, guint ttime, gpointer tree)
{
	GtkTreePath *path;
	GtkTreeSelection *sel;

	if (!tree)
		return FALSE;

	if (gtk_tree_view_get_path_at_pos (widget, x, y, &path, NULL, NULL, NULL))
	{
		sel = gtk_tree_view_get_selection (widget);
		gtk_tree_selection_unselect_all (sel);
		gtk_tree_selection_select_path (sel, path);
	}

	return FALSE;
}

static gboolean
userlist_dnd_leave (GtkTreeView *widget, GdkDragContext *context, guint ttime)
{
	gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (widget));
	return TRUE;
}

static int
userlist_alpha_cmp (GtkTreeModel *model, GtkTreeIter *iter_a, GtkTreeIter *iter_b, gpointer userdata)
{
	struct User *user_a, *user_b;

	gtk_tree_model_get (model, iter_a, COL_USER, &user_a, -1);
	gtk_tree_model_get (model, iter_b, COL_USER, &user_b, -1);

	return nick_cmp_alpha (user_a, user_b, ((session*)userdata)->server);
}

static int
userlist_ops_cmp (GtkTreeModel *model, GtkTreeIter *iter_a, GtkTreeIter *iter_b, gpointer userdata)
{
	struct User *user_a, *user_b;

	gtk_tree_model_get (model, iter_a, COL_USER, &user_a, -1);
	gtk_tree_model_get (model, iter_b, COL_USER, &user_b, -1);

	return nick_cmp_az_ops (((session*)userdata)->server, user_a, user_b);
}

static void
userlist_store_color (GtkListStore *store, GtkTreeIter *iter, ThemeSemanticToken token, gboolean has_token)
{
	GdkRGBA rgba;
	const GdkRGBA *color = NULL;

	if (has_token && theme_get_color (token, &rgba))
		color = &rgba;

	if (color)
	{
		gtk_list_store_set (store, iter, COL_GDKCOLOR, color, -1);
	}
	else
	{
		gtk_list_store_set (store, iter, COL_GDKCOLOR, NULL, -1);
	}
}

GtkListStore *
userlist_create_model (session *sess)
{
	GtkListStore *store;
	GtkTreeIterCompareFunc cmp_func;
	GtkSortType sort_type;

	store = gtk_list_store_new (6, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,
										G_TYPE_STRING, G_TYPE_POINTER, THEME_GTK_COLOR_TYPE);

	switch (prefs.hex_gui_ulist_sort)
	{
	case 0:
		cmp_func = userlist_ops_cmp;
		sort_type = GTK_SORT_ASCENDING;
		break;
	case 1:
		cmp_func = userlist_alpha_cmp;
		sort_type = GTK_SORT_ASCENDING;
		break;
	case 2:
		cmp_func = userlist_ops_cmp;
		sort_type = GTK_SORT_DESCENDING;
		break;
	case 3:
		cmp_func = userlist_alpha_cmp;
		sort_type = GTK_SORT_DESCENDING;
		break;
	default:
		/* No sorting */
		gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE(store), NULL, NULL, NULL);
		return store;
	}

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE(store), cmp_func, sess, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(store),
						GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, sort_type);

	return store;
}

static void
userlist_add_columns (GtkTreeView * treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* icon column */
	renderer = gtk_cell_renderer_pixbuf_new ();
	if (prefs.hex_gui_compact)
		g_object_set (G_OBJECT (renderer), "ypad", 0, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
																-1, NULL, renderer,
																"pixbuf", COL_PIX, NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), 0);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);

	/* nick column */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	renderer = gtk_cell_renderer_text_new ();
	if (prefs.hex_gui_compact)
		g_object_set (G_OBJECT (renderer), "ypad", 0, NULL);
	gtk_cell_renderer_text_set_fixed_height_from_font (GTK_CELL_RENDERER_TEXT (renderer), 1);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer, "markup", COL_PREFIX);

	renderer = gtk_cell_renderer_text_new ();
	if (prefs.hex_gui_compact)
		g_object_set (G_OBJECT (renderer), "ypad", 0, NULL);
	g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_cell_renderer_text_set_fixed_height_from_font (GTK_CELL_RENDERER_TEXT (renderer), 1);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer, "markup", COL_NICK);
	gtk_tree_view_column_add_attribute (column, renderer, THEME_GTK_FOREGROUND_PROPERTY, COL_GDKCOLOR);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), 1);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_min_width (column, 1);
	gtk_tree_view_column_set_resizable (column, TRUE);
	userlist_apply_saved_column_width (column, prefs.hex_gui_ulist_nick_width);
	g_signal_connect (G_OBJECT (column), "notify::width",
							G_CALLBACK (userlist_column_width_notify_cb),
							&prefs.hex_gui_ulist_nick_width);

	if (prefs.hex_gui_ulist_show_hosts)
	{
		/* hostname column */
		renderer = gtk_cell_renderer_text_new ();
		if (prefs.hex_gui_compact)
			g_object_set (G_OBJECT (renderer), "ypad", 0, NULL);
		g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
		gtk_cell_renderer_text_set_fixed_height_from_font (GTK_CELL_RENDERER_TEXT (renderer), 1);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
																	-1, NULL, renderer,
																	"text", COL_HOST, NULL);
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), 2);
		gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
		gtk_tree_view_column_set_expand (column, TRUE);
		gtk_tree_view_column_set_min_width (column, 1);
		gtk_tree_view_column_set_resizable (column, TRUE);
		userlist_apply_saved_column_width (column, prefs.hex_gui_ulist_host_width);
		g_signal_connect (G_OBJECT (column), "notify::width",
								G_CALLBACK (userlist_column_width_notify_cb),
								&prefs.hex_gui_ulist_host_width);
	}
}

static gint
userlist_click_cb (GtkWidget *widget, GdkEventButton *event, gpointer userdata)
{
	char **nicks;
	int i;
	GtkTreeSelection *sel;
	GtkTreePath *path;

	if (!event)
		return FALSE;

	if (!(event->state & STATE_CTRL) &&
		event->type == GDK_2BUTTON_PRESS && prefs.hex_gui_ulist_doubleclick[0])
	{
		nicks = userlist_selection_list (widget, &i);
		if (nicks)
		{
			nick_command_parse (current_sess, prefs.hex_gui_ulist_doubleclick, nicks[0],
									  nicks[0]);
			while (i)
			{
				i--;
				g_free (nicks[i]);
			}
			g_free (nicks);
		}
		return TRUE;
	}

	if (event->button == 3)
	{
		/* do we have a multi-selection? */
		nicks = userlist_selection_list (widget, &i);
		if (nicks && i > 1)
		{
			menu_nickmenu (current_sess, event, nicks[0], i);
			while (i)
			{
				i--;
				g_free (nicks[i]);
			}
			g_free (nicks);
			return TRUE;
		}
		if (nicks)
		{
			g_free (nicks[0]);
			g_free (nicks);
		}

		sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
		if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
			 event->x, event->y, &path, 0, 0, 0))
		{
			gtk_tree_selection_unselect_all (sel);
			gtk_tree_selection_select_path (sel, path);
			gtk_tree_path_free (path);
			nicks = userlist_selection_list (widget, &i);
			if (nicks)
			{
				menu_nickmenu (current_sess, event, nicks[0], i);
				while (i)
				{
					i--;
					g_free (nicks[i]);
				}
				g_free (nicks);
			}
		} else
		{
			gtk_tree_selection_unselect_all (sel);
		}

		return TRUE;
	}

	return FALSE;
}

static gboolean
userlist_key_cb (GtkWidget *wid, GdkEventKey *evt, gpointer userdata)
{
	if (evt->keyval >= GDK_KEY_asterisk && evt->keyval <= GDK_KEY_z)
	{
		/* dirty trick to avoid auto-selection */
		SPELL_ENTRY_SET_EDITABLE (current_sess->gui->input_box, FALSE);
		gtk_widget_grab_focus (current_sess->gui->input_box);
		SPELL_ENTRY_SET_EDITABLE (current_sess->gui->input_box, TRUE);
		gtk_widget_event (current_sess->gui->input_box, (GdkEvent *)evt);
		return TRUE;
	}

	return FALSE;
}

GtkWidget *
userlist_create (GtkWidget *box)
{
	GtkWidget *sw, *treeview;
	static const GtkTargetEntry dnd_dest_targets[] =
	{
		{"text/uri-list", 0, 1},
		{"ZOITECHAT_CHANVIEW", GTK_TARGET_SAME_APP, 75 }
	};
	static const GtkTargetEntry dnd_src_target[] =
	{
		{"ZOITECHAT_USERLIST", GTK_TARGET_SAME_APP, 75 }
	};

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
													 GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
											  prefs.hex_gui_ulist_show_hosts ?
												GTK_POLICY_AUTOMATIC :
												GTK_POLICY_NEVER,
											  GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (box), sw, TRUE, TRUE, 0);
	gtk_widget_show (sw);

	treeview = gtk_tree_view_new ();
	gtk_widget_set_name (treeview, "zoitechat-userlist");
	gtk_widget_set_can_focus (treeview, TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection
										  (GTK_TREE_VIEW (treeview)),
										  GTK_SELECTION_MULTIPLE);

	/* set up drops */
	gtk_drag_dest_set (treeview, GTK_DEST_DEFAULT_ALL, dnd_dest_targets, 2,
							 GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
	gtk_drag_source_set (treeview, GDK_BUTTON1_MASK, dnd_src_target, 1, GDK_ACTION_MOVE);

	/* file DND (for DCC) */
	g_signal_connect (G_OBJECT (treeview), "drag-motion",
							G_CALLBACK (userlist_dnd_motion), treeview);
	g_signal_connect (G_OBJECT (treeview), "drag-leave",
							G_CALLBACK (userlist_dnd_leave), 0);
	g_signal_connect (G_OBJECT (treeview), "drag-data-received",
							G_CALLBACK (userlist_dnd_drop), treeview);

	g_signal_connect (G_OBJECT (treeview), "button-press-event",
							G_CALLBACK (userlist_click_cb), 0);
	g_signal_connect (G_OBJECT (treeview), "key-press-event",
							G_CALLBACK (userlist_key_cb), 0);

	/* tree/chanview DND */
	g_signal_connect (G_OBJECT (treeview), "drag-begin",
							G_CALLBACK (mg_drag_begin_cb), NULL);
	g_signal_connect (G_OBJECT (treeview), "drag-drop",
							G_CALLBACK (mg_drag_drop_cb), NULL);
	g_signal_connect (G_OBJECT (treeview), "drag-motion",
							G_CALLBACK (mg_drag_motion_cb), NULL);
	g_signal_connect (G_OBJECT (treeview), "drag-end",
							G_CALLBACK (mg_drag_end_cb), NULL);

	userlist_add_columns (GTK_TREE_VIEW (treeview));

	gtk_container_add (GTK_CONTAINER (sw), treeview);
	gtk_widget_show (treeview);

	return treeview;
}

void
userlist_show (session *sess)
{
	gtk_tree_view_set_model (GTK_TREE_VIEW (sess->gui->user_tree),
									 GTK_TREE_MODEL(sess->res->user_model));
}

void
fe_uselect (session *sess, char *word[], int do_clear, int scroll_to)
{
	char *name;
	int thisname;
	GtkTreeIter iter;
	GtkTreeView *treeview = GTK_TREE_VIEW (sess->gui->user_tree);
	GtkTreeModel *model = gtk_tree_view_get_model (treeview);
	GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
	struct User *user;
	struct User *row_user;

	if (do_clear)
		gtk_tree_selection_unselect_all (selection);

	thisname = 0;
	while (*(name = word[thisname++]))
	{
		user = userlist_find (sess, name);
		if (!user)
			continue;

		if (userlist_row_map_get_iter (sess, model, user, &iter))
		{
			gtk_tree_selection_select_iter (selection, &iter);
			if (scroll_to)
				scroll_to_iter (&iter, treeview, model);
			continue;
		}

		if (gtk_tree_model_get_iter_first (model, &iter))
		{
			do
			{
				gtk_tree_model_get (model, &iter, COL_USER, &row_user, -1);
				if (row_user == user)
				{
					userlist_row_map_set (sess, model, row_user, &iter);
					gtk_tree_selection_select_iter (selection, &iter);
					if (scroll_to)
						scroll_to_iter (&iter, treeview, model);
					break;
				}
			}
			while (gtk_tree_model_iter_next (model, &iter));
		}
	}
}
