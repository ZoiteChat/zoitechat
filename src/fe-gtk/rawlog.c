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
#include <fcntl.h>
#include <stdlib.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "fe-gtk.h"

#include <gdk/gdkkeysyms.h>

#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/server.h"
#include "gtkutil.h"
#include "theme/theme-access.h"
#include "theme/theme-manager.h"
#include "maingui.h"
#include "rawlog.h"
#include "xtext.h"
#include "fkeys.h"

#define ICON_RAWLOG_CLEAR "zc-menu-clear"
#define ICON_RAWLOG_SAVE_AS "zc-menu-save-as"

#define RAWLOG_THEME_LISTENER_ID_KEY "rawlog.theme-listener-id"

static void
rawlog_theme_apply (GtkWidget *window)
{
	GtkWidget *xtext_widget;
	XTextColor xtext_palette[XTEXT_COLS];

	if (!window)
		return;

	xtext_widget = g_object_get_data (G_OBJECT (window), "rawlog-xtext");
	if (!xtext_widget)
		return;

	theme_get_xtext_colors (xtext_palette, XTEXT_COLS);
	gtk_xtext_set_palette (GTK_XTEXT (xtext_widget), xtext_palette);
}

static void
rawlog_theme_changed (const ThemeChangedEvent *event, gpointer userdata)
{
	GtkWidget *window = userdata;

	if (!theme_changed_event_has_reason (event, THEME_CHANGED_REASON_PALETTE) &&
	    !theme_changed_event_has_reason (event, THEME_CHANGED_REASON_THEME_PACK) &&
	    !theme_changed_event_has_reason (event, THEME_CHANGED_REASON_MODE))
		return;

	rawlog_theme_apply (window);
}

static void
rawlog_theme_destroy_cb (GtkWidget *widget, gpointer userdata)
{
	guint listener_id;

	(void) userdata;

	listener_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), RAWLOG_THEME_LISTENER_ID_KEY));
	if (listener_id)
	{
		theme_listener_unregister (listener_id);
		g_object_set_data (G_OBJECT (widget), RAWLOG_THEME_LISTENER_ID_KEY, NULL);
	}
}

static void
close_rawlog (GtkWidget *wid, server *serv)
{
	if (is_server (serv))
		serv->gui->rawlog_window = 0;
}

static void
rawlog_save (server *serv, char *file)
{
	int fh = -1;

	if (file)
	{
		if (serv->gui->rawlog_window)
			fh = zoitechat_open_file (file, O_TRUNC | O_WRONLY | O_CREAT,
										 0600, XOF_DOMODE | XOF_FULLPATH);
		if (fh != -1)
		{
			gtk_xtext_save (GTK_XTEXT (serv->gui->rawlog_textlist), fh);
			close (fh);
		}
	}
}

static int
rawlog_clearbutton (GtkWidget * wid, server *serv)
{
	gtk_xtext_clear (GTK_XTEXT (serv->gui->rawlog_textlist)->buffer, 0);
	return FALSE;
}

static int
rawlog_savebutton (GtkWidget * wid, server *serv)
{
	gtkutil_file_req (NULL, _("Save As..."), rawlog_save, serv, NULL, NULL, FRF_WRITE);
	return FALSE;
}

static gboolean
rawlog_key_cb (GtkWidget * wid, GdkEventKey * key, gpointer userdata)
{
	/* Copy rawlog selection to clipboard when Ctrl+Shift+C is pressed,
	 * but make sure not to copy twice, i.e. when auto-copy is enabled.
	 */
	if (!prefs.hex_text_autocopy_text &&
		(key->keyval == GDK_KEY_c || key->keyval == GDK_KEY_C) &&
		key->state & STATE_SHIFT &&
		key->state & STATE_CTRL)
	{
		gtk_xtext_copy_selection (userdata);
	}
	return FALSE;
}

void
open_rawlog (struct server *serv)
{
	GtkWidget *bbox, *scrolledwindow, *vbox;
	XTextColor xtext_palette[XTEXT_COLS];
	char tbuf[256];

	if (serv->gui->rawlog_window)
	{
		mg_bring_tofront (serv->gui->rawlog_window);
		return;
	}

	g_snprintf (tbuf, sizeof tbuf, _("Raw Log (%s) - %s"), serv->servername, _(DISPLAY_NAME));
	serv->gui->rawlog_window =
		mg_create_generic_tab ("RawLog", tbuf, FALSE, TRUE, close_rawlog, serv,
							 640, 320, &vbox, serv);
	gtkutil_destroy_on_esc (serv->gui->rawlog_window);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_IN);
	gtk_widget_set_hexpand (scrolledwindow, TRUE);
	gtk_widget_set_vexpand (scrolledwindow, TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);

	theme_get_xtext_colors (xtext_palette, XTEXT_COLS);
	serv->gui->rawlog_textlist = gtk_xtext_new (xtext_palette, 0);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), serv->gui->rawlog_textlist);
	gtk_xtext_set_font (GTK_XTEXT (serv->gui->rawlog_textlist), prefs.hex_text_font);
	GTK_XTEXT (serv->gui->rawlog_textlist)->ignore_hidden = 1;
	g_object_set_data (G_OBJECT (serv->gui->rawlog_window), "rawlog-xtext", serv->gui->rawlog_textlist);

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_end (GTK_BOX (vbox), bbox, 0, 0, 4);

	gtkutil_button (bbox, ICON_RAWLOG_CLEAR, NULL, rawlog_clearbutton,
						 serv, _("Clear Raw Log"));

	gtkutil_button (bbox, ICON_RAWLOG_SAVE_AS, NULL, rawlog_savebutton,
						 serv, _("Save As..."));

	/* Copy selection to clipboard when Ctrl+Shift+C is pressed AND text auto-copy is disabled */
	g_signal_connect (G_OBJECT (serv->gui->rawlog_window), "key_press_event", G_CALLBACK (rawlog_key_cb), serv->gui->rawlog_textlist);
	g_object_set_data (G_OBJECT (serv->gui->rawlog_window), RAWLOG_THEME_LISTENER_ID_KEY,
				   GUINT_TO_POINTER (theme_listener_register ("rawlog.window", rawlog_theme_changed, serv->gui->rawlog_window)));
	g_signal_connect (G_OBJECT (serv->gui->rawlog_window), "destroy", G_CALLBACK (rawlog_theme_destroy_cb), NULL);

	gtk_widget_show_all (serv->gui->rawlog_window);
}

void
fe_add_rawlog (server *serv, char *text, int len, int outbound)
{
	char **split_text;
	char *new_text;
	size_t i;

	if (!serv->gui->rawlog_window)
		return;

	split_text = g_strsplit (text, "\r\n", 0);

	for (i = 0; i < g_strv_length (split_text); i++)
	{
		if (split_text[i][0] == 0)
			break;

		if (outbound)
			new_text = g_strconcat ("\0034<<\017 ", split_text[i], NULL);
		else
			new_text = g_strconcat ("\0033>>\017 ", split_text[i], NULL);

		gtk_xtext_append (GTK_XTEXT (serv->gui->rawlog_textlist)->buffer, new_text, strlen (new_text), 0);

		g_free (new_text);
	}

	g_strfreev (split_text);
}
