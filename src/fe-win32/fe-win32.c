#include "config.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef WIN32
#include <io.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#else
#include <unistd.h>
#endif
#include <sys/types.h>
#include <ctype.h>
#include <glib-object.h>
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/outbound.h"
#include "../common/util.h"
#include "../common/fe.h"

static int done;
static GMainLoop *main_loop;
static int done_intro;

static void send_command (char *cmd)
{
	handle_multiline (current_tab, cmd, TRUE, FALSE);
}

static gboolean handle_line (GIOChannel *channel, GIOCondition cond, gpointer data)
{
	gchar *str_return;
	gsize length;
	gsize terminator_pos;
	GError *error = NULL;
	GIOStatus result;
	result = g_io_channel_read_line (channel, &str_return, &length, &terminator_pos, &error);
	if (result == G_IO_STATUS_ERROR || result == G_IO_STATUS_EOF)
		return FALSE;
	send_command (str_return);
	g_free (str_return);
	return TRUE;
}

void fe_new_window (struct session *sess, int focus)
{
	char buf[512];
	current_sess = sess;
	if (!sess->server->front_session)
		sess->server->front_session = sess;
	if (!sess->server->server_session)
		sess->server->server_session = sess;
	if (!current_tab || focus)
		current_tab = sess;
	if (done_intro)
		return;
	done_intro = 1;
	g_snprintf (buf, sizeof (buf), "\n ZoiteChat-Win32 " PACKAGE_VERSION "\n Running on %s\n", get_sys_str (1));
	fe_print_text (sess, buf, 0, FALSE);
}

void fe_print_text (struct session *sess, char *text, time_t stamp, gboolean no_activity)
{
	int i = 0;
	int j = 0;
	int len = strlen (text);
	unsigned char *newtext = g_malloc (len + 2);
	for (i = 0; i < len; i++)
	{
		if (text[i] == '\t')
			newtext[j++] = ' ';
		else
			newtext[j++] = text[i];
	}
	if (len == 0 || text[len - 1] != '\n')
		newtext[j++] = '\n';
	newtext[j] = 0;
	write (STDOUT_FILENO, newtext, j);
	g_free (newtext);
}

void fe_timeout_remove (int tag) { g_source_remove (tag); }
int fe_timeout_add (int interval, void *callback, void *userdata) { return g_timeout_add (interval, (GSourceFunc) callback, userdata); }
int fe_timeout_add_seconds (int interval, void *callback, void *userdata) { return g_timeout_add_seconds (interval, (GSourceFunc) callback, userdata); }
void fe_input_remove (int tag) { g_source_remove (tag); }

int fe_input_add (int sok, int flags, void *func, void *data)
{
	int tag;
	int type = 0;
	GIOChannel *channel;
#ifdef G_OS_WIN32
	channel = (flags & FIA_FD) ? g_io_channel_win32_new_fd (sok) : g_io_channel_win32_new_socket (sok);
#else
	channel = g_io_channel_unix_new (sok);
#endif
	if (flags & FIA_READ)
		type |= G_IO_IN | G_IO_HUP | G_IO_ERR;
	if (flags & FIA_WRITE)
		type |= G_IO_OUT | G_IO_ERR;
	if (flags & FIA_EX)
		type |= G_IO_PRI;
	tag = g_io_add_watch (channel, type, (GIOFunc) func, data);
	g_io_channel_unref (channel);
	return tag;
}

static char *arg_cfgdir = NULL;
static gint arg_show_autoload;
static gint arg_show_config;
static gint arg_show_version;

static const GOptionEntry gopt_entries[] = {
	{"no-auto", 'a', 0, G_OPTION_ARG_NONE, &arg_dont_autoconnect, N_("Don't auto connect to servers"), NULL},
	{"cfgdir", 'd', 0, G_OPTION_ARG_STRING, &arg_cfgdir, N_("Use a different config directory"), "PATH"},
	{"no-plugins", 'n', 0, G_OPTION_ARG_NONE, &arg_skip_plugins, N_("Don't auto load any plugins"), NULL},
	{"plugindir", 'p', 0, G_OPTION_ARG_NONE, &arg_show_autoload, N_("Show plugin/script auto-load directory"), NULL},
	{"configdir", 'u', 0, G_OPTION_ARG_NONE, &arg_show_config, N_("Show user config directory"), NULL},
	{"url", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_url, N_("Open an irc://server:port/channel URL"), "URL"},
	{"version", 'v', 0, G_OPTION_ARG_NONE, &arg_show_version, N_("Show version information"), NULL},
	{G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &arg_urls, N_("Open an irc://server:port/channel?key URL"), "URL"},
	{NULL}
};

int fe_args (int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, gopt_entries, GETTEXT_PACKAGE);
	g_option_context_parse (context, &argc, &argv, &error);
	if (error)
	{
		if (error->message)
			printf ("%s\n", error->message);
		return 1;
	}
	g_option_context_free (context);
	if (arg_show_version)
	{
		printf (PACKAGE_NAME " " PACKAGE_VERSION "\n");
		return 0;
	}
	if (arg_show_autoload)
	{
		printf ("%s\n", ZOITECHATLIBDIR);
		return 0;
	}
	if (arg_show_config)
	{
		printf ("%s\n", get_xdir ());
		return 0;
	}
	if (arg_cfgdir)
	{
		g_free (xdir);
		xdir = g_strdup (arg_cfgdir);
		if (xdir[strlen (xdir) - 1] == '/')
			xdir[strlen (xdir) - 1] = 0;
		g_free (arg_cfgdir);
	}
	return -1;
}

void fe_init (void)
{
	prefs.hex_gui_tab_server = 0;
	prefs.hex_gui_autoopen_dialog = 0;
	prefs.hex_gui_lagometer = 0;
	prefs.hex_gui_slist_skip = 1;
}

void fe_main (void)
{
	GIOChannel *keyboard_input;
	main_loop = g_main_loop_new (NULL, FALSE);
#ifdef G_OS_WIN32
	keyboard_input = g_io_channel_win32_new_fd (STDIN_FILENO);
#else
	keyboard_input = g_io_channel_unix_new (STDIN_FILENO);
#endif
	g_io_add_watch (keyboard_input, G_IO_IN, handle_line, NULL);
	g_main_loop_run (main_loop);
}

void fe_exit (void)
{
	done = TRUE;
	if (main_loop)
		g_main_loop_quit (main_loop);
}

void fe_new_server (struct server *serv) {}
void fe_add_rawlog (struct server *serv, char *text, int len, int outbound) {}
void fe_message (char *msg, int flags) { puts (msg); }
void fe_idle_add (void *func, void *data) { g_idle_add (func, data); }
void fe_set_topic (struct session *sess, char *topic, char *stripped_topic) {}
void fe_set_tab_color (struct session *sess, tabcolor col) {}
void fe_flash_window (struct session *sess) {}
void fe_update_mode_buttons (struct session *sess, char mode, char sign) {}
void fe_update_channel_key (struct session *sess) {}
void fe_update_channel_limit (struct session *sess) {}
int fe_is_chanwindow (struct server *serv) { return 0; }
void fe_add_chan_list (struct server *serv, char *chan, char *users, char *topic) {}
void fe_chan_list_end (struct server *serv) {}
gboolean fe_add_ban_list (struct session *sess, char *mask, char *who, char *when, int rplcode) { return FALSE; }
gboolean fe_ban_list_end (struct session *sess, int rplcode) { return FALSE; }
void fe_notify_update (char *name) {}
void fe_notify_ask (char *name, char *networks) {}
void fe_text_clear (struct session *sess, int lines) {}
void fe_close_window (struct session *sess) { session_free (sess); done = TRUE; }
void fe_progressbar_start (struct session *sess) {}
void fe_progressbar_end (struct server *serv) {}
void fe_userlist_insert (struct session *sess, struct User *newuser, gboolean sel) {}
int fe_userlist_remove (struct session *sess, struct User *user) { return 0; }
void fe_userlist_rehash (struct session *sess, struct User *user) {}
void fe_userlist_update (struct session *sess, struct User *user) {}
void fe_userlist_numbers (struct session *sess) {}
void fe_userlist_clear (struct session *sess) {}
void fe_userlist_set_selected (struct session *sess) {}
void fe_uselect (session *sess, char *word[], int do_clear, int scroll_to) {}
void fe_dcc_add (struct DCC *dcc) {}
void fe_dcc_update (struct DCC *dcc) {}
void fe_dcc_remove (struct DCC *dcc) {}
int fe_dcc_open_recv_win (int passive) { return FALSE; }
int fe_dcc_open_send_win (int passive) { return FALSE; }
int fe_dcc_open_chat_win (int passive) { return FALSE; }
void fe_clear_channel (struct session *sess) {}
void fe_session_callback (struct session *sess) {}
void fe_server_callback (struct server *serv) {}
void fe_url_add (const char *text) {}
void fe_pluginlist_update (void) {}
void fe_buttons_update (struct session *sess) {}
void fe_dlgbuttons_update (struct session *sess) {}
void fe_dcc_send_filereq (struct session *sess, char *nick, int maxcps, int passive) {}
void fe_set_channel (struct session *sess) {}
void fe_set_title (struct session *sess) {}
void fe_set_nonchannel (struct session *sess, int state) {}
void fe_set_nick (struct server *serv, char *newnick) {}
void fe_ignore_update (int level) {}
void fe_beep (session *sess) { putchar (7); }
void fe_lastlog (session *sess, session *lastlog_sess, char *sstr, gtk_xtext_search_flags flags) {}
void fe_set_lag (server *serv, long lag) {}
void fe_set_throttle (server *serv) {}
void fe_set_away (server *serv) {}
void fe_serverlist_open (session *sess) {}
void fe_get_bool (char *title, char *prompt, void *callback, void *userdata) {}
void fe_get_str (char *prompt, char *def, void *callback, void *ud) {}
void fe_get_int (char *prompt, int def, void *callback, void *ud) {}
void fe_get_file (const char *title, char *initial, void (*callback) (void *userdata, char *file), void *userdata, int flags) {}
void fe_ctrl_gui (session *sess, fe_gui_action action, int arg)
{
	if (action == FE_GUI_FOCUS)
	{
		current_sess = sess;
		current_tab = sess;
		sess->server->front_session = sess;
	}
}

int fe_gui_info (session *sess, int info_type) { return -1; }
void *fe_gui_info_ptr (session *sess, int info_type) { return NULL; }
void fe_confirm (const char *message, void (*yesproc)(void *), void (*noproc)(void *), void *ud) {}
char *fe_get_inputbox_contents (struct session *sess) { return NULL; }
int fe_get_inputbox_cursor (struct session *sess) { return 0; }
void fe_set_inputbox_contents (struct session *sess, char *text) {}
void fe_set_inputbox_cursor (struct session *sess, int delta, int pos) {}
void fe_open_url (const char *url) {}
void fe_menu_del (menu_entry *me) {}
char *fe_menu_add (menu_entry *me) { return NULL; }
void fe_menu_update (menu_entry *me) {}
void fe_server_event (server *serv, int type, int arg) {}
void fe_tray_set_flash (const char *filename1, const char *filename2, int timeout) {}
void fe_tray_set_file (const char *filename) {}
void fe_tray_set_icon (feicon icon) {}
void fe_tray_set_tooltip (const char *text) {}
void fe_open_chan_list (server *serv, char *filter, int do_refresh) { serv->p_list_channels (serv, filter, 1); }
const char *fe_get_default_font (void) { return NULL; }
void fe_cleanup (void) {}
