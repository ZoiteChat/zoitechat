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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stddef.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define GLIB_DISABLE_DEPRECATION_WARNINGS
#include "fe-gtk.h"

#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/fe.h"
#include "../common/userlist.h"
#include "../common/outbound.h"
#include "../common/util.h"
#include "../common/text.h"
#include "../common/plugin.h"
#include "../common/typedef.h"
#include <gdk/gdkkeysyms.h>
#include "gtkutil.h"
#include "menu.h"
#include "xtext.h"
#include "theme/theme-access.h"
#include "theme/theme-manager.h"
#include "theme/theme-css.h"
#include "theme/theme-gtk3.h"
#include "maingui.h"
#include "textgui.h"
#include "fkeys.h"
#ifdef __APPLE__
#include "macos-shortcuts.h"
#endif
#include "setup.h"
#include "preferences-persistence.h"
#include "preferences-pages.h"

#define ICON_FKEYS_NEW "document-new"
#define ICON_FKEYS_DELETE "edit-delete"
#define ICON_FKEYS_CANCEL "dialog-cancel"
#define ICON_FKEYS_SAVE "document-save"
#define ICON_FKEYS_RESET "edit-undo"

static void replace_handle (GtkWidget * wid);
void key_check_replace_on_change (GtkEditable *editable, gpointer data);
void key_action_tab_clean (void);

/***************** Key Binding Code ******************/

/* NOTES:

   To add a new action:
   1) inc KEY_MAX_ACTIONS
   2) write the function at the bottom of this file (with all the others)
   FIXME: Write about calling and returning
   3) Add it to key_actions

   --AGL

 */

/* Remember that the *number* of actions is this *plus* 1 --AGL */
#define KEY_MAX_ACTIONS 16

struct key_binding
{
	guint keyval;					  /* keyval from gdk */
	GdkModifierType mod;			  /* Modifier, always ran through key_modifier_get_valid() */
	int action;						  /* Index into key_actions */
	char *data1, *data2;			  /* Pointers to strings, these must be freed */
};

struct key_action
{
	int (*handler) (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
						 struct session * sess);
	char *name;
	char *help;
};

struct gcomp_data
{
	char data[CHANLEN];
	int elen;
};

static int key_load_kbs (void);
static int key_load_kbs_from_buffer (char *ibuf, off_t size, GSList **out_list);

static void key_dialog_load (GtkListStore *store);
static void key_dialog_reset (GtkWidget *wid, gpointer userdata);
static int key_action_handle_command (GtkWidget * wid, GdkEventKey * evt,
												  char *d1, char *d2,
												  struct session *sess);
static int key_action_page_switch (GtkWidget * wid, GdkEventKey * evt,
											  char *d1, char *d2, struct session *sess);
int key_action_insert (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
							  struct session *sess);
static int key_action_scroll_page (GtkWidget * wid, GdkEventKey * evt,
											  char *d1, char *d2, struct session *sess);
static int key_action_set_buffer (GtkWidget * wid, GdkEventKey * evt,
											 char *d1, char *d2, struct session *sess);
static int key_action_history_up (GtkWidget * wid, GdkEventKey * evt,
											 char *d1, char *d2, struct session *sess);
static int key_action_history_down (GtkWidget * wid, GdkEventKey * evt,
												char *d1, char *d2, struct session *sess);
static int key_action_tab_comp (GtkWidget * wid, GdkEventKey * evt, char *d1,
										  char *d2, struct session *sess);
static int key_action_comp_chng (GtkWidget * wid, GdkEventKey * evt, char *d1,
                                                                                        char *d2, struct session *sess);
static int key_action_replace (GtkWidget * wid, GdkEventKey * evt, char *d1,
										 char *d2, struct session *sess);
static int key_action_move_tab_left (GtkWidget * wid, GdkEventKey * evt,
												 char *d1, char *d2,
												 struct session *sess);
static int key_action_move_tab_right (GtkWidget * wid, GdkEventKey * evt,
												  char *d1, char *d2,
												  struct session *sess);
static int key_action_move_tab_family_left (GtkWidget * wid, GdkEventKey * evt,
												 char *d1, char *d2,
												 struct session *sess);
static int key_action_move_tab_family_right (GtkWidget * wid, GdkEventKey * evt,
												  char *d1, char *d2,
												  struct session *sess);
static int key_action_put_history (GtkWidget * wid, GdkEventKey * evt,
												  char *d1, char *d2,
												  struct session *sess);
static int key_action_menu_shortcut (GtkWidget * wid, GdkEventKey * evt,
											 char *d1, char *d2,
											 struct session *sess);
static int key_action_reopen_closed_tab (GtkWidget * wid, GdkEventKey * evt,
												char *d1, char *d2,
												struct session *sess);


static GSList *keybind_list = NULL;

static gsize
key_action_decode_escapes (const char *input, char *output)
{
	gsize ii;
	gsize oi = 0;
	gsize len = strlen (input);

	for (ii = 0; ii < len; ii++)
	{
		if (input[ii] != '\\')
		{
			output[oi++] = input[ii];
			continue;
		}

		if (ii + 1 >= len)
		{
			output[oi++] = '\\';
			break;
		}

		ii++;

		switch (input[ii])
		{
		case 'n':
			output[oi++] = '\n';
			break;
		case 'r':
			output[oi++] = '\r';
			break;
		case 't':
			output[oi++] = '\t';
			break;
		case '\\':
			output[oi++] = '\\';
			break;
		case 'x':
			if (ii + 2 < len && g_ascii_isxdigit (input[ii + 1]) &&
				 g_ascii_isxdigit (input[ii + 2]))
			{
				output[oi++] =
					(g_ascii_xdigit_value (input[ii + 1]) << 4) |
					 g_ascii_xdigit_value (input[ii + 2]);
				ii += 2;
			}
			else
			{
				output[oi++] = '\\';
				output[oi++] = 'x';
			}
			break;
		default:
			output[oi++] = '\\';
			output[oi++] = input[ii];
			break;
		}
	}

	output[oi] = 0;
	return oi;
}

static const struct key_action key_actions[KEY_MAX_ACTIONS + 1] = {

	{key_action_handle_command, "Run Command",
	 N_("The \002Run Command\002 action runs the data in Data 1 as if it had been typed into the entry box where you pressed the key sequence. Thus it can contain text (which will be sent to the channel/person), commands or user commands. Escapes in Data 1 are interpreted (for example \002\\n\002, \002\\r\002, \002\\t\002, \002\\xNN\002 and \002\\\\\002), so it is possible to run more than one command by using newlines.")},
	{key_action_page_switch, "Change Page",
	 N_("The \002Change Page\002 command switches between pages in the notebook. Set Data 1 to the page number, channel name, network/channel, or auto. If Data 2 is set to anything then numeric switches are relative to the current position. Auto switches to the page with the most recent and important activity (queries first, then channels with hilight, channels with dialogue, channels with other data)")},
	{key_action_insert, "Insert in Buffer",
	 N_("The \002Insert in Buffer\002 command will insert the contents of Data 1 into the entry where the key sequence was pressed at the current cursor position. Escapes in Data 1 are interpreted (for example \002\\n\002, \002\\r\002, \002\\t\002, \002\\xNN\002 and \002\\\\\002).")},
	{key_action_scroll_page, "Scroll Page",
	 N_("The \002Scroll Page\002 command scrolls the text widget up or down one page or one line. Set Data 1 to either Top, Bottom, Up, Down, +1 or -1.")},
	{key_action_set_buffer, "Set Buffer",
	 N_("The \002Set Buffer\002 command sets the entry where the key sequence was entered to the contents of Data 1")},
	{key_action_history_up, "Last Command",
	 N_("The \002Last Command\002 command sets the entry to contain the last command entered - the same as pressing up in a shell")},
	{key_action_history_down, "Next Command",
	 N_("The \002Next Command\002 command sets the entry to contain the next command entered - the same as pressing down in a shell")},
	{key_action_tab_comp, "Complete nick/command",
	 N_("This command changes the text in the entry to finish an incomplete nickname or command. If Data 1 is set then double-tabbing in a string will select the last nick, not the next")},
	{key_action_comp_chng, "Change Selected Nick",
	 N_("This command scrolls up and down through the list of nicks. If Data 1 is set to anything it will scroll up, else it scrolls down")},
	{key_action_replace, "Check For Replace",
	 N_("This command checks the last word entered in the entry against the replace list and replaces it if it finds a match")},
	{key_action_move_tab_left, "Move front tab left",
	 N_("This command moves the front tab left by one")},
	{key_action_move_tab_right, "Move front tab right",
	 N_("This command moves the front tab right by one")},
	{key_action_move_tab_family_left, "Move tab family left",
	 N_("This command moves the current tab family to the left")},
	{key_action_move_tab_family_right, "Move tab family right",
	 N_("This command moves the current tab family to the right")},
	{key_action_put_history, "Push input line into history",
	 N_("Push input line into history but doesn't send to server")},
	{key_action_menu_shortcut, "Menu Shortcut",
	 N_("Runs one of the built-in menu shortcuts. Set Data 1 to: network-list, new-server-tab, new-server-window, close, quit, menu-toggle, user-list-toggle, fullscreen-toggle, away-toggle, reset-marker, move-marker, copy-selection, search-text, search-next, search-previous or contents.")},
	{key_action_reopen_closed_tab, "Reopen Closed Tab",
	 N_("Reopens the most recently closed channel tab")},
};

#define default_kb_cfg \
	"ACCEL=<Primary>Page_Up\nChange Page\nD1:-1\nD2:Relative\n\n"\
	"ACCEL=<Primary>Page_Down\nChange Page\nD1:1\nD2:Relative\n\n"\
	"ACCEL=<Alt>9\nChange Page\nD1:9\nD2!\n\n"\
	"ACCEL=<Alt>8\nChange Page\nD1:8\nD2!\n\n"\
	"ACCEL=<Alt>7\nChange Page\nD1:7\nD2!\n\n"\
	"ACCEL=<Alt>6\nChange Page\nD1:6\nD2!\n\n"\
	"ACCEL=<Alt>5\nChange Page\nD1:5\nD2!\n\n"\
	"ACCEL=<Alt>4\nChange Page\nD1:4\nD2!\n\n"\
	"ACCEL=<Alt>3\nChange Page\nD1:3\nD2!\n\n"\
	"ACCEL=<Alt>2\nChange Page\nD1:2\nD2!\n\n"\
	"ACCEL=<Alt>1\nChange Page\nD1:1\nD2!\n\n"\
	"ACCEL=<Alt>grave\nChange Page\nD1:auto\nD2!\n\n"\
	"ACCEL=<Primary>o\nInsert in Buffer\nD1:\017\nD2!\n\n"\
	"ACCEL=<Primary>b\nInsert in Buffer\nD1:\002\nD2!\n\n"\
	"ACCEL=<Primary>k\nInsert in Buffer\nD1:\003\nD2!\n\n"\
	"ACCEL=<Primary>i\nInsert in Buffer\nD1:\035\nD2!\n\n"\
	"ACCEL=<Primary>u\nInsert in Buffer\nD1:\037\nD2!\n\n"\
	"ACCEL=<Primary><Alt>s\nInsert in Buffer\nD1:\036\nD2!\n\n"\
	"ACCEL=<Primary>r\nInsert in Buffer\nD1:\026\nD2!\n\n"\
	"ACCEL=<Shift>Page_Down\nChange Selected Nick\nD1!\nD2!\n\n"\
	"ACCEL=<Shift>Page_Up\nChange Selected Nick\nD1:Up\nD2!\n\n"\
	"ACCEL=Page_Down\nScroll Page\nD1:Down\nD2!\n\n"\
	"ACCEL=<Primary>Home\nScroll Page\nD1:Top\nD2!\n\n"\
	"ACCEL=<Primary>End\nScroll Page\nD1:Bottom\nD2!\n\n"\
	"ACCEL=Page_Up\nScroll Page\nD1:Up\nD2!\n\n"\
	"ACCEL=<Shift>Down\nScroll Page\nD1:+1\nD2!\n\n"\
	"ACCEL=<Shift>Up\nScroll Page\nD1:-1\nD2!\n\n"\
	"ACCEL=Down\nNext Command\nD1!\nD2!\n\n"\
	"ACCEL=Up\nLast Command\nD1!\nD2!\n\n"\
	"ACCEL=Tab\nComplete nick/command\nD1!\nD2!\n\n"\
	"ACCEL=<Shift>ISO_Left_Tab\nComplete nick/command\nD1:Previous\nD2!\n\n"\
	"ACCEL=space\nCheck For Replace\nD1!\nD2!\n\n"\
	"ACCEL=Return\nCheck For Replace\nD1!\nD2!\n\n"\
	"ACCEL=KP_Enter\nCheck For Replace\nD1!\nD2!\n\n"\
	"ACCEL=<Primary>Tab\nComplete nick/command\nD1:Up\nD2!\n\n"\
	"ACCEL=<Alt>Left\nMove front tab left\nD1!\nD2!\n\n"\
	"ACCEL=<Alt>Right\nMove front tab right\nD1!\nD2!\n\n"\
	"ACCEL=<Primary><Shift>Page_Up\nMove tab family left\nD1!\nD2!\n\n"\
	"ACCEL=<Primary><Shift>Page_Down\nMove tab family right\nD1!\nD2!\n\n"\
	"ACCEL=<Primary>s\nMenu Shortcut\nD1:network-list\nD2!\n\n"\
	"ACCEL=<Primary>t\nMenu Shortcut\nD1:new-server-tab\nD2!\n\n"\
	"ACCEL=<Primary>n\nMenu Shortcut\nD1:new-server-window\nD2!\n\n"\
	"ACCEL=<Primary>w\nMenu Shortcut\nD1:close\nD2!\n\n"\
	"ACCEL=<Primary>q\nMenu Shortcut\nD1:quit\nD2!\n\n"\
	"ACCEL=<Primary>F9\nMenu Shortcut\nD1:menu-toggle\nD2!\n\n"\
	"ACCEL=F7\nMenu Shortcut\nD1:user-list-toggle\nD2!\n\n"\
	"ACCEL=F11\nMenu Shortcut\nD1:fullscreen-toggle\nD2!\n\n"\
	"ACCEL=<Alt>a\nMenu Shortcut\nD1:away-toggle\nD2!\n\n"\
	"ACCEL=<Primary>m\nMenu Shortcut\nD1:reset-marker\nD2!\n\n"\
	"ACCEL=<Primary><Shift>M\nMenu Shortcut\nD1:move-marker\nD2!\n\n"\
	"ACCEL=<Primary><Shift>C\nMenu Shortcut\nD1:copy-selection\nD2!\n\n"\
	"ACCEL=<Primary>f\nMenu Shortcut\nD1:search-text\nD2!\n\n"\
	"ACCEL=<Primary>g\nMenu Shortcut\nD1:search-next\nD2!\n\n"\
	"ACCEL=<Primary><Shift>G\nMenu Shortcut\nD1:search-previous\nD2!\n\n"\
	"ACCEL=F1\nMenu Shortcut\nD1:contents\nD2!\n\n"\
	"ACCEL=<Primary><Shift>T\nReopen Closed Tab\nD1!\nD2!\n\n"

void
key_init (void)
{
	if (key_load_kbs () == 1)
	{
		fe_message (_("There was an error loading key"
							" bindings configuration"), FE_MSG_ERROR);
	}
}

static inline int
key_get_action_from_string (char *text)
{
	int i;

	for (i = 0; i < KEY_MAX_ACTIONS + 1; i++)
	{
		if (strcmp (key_actions[i].name, text) == 0)
		{
			return i;
		}
	}

	return 0;
}

static void
key_free (gpointer data)
{
	struct key_binding *kb = (struct key_binding*)data;

	g_return_if_fail (kb != NULL);

	g_free (kb->data1);
	g_free (kb->data2);
	g_free (kb);
}

/* Ok, here are the NOTES

   key_handle_key_press now handles all the key presses and history_keypress is
   now defunct. It goes thru the linked list keys_root and finds a matching
   key. It runs the action func and switches on these values:
   0) Return
   1) Find next
   2) stop signal and return

   * history_keypress is now dead (and gone)
   * key_handle_key_press now takes its role
   * All the possible actions are in a struct called key_actions (in fkeys.c)
   * it is made of {function, name, desc}
   * key bindings can pass 2 *text* strings to the handler. If more options are nee
   ded a format can be put on one of these options
   * key actions are passed {
   the entry widget
   the Gdk event
   data 1
   data 2
   session struct
   }
   * key bindings are stored in a linked list of key_binding structs
   * which looks like {
   guint keyval;  GDK keynumber
   int action;  Index into key_actions 
   GdkModiferType mod; modifier, only ones from key_modifer_get_valid()
   char *data1, *data2;  Pointers to strings, these must be freed 
   struct key_binding *next;
   }
   * remember that is (data1 || data2) != NULL then they need to be free()'ed

   --AGL

 */

static inline GdkModifierType
key_modifier_get_valid (GdkModifierType mod)
{
	GdkModifierType ret;

#ifdef __APPLE__
	ret = mod & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_META_MASK);
#else
	/* These masks work on both Windows and Unix */
	ret = mod & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK);
#endif

	return ret;
}

gboolean
key_handle_key_press (GtkWidget *wid, GdkEventKey *evt, session *sess)
{
	struct key_binding *kb;
	int n;
	GSList *list;

	/* where did this event come from? */
	list = sess_list;
	while (list)
	{
		sess = list->data;
		if (sess->gui->input_box == wid)
		{
			if (sess->gui->is_tab)
				sess = current_tab;
			break;
		}
		list = list->next;
	}
	if (!list)
		return FALSE;
	current_sess = sess;
	if (plugin_emit_keypress (sess, evt->state, evt->keyval, gdk_keyval_to_unicode (evt->keyval)))
		return 1;

	/* maybe the plugin closed this tab? */
	if (!is_session (sess))
		return 1;

	list = keybind_list;
	while (list)
	{
		kb = (struct key_binding*)list->data;

		if (kb->keyval == evt->keyval && kb->mod == key_modifier_get_valid (evt->state))
		{
			if (kb->action < 0 || kb->action > KEY_MAX_ACTIONS)
				return 0;

			/* Run the function */
			n = key_actions[kb->action].handler (wid, evt, kb->data1,
															 kb->data2, sess);
			switch (n)
			{
			case 0:
				return 1;
			case 2:
				g_signal_stop_emission_by_name (G_OBJECT (wid),
														"key-press-event");
				return 1;
			}
		}
		list = g_slist_next (list);
	}

#ifdef __APPLE__
	/* Plugins and user-defined input shortcuts above retain priority. */
	if (macos_shortcuts_copy_chat (wid, evt, GTK_XTEXT (sess->gui->xtext)))
		return TRUE;
#endif

	switch (evt->keyval)
	{
	case GDK_KEY_space:
		key_action_tab_clean ();
		break;
	}

	return 0;
}

gboolean
key_get_menu_accel (const char *name, guint *keyval, GdkModifierType *mod)
{
	struct key_binding *kb;
	GSList *list;

	if (!name)
		return FALSE;

	list = keybind_list;
	while (list)
	{
		kb = (struct key_binding*)list->data;
		if (kb->action >= 0 && kb->action <= KEY_MAX_ACTIONS && kb->keyval != 0 && !strcmp (key_actions[kb->action].name, "Menu Shortcut") && kb->data1 && !strcmp (kb->data1, name))
		{
			*keyval = kb->keyval;
			*mod = kb->mod;
			return TRUE;
		}
		list = g_slist_next (list);
	}

	return FALSE;
}


/* ***** GUI code here ******************* */

enum
{
        KEY_COLUMN = PREFERENCES_SHORTCUT_LABEL,
        ACCEL_COLUMN = PREFERENCES_SHORTCUT_ACCEL,
        ACTION_COLUMN = PREFERENCES_SHORTCUT_ACTION,
        D1_COLUMN = PREFERENCES_SHORTCUT_DATA1,
        D2_COLUMN = PREFERENCES_SHORTCUT_DATA2,
        CUSTOM_COLUMN = PREFERENCES_SHORTCUT_CUSTOM,
        N_COLUMNS = PREFERENCES_SHORTCUT_N_COLUMNS
};

static GtkWidget *key_dialog = NULL;

static inline GtkTreeModel *
get_store (void)
{
        return g_object_get_data (G_OBJECT (key_dialog), "bindings-store");
}

static char *
key_binding_signature (const char *action, const char *data1, const char *data2)
{
        const char *a = action ? action : "", *b = data1 ? data1 : "", *c = data2 ? data2 : "";
        return g_strdup_printf ("%" G_GSIZE_FORMAT ":%s%" G_GSIZE_FORMAT ":%s%" G_GSIZE_FORMAT ":%s",
                strlen (a), a, strlen (b), b, strlen (c), c);
}

static int
key_dialog_reset_count (GHashTable *table, const char *key)
{
	return GPOINTER_TO_INT (g_hash_table_lookup (table, key));
}

static void
key_dialog_reset_increment (GHashTable *table, char *key)
{
	g_hash_table_replace (table, key, GINT_TO_POINTER (key_dialog_reset_count (table, key) + 1));
}

static void
key_dialog_reset (GtkWidget *wid, gpointer userdata)
{
	GtkListStore *store = GTK_LIST_STORE (get_store ());
	GtkListStore *custom_store;
	GtkTreeIter iter, custom_iter;
	GtkWidget *delete_button;
	GHashTable *default_counts, *seen_counts;
	GSList *list = NULL, *old_list, *default_iter;
	struct key_binding *kb;
	gboolean custom, keep;
	char *key, *accel, *action, *data1, *data2, *signature;

	if (key_load_kbs_from_buffer (g_strdup (default_kb_cfg), strlen (default_kb_cfg), &list) != 0)
		return;

	default_counts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	seen_counts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (default_iter = list; default_iter; default_iter = g_slist_next (default_iter))
	{
		kb = default_iter->data;
		signature = key_binding_signature (key_actions[kb->action].name, kb->data1, kb->data2);
		key_dialog_reset_increment (default_counts, signature);
	}

	custom_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
								G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
	{
		do
		{
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
								KEY_COLUMN, &key,
								ACCEL_COLUMN, &accel,
								ACTION_COLUMN, &action,
								D1_COLUMN, &data1,
								D2_COLUMN, &data2,
								CUSTOM_COLUMN, &custom,
								-1);
			signature = key_binding_signature (action, data1, data2);
			keep = custom || key_dialog_reset_count (seen_counts, signature) >= key_dialog_reset_count (default_counts, signature);
			if (!custom)
				key_dialog_reset_increment (seen_counts, g_strdup (signature));
			if (keep)
			{
				gtk_list_store_append (custom_store, &custom_iter);
				gtk_list_store_set (custom_store, &custom_iter,
								KEY_COLUMN, key,
								ACCEL_COLUMN, accel,
								ACTION_COLUMN, action,
								D1_COLUMN, data1,
								D2_COLUMN, data2,
								CUSTOM_COLUMN, TRUE,
								-1);
			}
			g_free (signature);
			g_free (key);
			g_free (accel);
			g_free (action);
			g_free (data1);
			g_free (data2);
		}
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
	}

	old_list = keybind_list;
	keybind_list = list;
	gtk_list_store_clear (store);
	key_dialog_load (store);
	keybind_list = old_list;

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (custom_store), &iter))
	{
		do
		{
			gtk_tree_model_get (GTK_TREE_MODEL (custom_store), &iter,
								KEY_COLUMN, &key,
								ACCEL_COLUMN, &accel,
								ACTION_COLUMN, &action,
								D1_COLUMN, &data1,
								D2_COLUMN, &data2,
								-1);
			gtk_list_store_append (store, &custom_iter);
			gtk_list_store_set (store, &custom_iter,
							KEY_COLUMN, key,
							ACCEL_COLUMN, accel,
							ACTION_COLUMN, action,
							D1_COLUMN, data1,
							D2_COLUMN, data2,
							CUSTOM_COLUMN, TRUE,
							-1);
			g_free (key);
			g_free (accel);
			g_free (action);
			g_free (data1);
			g_free (data2);
		}
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL (custom_store), &iter));
	}

	delete_button = g_object_get_data (G_OBJECT (key_dialog), "delete_button");
	if (delete_button)
		gtk_widget_set_sensitive (delete_button, FALSE);
	g_hash_table_destroy (default_counts);
	g_hash_table_destroy (seen_counts);
	g_object_unref (custom_store);
	g_slist_free_full (list, key_free);
}

static void
key_dialog_load (GtkListStore *store)
{
        GHashTable *defaults = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        GSList *reference = NULL;
        key_load_kbs_from_buffer (g_strdup (default_kb_cfg), strlen (default_kb_cfg), &reference);
        for (GSList *item = reference; item; item = item->next)
        {
                struct key_binding *kb = item->data;
                key_dialog_reset_increment (defaults, key_binding_signature (key_actions[kb->action].name, kb->data1, kb->data2));
        }
        /* Protect only the built-in number of occurrences of an action and its
         * parameters. Extra copies remain custom after save/reload, including
         * copies whose only difference is their key. No runtime order changes. */
        for (GSList *item = keybind_list; item; item = item->next)
        {
                struct key_binding *kb = item->data;
                GtkTreeIter iter;
                char *signature = key_binding_signature (key_actions[kb->action].name, kb->data1, kb->data2);
                gboolean custom = key_dialog_reset_count (seen, signature) >= key_dialog_reset_count (defaults, signature);
                key_dialog_reset_increment (seen, signature);
                char *label = gtk_accelerator_get_label (kb->keyval, kb->mod);
                char *accel = gtk_accelerator_name (kb->keyval, kb->mod);
                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, KEY_COLUMN, label, ACCEL_COLUMN, accel,
                        ACTION_COLUMN, key_actions[kb->action].name, D1_COLUMN, kb->data1,
                        D2_COLUMN, kb->data2, CUSTOM_COLUMN, custom, -1);
                g_free (label); g_free (accel);
        }
        g_slist_free_full (reference, key_free);
        g_hash_table_destroy (defaults); g_hash_table_destroy (seen);
}

static char *key_preferences_serialize (GtkTreeModel *model, GError **error);

static void
key_preferences_restore (gpointer unused)
{
        key_dialog_reset (NULL, NULL);
}

GtkWidget *
key_preferences_create (void)
{
        GtkListStore *store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING,
                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
        PreferencesShortcutAction actions[KEY_MAX_ACTIONS + 1];
        for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
        {
                actions[i].id = key_actions[i].name;
                actions[i].help = key_actions[i].help;
        }
        key_dialog_load (store);
        key_dialog = preferences_shortcuts_page_new (store, actions, G_N_ELEMENTS (actions),
                key_modifier_get_valid, key_preferences_restore, NULL);
        g_object_add_weak_pointer (G_OBJECT (key_dialog), (gpointer *)&key_dialog);
        g_object_set_data_full (G_OBJECT (key_dialog), "original-bindings",
                key_preferences_serialize (GTK_TREE_MODEL (store), NULL), g_free);
        g_object_unref (store);
        return key_dialog;
}

void
key_dialog_show (void)
{
	setup_open_page ("keyboard");
}

static char *
key_preferences_serialize (GtkTreeModel *model, GError **error)
{
	GtkTreeIter iter;
	GString *text = g_string_new ("# ZoiteChat key bindings config file\n\n");
	gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid)
	{
		char *accel, *action, *data1, *data2;
		guint key;
		GdkModifierType modifiers;
		gtk_tree_model_get (model, &iter, ACCEL_COLUMN, &accel, ACTION_COLUMN, &action,
			D1_COLUMN, &data1, D2_COLUMN, &data2, -1);
		if ((!accel || !*accel) && (!action || !*action) && (!data1 || !*data1) && (!data2 || !*data2))
			goto next;
		gtk_accelerator_parse (accel ? accel : "", &key, &modifiers);
		if (!key || !action || strcmp (key_actions[key_get_action_from_string (action)].name, action) != 0 ||
			(data1 && strpbrk (data1, "\r\n")) || (data2 && strpbrk (data2, "\r\n")))
		{
			GtkTreeView *view = g_object_get_data (G_OBJECT (key_dialog), "view");
			GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
			setup_reveal_widget (GTK_WIDGET (view));
                        preferences_shortcuts_reveal_path (key_dialog, path);
                        gtk_widget_grab_focus (GTK_WIDGET (view));
			gtk_tree_path_free (path);
			g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
				_("Complete each keyboard shortcut with a key and an action. Data fields must stay on one line."));
			g_free (accel); g_free (action); g_free (data1); g_free (data2);
			g_string_free (text, TRUE);
			return NULL;
		}
		g_string_append_printf (text, "ACCEL=%s\n%s\n", accel, action);
		if (data1 && *data1) g_string_append_printf (text, "D1:%s\n", data1);
		else g_string_append (text, "D1!\n");
		if (data2 && *data2) g_string_append_printf (text, "D2:%s\n", data2);
		else g_string_append (text, "D2!\n");
		g_string_append_c (text, '\n');
 next:
		g_free (accel); g_free (action); g_free (data1); g_free (data2);
		valid = gtk_tree_model_iter_next (model, &iter);
	}
	return g_string_free (text, FALSE);
}

gboolean
key_preferences_stage (GError **error)
{
	char *contents;
	const char *original;
	if (!key_dialog) return TRUE;
	contents = key_preferences_serialize (get_store (), error);
	if (!contents) return FALSE;
	original = g_object_get_data (G_OBJECT (key_dialog), "original-bindings");
	g_object_set_data_full (G_OBJECT (key_dialog), "pending-bindings", NULL, g_free);
	if (g_strcmp0 (contents, original) != 0)
	{
		preferences_persistence_stage_file ("keybindings.conf", contents);
		g_object_set_data_full (G_OBJECT (key_dialog), "pending-bindings", g_strdup (contents), g_free);
	}
	g_free (contents);
	return TRUE;
}

void
key_preferences_commit (void)
{
	GSList *bindings = NULL;
	const char *contents;
	if (!key_dialog) return;
	contents = g_object_get_data (G_OBJECT (key_dialog), "pending-bindings");
	if (!contents) return;
	if (key_load_kbs_from_buffer (g_strdup (contents), strlen (contents), &bindings) == 0)
	{
		g_slist_free_full (keybind_list, key_free);
		keybind_list = bindings;
		menu_update_quit_accel ();
	}
}

#define KBSTATE_MOD 0
#define KBSTATE_KEY 1
#define KBSTATE_ACT 2
#define KBSTATE_DT1 3
#define KBSTATE_DT2 4

#ifdef __APPLE__
static void
key_macos_adjust_defaults (GSList *bindings)
{
	GSList *item, *other;
	struct key_binding *kb, *existing;
	guint keyval;
	GdkModifierType mod;

	for (item = bindings; item; item = item->next)
	{
		kb = item->data;
		keyval = kb->keyval;
		mod = kb->mod;
		if (!macos_shortcuts_remap_default (&keyval, &mod,
		                                   key_actions[kb->action].name,
		                                   kb->data1, kb->data2))
			continue;

		/* Never let a migrated default shadow an existing user shortcut.
		 * In that case leave the old assignment for the user to resolve. */
		for (other = bindings; other; other = other->next)
		{
			existing = other->data;
			if (existing != kb &&
			    gdk_keyval_to_lower (existing->keyval) == gdk_keyval_to_lower (keyval) &&
			    existing->mod == mod)
				break;
		}
		if (!other)
		{
			kb->keyval = keyval;
			kb->mod = mod;
		}
	}
}

gboolean
key_is_bound (guint keyval, GdkModifierType mod)
{
	GSList *item;
	for (item = keybind_list; item; item = item->next)
	{
		struct key_binding *kb = item->data;
		if (kb->keyval == keyval && kb->mod == mod)
			return TRUE;
	}
	return FALSE;
}
#endif

#define STRIP_WHITESPACE \
	while (buf[0] == ' ' || buf[0] == '\t') \
		buf++; \
	len = strlen (buf); \
	while (buf[len] == ' ' || buf[len] == '\t') \
	{ \
		buf[len] = 0; \
		len--; \
	} \

static inline int
key_load_kbs_helper_mod (char *buf, GdkModifierType *out)
{
	int n, len, mod = 0;

	/* First strip off the fluff */
	STRIP_WHITESPACE

	if (strcmp (buf, "None") == 0)
	{
		*out = 0;
		return 0;
	}
	for (n = 0; n < len; n++)
	{
		switch (buf[n])
		{
		case 'C':
			mod |= GDK_CONTROL_MASK;
			break;
		case 'A':
			mod |= GDK_MOD1_MASK;
			break;
		case 'S':
			mod |= GDK_SHIFT_MASK;
			break;
		default:
			return 1;
		}
	}

	*out = mod;
	return 0;
}

static int
key_load_kbs_from_buffer (char *ibuf, off_t size, GSList **out_list)
{
	char *buf;
	struct key_binding *kb = NULL;
	int len, state = 0, pnt = 0;
	guint keyval;
	GdkModifierType mod = 0;
	GSList *list = NULL;

	while (buf_get_line (ibuf, &buf, &pnt, size))
	{		
		if (buf[0] == '#')
			continue;
		if (strlen (buf) == 0)
			continue;

		switch (state)
		{
		case KBSTATE_MOD:
			kb = g_new0 (struct key_binding, 1);

			if (strncmp (buf, "ACCEL=", 6) == 0)
			{
				buf += 6;

				gtk_accelerator_parse (buf, &keyval, &mod);

				kb->keyval = keyval;
				kb->mod = key_modifier_get_valid (mod);

				state = KBSTATE_ACT; 
				continue;
			}

			if (key_load_kbs_helper_mod (buf, &mod))
				goto corrupt_file;

			kb->mod = mod;

			state = KBSTATE_KEY;
			continue;

		case KBSTATE_KEY:
			STRIP_WHITESPACE

			keyval = gdk_keyval_from_name (buf);
			if (keyval == 0)
			{
				g_free (ibuf);
				key_free (kb);
				g_slist_free_full (list, key_free);
				return 2;
			}

			kb->keyval = keyval;

			state = KBSTATE_ACT;
			continue;

		case KBSTATE_ACT:
			STRIP_WHITESPACE

			kb->action = key_get_action_from_string (buf);

			if (kb->action == KEY_MAX_ACTIONS + 1)
			{
				g_free (ibuf);
				key_free (kb);
				g_slist_free_full (list, key_free);
				return 3;
			}

			state = KBSTATE_DT1;
			continue;

		case KBSTATE_DT1:
		case KBSTATE_DT2:
			if (state == KBSTATE_DT1)
				kb->data1 = kb->data2 = NULL;

			while (buf[0] == ' ' || buf[0] == '\t')
				buf++;

			if (buf[0] != 'D')
			{
				g_free (ibuf);
				key_free (kb);
				g_slist_free_full (list, key_free);
				return 4;
			}

			switch (buf[1])
			{
			case '1':
				if (state != KBSTATE_DT1)
					goto corrupt_file;
				break;
			case '2':
				if (state != KBSTATE_DT2)
					goto corrupt_file;
				break;
			default:
				goto corrupt_file;
			}

			if (buf[2] == ':')
			{
				len = strlen (buf);
				len++;
				len -= 3;
				if (state == KBSTATE_DT1)
				{
					kb->data1 = g_strndup (&buf[3], len);
				} else
				{
					kb->data2 = g_strndup (&buf[3], len);
				}
			} else if (buf[2] == '!')
			{
				if (state == KBSTATE_DT1)
					kb->data1 = NULL;
				else
					kb->data2 = NULL;
			}
			if (state == KBSTATE_DT1)
			{
				state = KBSTATE_DT2;
				continue;
			} else
			{
				list = g_slist_append (list, kb);
				kb = NULL;

				state = KBSTATE_MOD;
			}

			continue;
		}
	}
	g_free (ibuf);
#ifdef __APPLE__
	key_macos_adjust_defaults (list);
#endif
	*out_list = list;
	return 0;

corrupt_file:
	g_free (ibuf);
	key_free (kb);
	g_slist_free_full (list, key_free);
	return 5;
}

static int
key_load_kbs (void)
{
	char *ibuf;
	struct stat st;
	int fd, result;
	off_t size;
	GSList *list = NULL;

	fd = zoitechat_open_file ("keybindings.conf", O_RDONLY, 0, 0);
	if (fd < 0)
	{
		ibuf = g_strdup (default_kb_cfg);
		size = strlen (default_kb_cfg);
	}
	else
	{
		if (fstat (fd, &st) != 0)
		{
			close (fd);
			return 1;
		}

		ibuf = g_malloc(st.st_size);
		read (fd, ibuf, st.st_size);
		size = st.st_size;
		close (fd);
	}

	result = key_load_kbs_from_buffer (ibuf, size, &list);
	if (result != 0)
		return result;

	if (keybind_list)
		g_slist_free_full (keybind_list, key_free);
	keybind_list = list;

	return 0;
}

static int
key_action_handle_command (GtkWidget * wid, GdkEventKey * evt, char *d1,
									char *d2, struct session *sess)
{
	char *out;

	if (!d1)
		return 0;

	out = g_malloc (strlen (d1) + 1);
	key_action_decode_escapes (d1, out);

	handle_multiline (sess, out, 0, 0);
	g_free (out);
	return 0;
}

static int
key_action_menu_shortcut (GtkWidget * wid, GdkEventKey * evt, char *d1,
									 char *d2, struct session *sess)
{
	if (menu_key_action (d1, evt->keyval, evt->state))
		return 2;

	return 0;
}

static int
key_action_reopen_closed_tab (GtkWidget * wid, GdkEventKey * evt, char *d1,
										 char *d2, struct session *sess)
{
	mg_reopen_closed_channel_tab ();
	return 2;
}

/*
 * Check if the given session is inside the main window. This predicate
 * is passed to lastact_getfirst() as a way to filter out detached sessions.
 */
static int
session_check_is_tab(session *sess)
{
	if (!sess || !sess->gui)
		return FALSE;

	return (sess->gui->is_tab);
}

static int
key_action_page_switch (GtkWidget * wid, GdkEventKey * evt, char *d1,
								char *d2, struct session *sess)
{
	session *newsess;
	char *network;
	char *channel;
	char *slash;
	int len, i, num;

	if (!d1)
		return 1;

	len = strlen (d1);
	if (!len)
		return 1;

	if (g_ascii_strcasecmp(d1, "auto") == 0)
	{
		if (!sess->gui->is_tab)
			return 1;

		newsess = lastact_getfirst(session_check_is_tab);

		if (newsess)
		{
			mg_bring_tofront_sess(newsess);
			return 0;
		}
		else
			return 1;
	}

	for (i = 0; i < len; i++)
	{
		if (d1[i] < '0' || d1[i] > '9')
		{
			if (i == 0 && (d1[i] == '+' || d1[i] == '-'))
				continue;

			network = NULL;
			channel = d1;
			slash = strchr (d1, '/');

			if (slash && slash[1])
			{
				network = g_strndup (d1, slash - d1);
				channel = slash + 1;
			}

			newsess = plugin_find_context (network, channel, sess->server);
			g_free (network);

			if (newsess && newsess->gui)
			{
				mg_bring_tofront_sess(newsess);
				return 0;
			}

			return 1;
		}
	}

	num = atoi (d1);
	if (!d2)
		num--;
	if (!d2 || d2[0] == 0)
		mg_switch_page (FALSE, num);
	else
		mg_switch_page (TRUE, num);
	return 0;
}

int
key_action_insert (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
						 struct session *sess)
{
	int tmp_pos;
	char *decoded;
	gsize decoded_len;

	if (!d1)
		return 1;

	decoded = g_malloc (strlen (d1) + 1);
	decoded_len = key_action_decode_escapes (d1, decoded);

	tmp_pos = SPELL_ENTRY_GET_POS (wid);
	SPELL_ENTRY_INSERT (wid, decoded, decoded_len, &tmp_pos);
	SPELL_ENTRY_SET_POS (wid, tmp_pos);
	g_free (decoded);
	return 2;
}

/* handles PageUp/Down keys */
static int
key_action_scroll_page (GtkWidget * wid, GdkEventKey * evt, char *d1,
								char *d2, struct session *sess)
{
	int value, end;
	GtkAdjustment *adj;
	enum scroll_type { PAGE_TOP, PAGE_BOTTOM, PAGE_UP, PAGE_DOWN, LINE_UP, LINE_DOWN };
	int type = PAGE_DOWN;

	if (d1)
	{
		if (!g_ascii_strcasecmp (d1, "top"))
			type = PAGE_TOP;
		else if (!g_ascii_strcasecmp (d1, "bottom"))
			type = PAGE_BOTTOM;
		else if (!g_ascii_strcasecmp (d1, "up"))
			type = PAGE_UP;
		else if (!g_ascii_strcasecmp (d1, "down"))
			type = PAGE_DOWN;
		else if (!g_ascii_strcasecmp (d1, "+1"))
			type = LINE_DOWN;
		else if (!g_ascii_strcasecmp (d1, "-1"))
			type = LINE_UP;
	}

	if (!sess)
		return 0;

	adj = gtk_range_get_adjustment (GTK_RANGE (sess->gui->vscrollbar));
	end = gtk_adjustment_get_upper (adj) - gtk_adjustment_get_lower (adj) - gtk_adjustment_get_page_size (adj);

	switch (type)
	{
	case PAGE_TOP:
		value = 0;
		break;

	case PAGE_BOTTOM:
		value = end;
		break;

	case PAGE_UP:
		value = gtk_adjustment_get_value (adj) - (gtk_adjustment_get_page_size (adj) - 1);
		break;

	case PAGE_DOWN:
		value = gtk_adjustment_get_value (adj) + (gtk_adjustment_get_page_size (adj) - 1);
		break;

	case LINE_UP:
		value = gtk_adjustment_get_value (adj) - 1.0;
		break;

	case LINE_DOWN:
		value = gtk_adjustment_get_value (adj) + 1.0;
		break;
	}

	if (value < 0)
		value = 0;
	if (value > end)
		value = end;

	gtk_adjustment_set_value (adj, value);

	return 0;
}

static int
key_action_set_buffer (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
							  struct session *sess)
{
	if (!d1)
		return 1;
	if (d1[0] == 0)
		return 1;

	SPELL_ENTRY_SET_TEXT (wid, d1);
	SPELL_ENTRY_SET_POS (wid, -1);

	return 2;
}

static int
key_action_history_up (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
							  struct session *sess)
{
	char *new_line;

	new_line = history_up (&sess->history, SPELL_ENTRY_GET_TEXT (wid));
	if (new_line)
	{
		SPELL_ENTRY_SET_TEXT (wid, new_line);
		SPELL_ENTRY_SET_POS (wid, -1);
	}

	return 2;
}

static int
key_action_history_down (GtkWidget * wid, GdkEventKey * ent, char *d1,
								 char *d2, struct session *sess)
{
	char *new_line;

	new_line = history_down (&sess->history);
	if (new_line)
	{
		SPELL_ENTRY_SET_TEXT (wid, new_line);
		SPELL_ENTRY_SET_POS (wid, -1);
	}

	return 2;
}

/* old data that we reuse */
static struct gcomp_data old_gcomp;

/* work on the data, ie return only channels */
static int
double_chan_cb (session *lsess, GList **list)
{
	if (lsess->type == SESS_CHANNEL)
		*list = g_list_prepend(*list, lsess->channel);
	return TRUE;
}

/* convert a slist -> list. */
static GList *
chanlist_double_list (GSList *inlist)
{
	GList *list = NULL;
	g_slist_foreach(inlist, (GFunc)double_chan_cb, &list);
	return list;
}

/* handle commands */
static int
double_cmd_cb (struct popup *pop, GList **list)
{
	*list = g_list_prepend(*list, pop->name);
	return TRUE;
}

/* convert a slist -> list. */
static GList *
cmdlist_double_list (GSList *inlist)
{
	GList *list = NULL;
	g_slist_foreach (inlist, (GFunc)double_cmd_cb, &list);
	return list;
}

static char *
gcomp_nick_func (char *data)
{
	if (data)
		return ((struct User *)data)->nick;
	return "";
}

void
key_action_tab_clean(void)
{
	if (old_gcomp.elen)
	{
		old_gcomp.data[0] = 0;
		old_gcomp.elen = 0;
	}
}

/* For use in sorting the user list for completion

This sorts everyone by the last talked time except your own nick
which is forced to the bottom of the list to avoid completing your
own name, which is very unlikely.
*/
static int
talked_recent_cmp (struct User *a, struct User *b)
{
	if (a->me)
		return -1;

	if (b->me)
		return 1;

	if (a->lasttalk < b->lasttalk)
		return -1;

	if (a->lasttalk > b->lasttalk)
		return 1;

	return 0;
}

#define COMP_BUF 2048

static inline glong
len_to_offset (const char *str, glong len)
{
	return g_utf8_pointer_to_offset (str, str + len);
}

static inline glong
offset_to_len (const char *str, glong offset)
{
	return g_utf8_offset_to_pointer (str, offset) - str;
}

static int
key_action_tab_comp (GtkWidget *t, GdkEventKey *entry, char *d1, char *d2,
							struct session *sess)
{
	int len = 0, elen = 0, i = 0, cursor_pos, ent_start = 0, comp = 0, prefix_len, skip_len = 0;
	gboolean is_nick = FALSE, is_cmd = FALSE, found = FALSE, has_nick_prefix = FALSE;
	char ent[CHANLEN], *postfix = NULL, *result, *ch;
	GList *list = NULL, *tmp_list = NULL;
	const char *text;
	GCompletion *gcomp = NULL;
	GString *buf;

	/* force the IM Context to reset */
	SPELL_ENTRY_SET_EDITABLE (t, FALSE);
	SPELL_ENTRY_SET_EDITABLE (t, TRUE);

	text = SPELL_ENTRY_GET_TEXT (t);
	if (text[0] == 0)
		return 1;

	len = g_utf8_strlen (text, -1); /* must be null terminated */

	cursor_pos = SPELL_ENTRY_GET_POS (t);

	/* handle "nick: " or "nick " or "#channel "*/
	ch = g_utf8_find_prev_char(text, g_utf8_offset_to_pointer(text,cursor_pos));
	if (ch && ch[0] == ' ')
	{
		skip_len++;
		ch = g_utf8_find_prev_char(text, ch);
		if (!ch)
			return 2;

		cursor_pos = g_utf8_pointer_to_offset(text, ch);
		if (cursor_pos && (g_utf8_get_char_validated(ch, -1) == ':' || 
					g_utf8_get_char_validated(ch, -1) == ',' ||
					g_utf8_get_char_validated (ch, -1) == g_utf8_get_char_validated (prefs.hex_completion_suffix, -1)))
		{
			skip_len++;
		}
		else
			cursor_pos = g_utf8_pointer_to_offset(text, g_utf8_offset_to_pointer(ch, 1));
	}

	comp = skip_len;
	
	/* store the text following the cursor for reinsertion later */
	if ((cursor_pos + skip_len) < len)
		postfix = g_utf8_offset_to_pointer(text, cursor_pos + skip_len);

	for (ent_start = cursor_pos; ; --ent_start)
	{
		if (ent_start == 0)
			break;
		ch = g_utf8_offset_to_pointer(text, ent_start - 1);
		if (ch && ch[0] == ' ')
			break;
	}

	if (ent_start == 0 && text[0] == prefs.hex_input_command_char[0])
	{
		ent_start++;
		is_cmd = TRUE;
	}
	else if (strchr (sess->server->chantypes, text[ent_start]) == NULL)
	{
		is_nick = TRUE;
		if (strchr (sess->server->nick_prefixes, text[ent_start]) != NULL)
		{
			if (ent_start == 0)
				has_nick_prefix = TRUE;
			ent_start++;
		}
	}

	prefix_len = ent_start;
	elen = cursor_pos - ent_start;

	g_utf8_strncpy (ent, g_utf8_offset_to_pointer (text, prefix_len), elen);
	
	if (sess->type == SESS_DIALOG && is_nick)
	{
		/* tab in a dialog completes the other person's name */
		if (rfc_ncasecmp (sess->channel, ent, elen) == 0)
		{
			result =  sess->channel;
			is_nick = FALSE;
		}
		else
			return 2;
	}
	else
	{
		if (is_nick)
		{
			gcomp = g_completion_new((GCompletionFunc)gcomp_nick_func);
			tmp_list = userlist_double_list(sess); /* create a temp list so we can free the memory */
			if (prefs.hex_completion_sort == 1)	/* sort in last-talk order? */
				tmp_list = g_list_sort (tmp_list, (void *)talked_recent_cmp);
		}
		else
		{
			gcomp = g_completion_new (NULL);
			if (is_cmd)
			{
				tmp_list = cmdlist_double_list (command_list);
				for(i = 0; xc_cmds[i].name != NULL ; i++)
				{
					tmp_list = g_list_prepend (tmp_list, xc_cmds[i].name);
				}
				tmp_list = plugin_command_list(tmp_list);
			}
			else
				tmp_list = chanlist_double_list (sess_list);
		}
		tmp_list = g_list_reverse(tmp_list); /* make the comp entries turn up in the right order */
		g_completion_set_compare (gcomp, (GCompletionStrncmpFunc)rfc_ncasecmp);
		if (tmp_list)
		{
			g_completion_add_items (gcomp, tmp_list);
			g_list_free (tmp_list);
		}

		if (comp && !(rfc_ncasecmp(old_gcomp.data, ent, old_gcomp.elen) == 0))
		{
			key_action_tab_clean ();
			comp = 0;
		}
	
		list = g_completion_complete_utf8 (gcomp, comp ? old_gcomp.data : ent, &result);
		
		if (result == NULL) /* No matches found */
		{
			g_completion_free(gcomp);
			return 2;
		}

		if (comp) /* existing completion */
		{
			while(list) /* find the current entry */
			{
				if(rfc_ncasecmp(list->data, ent, elen) == 0)
				{
					found = TRUE;
					break;
				}
				list = list->next;
			}

			if (found)
			{
				if (!(d1 && d1[0])) /* not holding down shift */
				{
					if (g_list_next(list) == NULL)
						list = g_list_first(list);
					else
						list = g_list_next(list);
				}
				else
				{
					if (g_list_previous(list) == NULL)
						list = g_list_last(list);
					else
						list = g_list_previous(list);
				}
				g_free(result);
				result = (char*)list->data;
			}
			else
			{
				g_free(result);
				g_completion_free(gcomp);
				return 2;
			}
		}
		else
		{
			strcpy(old_gcomp.data, ent);
			old_gcomp.elen = elen;

			/* Get the first nick and put out the data for future nickcompletes */
			if (prefs.hex_completion_amount > 0 && g_list_length (list) <= (guint) prefs.hex_completion_amount)
			{
				g_free(result);
				result = (char*)list->data;
			}
			else
			{
				/* bash style completion */
				if (g_list_next(list) != NULL)
				{
					buf = g_string_sized_new (MAX(COMP_BUF, len + NICKLEN));
					if (strlen (result) > elen) /* the largest common prefix is larger than nick, change the data */
					{
						if (prefix_len)
							g_string_append_len (buf, text, offset_to_len (text, prefix_len));
						g_string_append (buf, result);
						cursor_pos = buf->len;
						g_free(result);
						if (postfix)
						{
							g_string_append_c (buf, ' ');
							g_string_append (buf, postfix);
						}
						SPELL_ENTRY_SET_TEXT (t, buf->str);
						SPELL_ENTRY_SET_POS (t, len_to_offset (buf->str, cursor_pos));
						g_string_erase (buf, 0, -1);
					}
					else
						g_free(result);

					while (list)
					{
						len = buf->len;
						elen = strlen (list->data);	/* next item to add */
						if (len + elen + 2 >= COMP_BUF) /* +2 is space + null */
						{
							PrintText (sess, buf->str);
							g_string_erase (buf, 0, -1);
						}
						g_string_append (buf, (char*)list->data);
						g_string_append_c (buf, ' ');
						list = list->next;
					}
					PrintText (sess, buf->str);
					g_completion_free(gcomp);
					g_string_free (buf, TRUE);
					return 2;
				}
				/* Only one matching entry */
				g_free(result);
				result = list->data;
			}
		}
	}
	
	if(result)
	{
		buf = g_string_sized_new (len + NICKLEN);
		if (prefix_len)
			g_string_append_len (buf, text, offset_to_len (text, prefix_len));
		g_string_append (buf, result);
		if((!prefix_len || has_nick_prefix) && is_nick && prefs.hex_completion_suffix[0] != '\0')
			g_string_append_unichar (buf, g_utf8_get_char_validated (prefs.hex_completion_suffix, -1));
		g_string_append_c (buf, ' ');
		cursor_pos = buf->len;
		if (postfix)
			g_string_append (buf, postfix);
		SPELL_ENTRY_SET_TEXT (t, buf->str);
		SPELL_ENTRY_SET_POS (t, len_to_offset (buf->str, cursor_pos));
		g_string_free (buf, TRUE);
	}
	if (gcomp)
		g_completion_free(gcomp);
	return 2;
}
#undef COMP_BUF

static int
key_action_comp_chng (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
		struct session *sess)
{
	key_action_tab_comp(wid, ent, d1, d2, sess);
	return 2;
}


static int
key_action_replace (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
						  struct session *sess)
{
	replace_handle (wid);
	return 1;
}


static int
key_action_move_tab_left (GtkWidget * wid, GdkEventKey * ent, char *d1,
								  char *d2, struct session *sess)
{
	mg_move_tab (sess, +1);
	return 2;
}

static int
key_action_move_tab_right (GtkWidget * wid, GdkEventKey * ent, char *d1,
									char *d2, struct session *sess)
{
	mg_move_tab (sess, -1);
	return 2;
}

static int
key_action_move_tab_family_left (GtkWidget * wid, GdkEventKey * ent, char *d1,
								  char *d2, struct session *sess)
{
	mg_move_tab_family (sess, +1);
	return 2;
}

static int
key_action_move_tab_family_right (GtkWidget * wid, GdkEventKey * ent, char *d1,
									char *d2, struct session *sess)
{
	mg_move_tab_family (sess, -1);
	return 2;
}

static int
key_action_put_history (GtkWidget * wid, GdkEventKey * ent, char *d1,
									char *d2, struct session *sess)
{
	history_add (&sess->history, SPELL_ENTRY_GET_TEXT (wid));
	SPELL_ENTRY_SET_TEXT (wid, "");
	return 2;
}

static gboolean
replace_set_pos_idle (gpointer data)
{
	GtkWidget *t = GTK_WIDGET (data);
	gpointer pos_data = g_object_get_data (G_OBJECT (t), "zoitechat-replace-pos");

	if (pos_data)
		SPELL_ENTRY_SET_POS (t, GPOINTER_TO_INT (pos_data));

	g_object_set_data (G_OBJECT (t), "zoitechat-replace-pos", NULL);
	g_object_unref (t);
	return G_SOURCE_REMOVE;
}

static void
replace_handle (GtkWidget *t)
{
	const char *text;
	struct popup *pop;
	GSList *list = replace_list;
	glong cursor_pos;
	const char *cursor_ptr;
	const char *match_start;
	GString *buf;
	gboolean matched;
	size_t replacement_len;
	size_t match_len;
	ptrdiff_t cursor_byte_offset;
	ptrdiff_t match_start_offset;
	ptrdiff_t new_cursor_offset;
	const char *best_match;
	size_t best_len;
	struct popup *best_pop;
	int best_rank;
	ptrdiff_t best_distance;

	text = SPELL_ENTRY_GET_TEXT (t);

	if (!text || text[0] == '\0')
		return;

	cursor_pos = SPELL_ENTRY_GET_POS (t);
	cursor_ptr = g_utf8_offset_to_pointer (text, cursor_pos);
	matched = FALSE;
	match_start = NULL;
	match_len = 0;
	best_match = NULL;
	best_len = 0;
	best_pop = NULL;
	best_rank = 3;
	best_distance = 0;
	cursor_byte_offset = cursor_ptr - text;
	while (list)
	{
		pop = (struct popup *) list->data;
		if (pop->name[0] != '\0')
		{
			size_t pop_len = strlen (pop->name);
			const char *found = text;

			while ((found = strstr (found, pop->name)) != NULL)
			{
				ptrdiff_t found_offset = found - text;
				ptrdiff_t found_end_offset = found_offset + (ptrdiff_t) pop_len;
				gboolean start_ok;
				gboolean end_ok;
				int rank;
				ptrdiff_t distance;
				const char *before = found;
				const char *after = found + pop_len;

				if (before > text)
				{
					before = g_utf8_find_prev_char (text, before);
					start_ok = !before || (!g_unichar_isalnum (g_utf8_get_char (before)) && g_utf8_get_char (before) != '_');
				}
				else
				{
					start_ok = TRUE;
				}

				if (*after != '\0')
				{
					end_ok = !g_unichar_isalnum (g_utf8_get_char (after)) && g_utf8_get_char (after) != '_';
				}
				else
				{
					end_ok = TRUE;
				}

				if (!start_ok || !end_ok)
				{
					found++;
					continue;
				}

				if (cursor_byte_offset >= found_offset && cursor_byte_offset <= found_end_offset)
				{
					rank = 0;
					distance = found_end_offset - cursor_byte_offset;
				}
				else if (found_end_offset <= cursor_byte_offset)
				{
					rank = 1;
					distance = cursor_byte_offset - found_end_offset;
				}
				else
				{
					rank = 2;
					distance = found_offset - cursor_byte_offset;
				}

				if (rank < best_rank || (rank == best_rank && distance < best_distance))
				{
					best_rank = rank;
					best_distance = distance;
					best_match = found;
					best_len = pop_len;
					best_pop = pop;
				}

				found++;
			}
		}
		list = list->next;
	}

	if (best_match)
	{
		match_start = best_match;
		match_len = best_len;
		pop = best_pop;
		matched = TRUE;
	}

	if (!matched)
		return;

	replacement_len = strlen (pop->cmd);
	match_start_offset = match_start - text;
	new_cursor_offset = match_start_offset + (ptrdiff_t) replacement_len;
	buf = g_string_sized_new (strlen (text) + 32);
	g_string_append_len (buf, text, match_start - text);
	g_string_append (buf, pop->cmd);
	g_string_append (buf, match_start + match_len);
	SPELL_ENTRY_SET_TEXT (t, buf->str);
	new_cursor_offset = len_to_offset (buf->str, new_cursor_offset);
	SPELL_ENTRY_SET_POS (t, new_cursor_offset);
	g_object_set_data (G_OBJECT (t), "zoitechat-replace-pos", GINT_TO_POINTER ((gint) new_cursor_offset));
	g_idle_add (replace_set_pos_idle, g_object_ref (t));
	g_string_free (buf, TRUE);
}

void
key_check_replace_on_change (GtkEditable *editable, gpointer data)
{
	static gboolean replace_in_progress = FALSE;

	if (replace_in_progress)
		return;

	replace_in_progress = TRUE;
	replace_handle (GTK_WIDGET (editable));
	replace_in_progress = FALSE;
}
