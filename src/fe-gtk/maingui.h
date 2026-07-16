/* ZoiteChat
 * Copyright (C) 1998-2010 Peter Zelezny.
 * Copyright (C) 2009-2013 Berke Viktor.
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

#ifndef ZOITECHAT_MAINGUI_H
#define ZOITECHAT_MAINGUI_H

extern InputStyle *input_style;
extern GtkWidget *parent_window;

void mg_changui_new (session *sess, restore_gui *res, int tab, int focus);
void mg_update_xtext (GtkWidget *wid);
void mg_open_quit_dialog (gboolean minimize_button);
void mg_switch_page (int relative, int num);
void mg_move_tab (session *, int delta);
void mg_move_tab_family (session *, int delta);
void mg_bring_tofront (GtkWidget *vbox);
void mg_bring_tofront_sess (session *sess);
void mg_decide_userlist (session *sess, gboolean switch_to_current);
void mg_set_topic_tip (session *sess);
GtkWidget *mg_create_generic_tab (char *name, char *title, int force_toplevel, int link_buttons, void *close_callback, void *userdata, int width, int height, GtkWidget **vbox_ret, void *family);
void mg_set_title (GtkWidget *button, char *title);
void mg_set_access_icon (session_gui *gui, GdkPixbuf *pix, gboolean away);
void mg_apply_setup (void);

/* which parts of the GUI need a live rebuild after a preferences change */
typedef struct
{
	unsigned int chanview:1;			/* rebuild the tab bar / channel tree */
	unsigned int tab_resort:1;		/* re-sort channel tabs */
	unsigned int tab_trunc:1;			/* re-truncate tab labels */
	unsigned int ulist_columns:1;	/* rebuild user list columns */
	unsigned int ulist_rows:1;		/* re-fill user list rows */
	unsigned int ulist_sort:1;		/* re-apply user list sort order */
	unsigned int ulist_count:1;		/* user count label visibility */
	unsigned int meters:1;				/* rebuild lag/throttle meters */
	unsigned int input_box:1;			/* nick box visibility and access icon */
	unsigned int topic_bar:1;			/* topic wrap mode and mode button placement */
	unsigned int transparency:1;	/* main window opacity */
} mg_live_prefs;

void mg_apply_live_prefs (const mg_live_prefs *changes);
void mg_apply_session_font_prefs (session_gui *gui);
void mg_close_sess (session *);
void mg_tab_close (session *sess);
void mg_reopen_closed_channel_tab (void);
void mg_detach (session *sess, int mode);
void mg_progressbar_create (session_gui *gui);
void mg_progressbar_destroy (session_gui *gui);
void mg_dnd_drop_file (session *sess, char *target, char *uri);
void mg_change_layout (int type);
void mg_update_meters (session_gui *);
void mg_inputbox_cb (GtkWidget *igad, session_gui *gui);
void mg_reply_update (session *sess);
void mg_create_icon_item (char *label, char *stock, GtkWidget *menu, void *callback, void *userdata);
GtkWidget *mg_submenu (GtkWidget *menu, char *text);
/* DND */
gboolean mg_drag_begin_cb (GtkWidget *widget, GdkDragContext *context, gpointer userdata);
void mg_drag_end_cb (GtkWidget *widget, GdkDragContext *context, gpointer userdata);
gboolean mg_drag_drop_cb (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time, gpointer user_data);
gboolean mg_drag_motion_cb (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time, gpointer user_data);
/* search */
void mg_search_toggle(session *sess);
void mg_search_handle_previous(GtkWidget *wid, session *sess);
void mg_search_handle_next(GtkWidget *wid, session *sess);

#endif
