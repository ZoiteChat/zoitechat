/* X-Chat
 * Copyright (C) 2004-2008 Peter Zelezny.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#ifdef WIN32
#include <windows.h>
#include <winhttp.h>
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif
#endif

#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/servlist.h"
#include "../common/cfgfiles.h"
#include "../common/fe.h"
#include "../common/secretstore.h"
#include "../common/server.h"
#include "../common/util.h"

#include "fe-gtk.h"
#include "gtkutil.h"
#include "menu.h"
#include "pixmaps.h"
#include "fkeys.h"
#include "theme/theme-manager.h"

#define SERVLIST_X_PADDING 4			/* horizontal paddig in the network editor */
#define SERVLIST_Y_PADDING 0			/* vertical padding in the network editor */

#define ICON_SERVLIST_CONNECT "zc-menu-connect"
#define ICON_SERVLIST_ADD "list-add"
#define ICON_SERVLIST_REMOVE "list-remove"
#define ICON_SERVLIST_CLOSE "gtk-close"
#define ICON_SERVLIST_ERROR "dialog-error"

#ifdef USE_OPENSSL
# define DEFAULT_SERVER "newserver/6697"
#else
# define DEFAULT_SERVER "newserver/6667"
#endif

/* servlistgui.c globals */
static GtkWidget *serverlist_win = NULL;
static GtkWidget *networks_tree;		/* network TreeView */

static int netlist_win_width = 0;		/* don't hardcode pixels, just use as much as needed by default, save if resized */
static int netlist_win_height = 0;
static int netedit_win_width = 0;
static int netedit_win_height = 0;

static int netedit_active_tab = 0;

/* global user info */
static GtkWidget *entry_nick1;
static GtkWidget *entry_nick2;
static GtkWidget *entry_nick3;
static GtkWidget *entry_guser;
/* static GtkWidget *entry_greal; */

enum {
		SERVER_TREE,
		CHANNEL_TREE,
		CMD_TREE,
		N_TREES,
};

/* edit area */
static GtkWidget *edit_win;
static GtkWidget *edit_entry_nick;
static GtkWidget *edit_entry_nick2;
static GtkWidget *edit_entry_user;
static GtkWidget *edit_entry_real;
static GtkWidget *edit_entry_pass;
static GtkWidget *edit_check_show_pass;
static GtkWidget *edit_check_use_keyring;
static GtkWidget *edit_check_ask_pass;
static GtkWidget *edit_button_encrypt_pass;
static GtkWidget *edit_button_import_pass;
static int edit_pass_changed;
static char *edit_loaded_password;
static GtkWidget *edit_label_nick;
static GtkWidget *edit_label_nick2;
static GtkWidget *edit_label_real;
static GtkWidget *edit_label_user;
static GtkWidget *edit_trees[N_TREES];
static GtkWidget *edit_button_cert_generate;
static GtkWidget *edit_button_cert_import;
static GtkWidget *edit_button_cert_info;
static GtkWidget *edit_button_cert_delete;

static ircnet *selected_net = NULL;
static ircserver *selected_serv = NULL;
static commandentry *selected_cmd = NULL;
static favchannel *selected_chan = NULL;
static session *servlist_sess;

static void servlist_network_row_cb (GtkTreeSelection *sel, gpointer user_data);
static GtkWidget *servlist_open_edit (GtkWidget *parent, ircnet *net);
static void servlist_password_changed_cb (GtkEditable *editable, gpointer userdata);

static void
servlist_update_password_tools (ircnet *net)
{
	gboolean has_local;
	gboolean use_keyring;

	if (!edit_button_encrypt_pass || !edit_button_import_pass)
		return;

	use_keyring = net && (net->flags & FLAG_USE_KEYRING);
	if ((net && (net->flags & FLAG_PROMPT_PASSWORD)) ||
		(edit_check_ask_pass && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (edit_check_ask_pass))))
		use_keyring = FALSE;
	has_local = net && net->pass && *net->pass && !use_keyring && !edit_pass_changed &&
		(!edit_check_ask_pass || !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (edit_check_ask_pass)));
	gtk_widget_set_sensitive (edit_button_encrypt_pass, has_local && !servlist_password_is_encrypted (net->pass));
	gtk_widget_set_sensitive (edit_button_import_pass, has_local);
}

static void
servlist_entry_set_text_silent (GtkWidget *entry, const char *text)
{
	g_signal_handlers_block_by_func (G_OBJECT (entry), G_CALLBACK (servlist_password_changed_cb), NULL);
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	g_signal_handlers_unblock_by_func (G_OBJECT (entry), G_CALLBACK (servlist_password_changed_cb), NULL);
}

static void
servlist_toggle_ask_pass_cb (GtkToggleButton *toggle, gpointer userdata)
{
	gboolean active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (edit_check_use_keyring, !active);
	gtk_widget_set_sensitive (edit_entry_pass, !active && (!selected_net || selected_net->logintype != LOGIN_SASLEXTERNAL));
	gtk_widget_set_sensitive (edit_check_show_pass, !active);
	servlist_update_password_tools (selected_net);
}

static char *
servlist_display_password (ircnet *net)
{
	if (!net)
		return NULL;
	if (edit_pass_changed)
		return g_strdup (gtk_entry_get_text (GTK_ENTRY (edit_entry_pass)));
	if (edit_loaded_password)
		return g_strdup (edit_loaded_password);
	if (net->flags & FLAG_USE_KEYRING)
		return secretstore_get_network_password (net->name);
	return servlist_password_decrypt_for_storage (net->pass);
}

static void
servlist_toggle_show_password_cb (GtkToggleButton *toggle, gpointer userdata)
{
	if (gtk_toggle_button_get_active (toggle))
	{
		char *password = servlist_display_password (selected_net);
		if (password)
		{
			if (edit_loaded_password)
			{
				memset (edit_loaded_password, 0, strlen (edit_loaded_password));
				g_free (edit_loaded_password);
			}
			edit_loaded_password = g_strdup (password);
			servlist_entry_set_text_silent (userdata, password);
			memset (password, 0, strlen (password));
			g_free (password);
		}
		gtk_entry_set_visibility (GTK_ENTRY (userdata), TRUE);
	}
	else
	{
		gtk_entry_set_visibility (GTK_ENTRY (userdata), FALSE);
		if (edit_loaded_password && !edit_pass_changed)
			servlist_entry_set_text_silent (userdata, "***");
	}
}


static void
servlist_toggle_keyring_cb (GtkToggleButton *toggle, gpointer userdata)
{
	servlist_update_password_tools (selected_net);
}

static void
servlist_password_changed_cb (GtkEditable *editable, gpointer userdata)
{
	edit_pass_changed = 1;
	if (edit_loaded_password && strcmp (gtk_entry_get_text (GTK_ENTRY (editable)), "***"))
	{
		memset (edit_loaded_password, 0, strlen (edit_loaded_password));
		g_free (edit_loaded_password);
		edit_loaded_password = NULL;
	}
	servlist_update_password_tools (selected_net);
}

static void
servlist_encrypt_password_cb (GtkWidget *button, gpointer userdata)
{
	ircnet *net = userdata;
	char *plain;
	char *enc;

	if (!net || (net->flags & FLAG_USE_KEYRING) || !net->pass || servlist_password_is_encrypted (net->pass))
		return;

	plain = servlist_password_decrypt_for_storage (net->pass);
	if (!plain || !*plain)
	{
		if (plain)
		{
			memset (plain, 0, strlen (plain));
			g_free (plain);
		}
		return;
	}

	enc = servlist_password_encrypt_for_storage (plain);
	memset (plain, 0, strlen (plain));
	g_free (plain);
	if (!enc)
	{
		fe_message (_("Could not encrypt this password."), FE_MSG_WARN);
		return;
	}

	g_free (net->pass);
	net->pass = enc;
	servlist_save ();
	servlist_update_password_tools (net);
}

static void
servlist_import_password_cb (GtkWidget *button, gpointer userdata)
{
	ircnet *net = userdata;
	char *plain;

	if (!net || !net->name || (net->flags & FLAG_USE_KEYRING) || !net->pass || !*net->pass)
		return;

	plain = servlist_password_decrypt_for_storage (net->pass);
	if (!plain || !*plain)
	{
		if (plain)
		{
			memset (plain, 0, strlen (plain));
			g_free (plain);
		}
		return;
	}

	if (!secretstore_set_network_password (net->name, plain))
	{
		memset (plain, 0, strlen (plain));
		g_free (plain);
		fe_message (_("Could not move this password into the system keyring."), FE_MSG_WARN);
		return;
	}

	memset (plain, 0, strlen (plain));
	g_free (plain);
	g_free (net->pass);
	net->pass = NULL;
	net->flags |= FLAG_USE_KEYRING;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edit_check_use_keyring), TRUE);
	servlist_entry_set_text_silent (edit_entry_pass, "***");
	edit_pass_changed = 0;
	servlist_save ();
	servlist_update_password_tools (net);
}

static char *
servlist_get_cert_file (ircnet *net)
{
	if (!net || !net->name || !net->name[0])
		return NULL;

	return g_strdup_printf ("%s" G_DIR_SEPARATOR_S "certs" G_DIR_SEPARATOR_S "%s.pem",
								 get_xdir (), net->name);
}

static gboolean
servlist_network_cert_exists (ircnet *net)
{
	char *cert_file;
	gboolean exists;

	cert_file = servlist_get_cert_file (net);
	if (!cert_file)
		return FALSE;

	exists = g_file_test (cert_file, G_FILE_TEST_IS_REGULAR);
	g_free (cert_file);
	return exists;
}

static void
servlist_update_cert_buttons (ircnet *net)
{
	gboolean has_cert = servlist_network_cert_exists (net);

	if (edit_button_cert_generate)
		gtk_widget_set_visible (edit_button_cert_generate, !has_cert);
	if (edit_button_cert_import)
		gtk_widget_set_visible (edit_button_cert_import, !has_cert);
	if (edit_button_cert_info)
		gtk_widget_set_visible (edit_button_cert_info, has_cert);
	if (edit_button_cert_delete)
		gtk_widget_set_visible (edit_button_cert_delete, has_cert);
}

static void
servlist_import_client_cert_cb (GtkWidget *button, gpointer userdata)
{
	ircnet *net = (ircnet *)userdata;
	GtkWidget *dialog;
	GtkWidget *message;
	GtkFileFilter *filter;
	char *cert_dir;
	char *cert_file;
	char *source_file;
	char *contents;
	gsize length;

	if (!net || !net->name || !net->name[0])
		return;

	dialog = gtk_file_chooser_dialog_new (_("Import Client Certificate"),
													 GTK_WINDOW (edit_win),
													 GTK_FILE_CHOOSER_ACTION_OPEN,
													 _("_Cancel"), GTK_RESPONSE_CANCEL,
													 _("_Open"), GTK_RESPONSE_ACCEPT,
													 NULL);
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Certificate files"));
	gtk_file_filter_add_pattern (filter, "*.pem");
	gtk_file_filter_add_pattern (filter, "*.crt");
	gtk_file_filter_add_pattern (filter, "*.cer");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
	theme_manager_attach_window (dialog);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
	{
		gtk_widget_destroy (dialog);
		return;
	}

	source_file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	gtk_widget_destroy (dialog);
	if (!source_file)
		return;

	cert_dir = g_build_filename (get_xdir (), "certs", NULL);
	cert_file = servlist_get_cert_file (net);
	contents = NULL;
	length = 0;

	if (cert_file &&
		 g_mkdir_with_parents (cert_dir, 0700) == 0 &&
		 g_file_get_contents (source_file, &contents, &length, NULL) &&
		 g_file_set_contents (cert_file, contents, length, NULL))
	{
		chmod (cert_file, 0600);
		servlist_update_cert_buttons (net);
		message = gtk_message_dialog_new (GTK_WINDOW (edit_win),
													 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
													 GTK_MESSAGE_INFO,
													 GTK_BUTTONS_CLOSE,
													 _("Client certificate imported for \"%s\"."),
													 net->name);
	}
	else
	{
		message = gtk_message_dialog_new (GTK_WINDOW (edit_win),
													 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
													 GTK_MESSAGE_ERROR,
													 GTK_BUTTONS_CLOSE,
													 _("Failed to import client certificate for \"%s\"."),
													 net->name);
	}

	theme_manager_attach_window (message);
	g_signal_connect_swapped (message, "response", G_CALLBACK (gtk_widget_destroy), message);
	gtk_widget_show (message);

	g_free (contents);
	g_free (cert_file);
	g_free (cert_dir);
	g_free (source_file);
}

static void
servlist_generate_client_cert_cb (GtkWidget *button, gpointer userdata)
{
#ifdef USE_OPENSSL
	ircnet *net = (ircnet *)userdata;
	GtkWidget *dialog;
	char *cert_dir;
	char *cert_file;
	char *key_file;
	char *crt_file;
	char *subject;
	char *openssl_conf;
	const char *conf_data;
	char *key_data;
	char *crt_data;
	char *pem_data;
	char *stderr_data;
	char *stdout_data;
	gsize key_len;
	gsize crt_len;
	gboolean spawned;
	gboolean success;
	gint status;
	char *argv[20];
	char **envp;

	if (!net || !net->name || !net->name[0])
		return;

	cert_dir = g_build_filename (get_xdir (), "certs", NULL);
	cert_file = servlist_get_cert_file (net);
	key_file = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%s.key", cert_dir, net->name);
	crt_file = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%s.crt", cert_dir, net->name);
	subject = g_strdup_printf ("/CN=%s", net->name);
	openssl_conf = g_build_filename (cert_dir, "openssl.cnf", NULL);
	conf_data = "[req]\n"
					"distinguished_name=req_distinguished_name\n"
					"[req_distinguished_name]\n";
	key_data = NULL;
	crt_data = NULL;
	pem_data = NULL;
	stderr_data = NULL;
	stdout_data = NULL;
	key_len = 0;
	crt_len = 0;
	success = FALSE;
	status = 0;
	envp = g_environ_unsetenv (g_get_environ (), "LD_LIBRARY_PATH");

	if (g_mkdir_with_parents (cert_dir, 0700) == 0 &&
		 g_file_set_contents (openssl_conf, conf_data, -1, NULL))
	{
		argv[0] = "openssl";
		argv[1] = "req";
		argv[2] = "-x509";
		argv[3] = "-newkey";
		argv[4] = "ec";
		argv[5] = "-pkeyopt";
		argv[6] = "ec_paramgen_curve:P-256";
		argv[7] = "-sha256";
		argv[8] = "-days";
		argv[9] = "3650";
		argv[10] = "-nodes";
		argv[11] = "-keyout";
		argv[12] = key_file;
		argv[13] = "-out";
		argv[14] = crt_file;
		argv[15] = "-config";
		argv[16] = openssl_conf;
		argv[17] = "-subj";
		argv[18] = subject;
		argv[19] = NULL;

		spawned = g_spawn_sync (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL,
									 &stdout_data, &stderr_data, &status, NULL);
		if (spawned && g_spawn_check_exit_status (status, NULL) &&
			 g_file_get_contents (key_file, &key_data, &key_len, NULL) &&
			 g_file_get_contents (crt_file, &crt_data, &crt_len, NULL))
		{
			pem_data = g_strconcat (key_data, crt_data, NULL);
			if (pem_data && g_file_set_contents (cert_file, pem_data, -1, NULL))
			{
				chmod (cert_file, 0600);
				success = TRUE;
			}
		}
	}

	g_remove (key_file);
	g_remove (crt_file);
	g_remove (openssl_conf);

	if (success)
	{
		servlist_update_cert_buttons (net);
		dialog = gtk_message_dialog_new (GTK_WINDOW (edit_win),
												 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
												 GTK_MESSAGE_INFO,
												 GTK_BUTTONS_CLOSE,
												 _("Client certificate generated for \"%s\"."),
												 net->name);
	}
	else
	{
		dialog = gtk_message_dialog_new (GTK_WINDOW (edit_win),
												 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
												 GTK_MESSAGE_ERROR,
												 GTK_BUTTONS_CLOSE,
												 _("Failed to generate the client certificate for \"%s\"."),
												 net->name);
		if (stderr_data && stderr_data[0])
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", stderr_data);
	}
	theme_manager_attach_window (dialog);
	g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
	gtk_widget_show (dialog);

	g_free (stdout_data);
	g_free (stderr_data);
	g_free (pem_data);
	g_free (key_data);
	g_free (crt_data);
	g_free (subject);
	g_free (crt_file);
	g_free (key_file);
	g_free (openssl_conf);
	g_free (cert_file);
	g_free (cert_dir);
	g_strfreev (envp);
#else
	return;
#endif
}

static void
servlist_cert_info_cb (GtkWidget *button, gpointer userdata)
{
#ifdef USE_OPENSSL
	ircnet *net = (ircnet *)userdata;
	GtkWidget *dialog;
	char *cert_file;
	char *stdout_data;
	char *stderr_data;
	gboolean spawned;
	gint status;
	char *argv[12];
	char **envp;

	cert_file = servlist_get_cert_file (net);
	if (!cert_file)
		return;

	stdout_data = NULL;
	stderr_data = NULL;
	status = 0;
	envp = g_environ_unsetenv (g_get_environ (), "LD_LIBRARY_PATH");
	argv[0] = "openssl";
	argv[1] = "x509";
	argv[2] = "-in";
	argv[3] = cert_file;
	argv[4] = "-noout";
	argv[5] = "-subject";
	argv[6] = "-issuer";
	argv[7] = "-startdate";
	argv[8] = "-enddate";
	argv[9] = "-fingerprint";
	argv[10] = "-sha256";
	argv[11] = NULL;

	spawned = g_spawn_sync (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL,
								 &stdout_data, &stderr_data, &status, NULL);

	if (spawned && g_spawn_check_exit_status (status, NULL) && stdout_data && stdout_data[0])
	{
		dialog = gtk_message_dialog_new (GTK_WINDOW (edit_win),
												 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
												 GTK_MESSAGE_INFO,
												 GTK_BUTTONS_CLOSE,
												 _("Client certificate information for \"%s\"."),
												 net->name);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", stdout_data);
	}
	else
	{
		dialog = gtk_message_dialog_new (GTK_WINDOW (edit_win),
												 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
												 GTK_MESSAGE_ERROR,
												 GTK_BUTTONS_CLOSE,
												 _("Failed to read client certificate information for \"%s\"."),
												 net->name);
		if (stderr_data && stderr_data[0])
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", stderr_data);
	}

	theme_manager_attach_window (dialog);
	g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
	gtk_widget_show (dialog);
	g_free (stdout_data);
	g_free (stderr_data);
	g_free (cert_file);
	g_strfreev (envp);
#else
	return;
#endif
}

static void
servlist_delete_client_cert_cb (GtkWidget *button, gpointer userdata)
{
	ircnet *net = (ircnet *)userdata;
	GtkWidget *dialog;
	char *cert_file;

	cert_file = servlist_get_cert_file (net);
	if (!cert_file)
		return;

	if (g_remove (cert_file) == 0)
	{
		servlist_update_cert_buttons (net);
		dialog = gtk_message_dialog_new (GTK_WINDOW (edit_win),
												 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
												 GTK_MESSAGE_INFO,
												 GTK_BUTTONS_CLOSE,
												 _("Client certificate removed for \"%s\"."),
												 net->name);
	}
	else
	{
		dialog = gtk_message_dialog_new (GTK_WINDOW (edit_win),
												 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
												 GTK_MESSAGE_ERROR,
												 GTK_BUTTONS_CLOSE,
												 _("Failed to remove client certificate for \"%s\"."),
												 net->name);
	}

	theme_manager_attach_window (dialog);
	g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
	gtk_widget_show (dialog);
	g_free (cert_file);
}

static GtkWidget *
servlist_icon_button_new (const char *label, const char *icon_name)
{
	GtkWidget *button;

	(void)icon_name;
	button = gtk_button_new_with_mnemonic (label);

	return button;
}


static const char *pages[]=
{
	IRC_DEFAULT_CHARSET,
	"CP1252 (Windows-1252)",
	"ISO-8859-15 (Western Europe)",
	"ISO-8859-2 (Central Europe)",
	"ISO-8859-7 (Greek)",
	"ISO-8859-8 (Hebrew)",
	"ISO-8859-9 (Turkish)",
	"ISO-2022-JP (Japanese)",
	"SJIS (Japanese)",
	"CP949 (Korean)",
	"KOI8-R (Cyrillic)",
	"CP1251 (Cyrillic)",
	"CP1256 (Arabic)",
	"CP1257 (Baltic)",
	"GB18030 (Chinese)",
	"TIS-620 (Thai)",
	NULL
};

/* This is our dictionary for authentication types. Keep these in sync with
 * login_types[]! This allows us to re-order the login type dropdown in the
 * network list without breaking config compatibility.
 *
 * Also make sure inbound_nickserv_login() won't break, i.e. if you add a new
 * type that is NickServ-based, add it there as well so that ZoiteChat knows to
 * treat it as such.
 */
static int login_types_conf[] =
{
	LOGIN_DEFAULT,			/* default entry - we don't use this but it makes indexing consistent with login_types[] so it's nice */
	LOGIN_SASL,
#ifdef USE_OPENSSL
	LOGIN_SASLEXTERNAL,
	LOGIN_SASL_SCRAM_SHA_1,
	LOGIN_SASL_SCRAM_SHA_256,
	LOGIN_SASL_SCRAM_SHA_512,
#endif
	LOGIN_PASS,
	LOGIN_MSG_NICKSERV,
	LOGIN_NICKSERV,
#ifdef USE_OPENSSL
	LOGIN_CHALLENGEAUTH,
#endif
	LOGIN_CUSTOM
#if 0
	LOGIN_NS,
	LOGIN_MSG_NS,
	LOGIN_AUTH,
#endif
};

static const char *login_types[]=
{
	"Default",
	"SASL PLAIN (username + password)",
#ifdef USE_OPENSSL
	"SASL EXTERNAL (cert)",
	"SASL SCRAM-SHA-1",
	"SASL SCRAM-SHA-256",
	"SASL SCRAM-SHA-512",
#endif
	"Server password (/PASS password)",
	"NickServ (/MSG NickServ + password)",
	"NickServ (/NICKSERV + password)",
#ifdef USE_OPENSSL
	"Challenge Auth (username + password)",
#endif
	"Custom... (connect commands)",
#if 0
	"NickServ (/NS + password)",
	"NickServ (/MSG NS + password)",
	"AUTH (/AUTH nickname password)",
#endif
	NULL
};

/* poor man's IndexOf() - find the dropdown string index that belongs to the given config value */
static int
servlist_get_login_desc_index (int conf_value)
{
	int i;
	int length = sizeof (login_types_conf) / sizeof (login_types_conf[0]);		/* the number of elements in the conf array */

	for (i = 0; i < length; i++)
	{
		if (login_types_conf[i] == conf_value)
		{
			return i;
		}
	}

	return 0;	/* make the compiler happy */
}

static void
servlist_select_and_show (GtkTreeView *treeview, GtkTreeIter *iter,
								  GtkListStore *store)
{
	GtkTreePath *path;
	GtkTreeSelection *sel;

	sel = gtk_tree_view_get_selection (treeview);

	/* select this network */
	gtk_tree_selection_select_iter (sel, iter);
	/* and make sure it's visible */
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
	if (path)
	{
		gtk_tree_view_scroll_to_cell (treeview, path, NULL, TRUE, 0.5, 0.5);
		gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
		gtk_tree_path_free (path);
	}
}

static void
servlist_channels_populate (ircnet *net, GtkWidget *treeview)
{
	GtkListStore *store;
	GtkTreeIter iter;
	int i;
	favchannel *favchan;
	GSList *list = net->favchanlist;

	store = (GtkListStore *)gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	gtk_list_store_clear (store);

	i = 0;
	while (list)
	{
		favchan = list->data;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, favchan->name, 1, favchan->key, 2, TRUE, -1);

		if (net->selected == i)
		{
			/* select this server */
			servlist_select_and_show (GTK_TREE_VIEW (treeview), &iter, store);
		}

		i++;
		list = list->next;
	}
}

static void
servlist_servers_populate (ircnet *net, GtkWidget *treeview)
{
	GtkListStore *store;
	GtkTreeIter iter;
	int i;
	ircserver *serv;
	GSList *list = net->servlist;

	store = (GtkListStore *)gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	gtk_list_store_clear (store);

	i = 0;
	while (list)
	{
		serv = list->data;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, serv->hostname, 1, 1, -1);

		if (net->selected == i)
		{
			/* select this server */
			servlist_select_and_show (GTK_TREE_VIEW (treeview), &iter, store);
		}

		i++;
		list = list->next;
	}
}

static void
servlist_commands_populate (ircnet *net, GtkWidget *treeview)
{
	GtkListStore *store;
	GtkTreeIter iter;
	int i;
	commandentry *entry;
	GSList *list = net->commandlist;

	store = (GtkListStore *)gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	gtk_list_store_clear (store);

	i = 0;
	while (list)
	{
		entry = list->data;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, entry->command, 1, 1, -1);

		if (net->selected == i)
		{
			/* select this server */
			servlist_select_and_show (GTK_TREE_VIEW (treeview), &iter, store);
		}

		i++;
		list = list->next;
	}
}

static void
servlist_networks_populate_ (GtkWidget *treeview, GSList *netlist, gboolean favorites)
{
	GtkListStore *store;
	GtkTreeIter iter;
	int i;
	ircnet *net;

	if (!netlist)
	{
		net = servlist_net_add (_("New Network"), "", FALSE);
		servlist_server_add (net, DEFAULT_SERVER);
		netlist = network_list;
	}
	store = (GtkListStore *)gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	gtk_list_store_clear (store);

	i = 0;
	while (netlist)
	{
		net = netlist->data;
		if (!favorites || (net->flags & FLAG_FAVORITE))
		{
			if (favorites)
				gtk_list_store_insert_with_values (store, &iter, 0x7fffffff, 0, net->name, 1, 1, 2, 400, -1);
			else
				gtk_list_store_insert_with_values (store, &iter, 0x7fffffff, 0, net->name, 1, 1, 2, (net->flags & FLAG_FAVORITE) ? 800 : 400, -1);
			if (i == prefs.hex_gui_slist_select)
			{
				/* select this network */
				servlist_select_and_show (GTK_TREE_VIEW (treeview), &iter, store);
				selected_net = net;
			}
		}
		i++;
		netlist = netlist->next;
	}
}

static void
servlist_networks_populate (GtkWidget *treeview, GSList *netlist)
{
	servlist_networks_populate_ (treeview, netlist, prefs.hex_gui_slist_fav);
}

static void
servlist_server_row_cb (GtkTreeSelection *sel, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	ircserver *serv;
	char *servname;
	int pos;

	if (!selected_net)
		return;

	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 0, &servname, -1);
		serv = servlist_server_find (selected_net, servname, &pos);
		g_free (servname);
		if (serv)
			selected_net->selected = pos;
		selected_serv = serv;
	}
}

static void
servlist_command_row_cb (GtkTreeSelection *sel, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	commandentry *cmd;
	char *cmdname;
	int pos;

	if (!selected_net)
		return;

	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 0, &cmdname, -1);
		cmd = servlist_command_find (selected_net, cmdname, &pos);
		g_free (cmdname);
		if (cmd)
			selected_net->selected = pos;
		selected_cmd = cmd;
	}
}

static void
servlist_channel_row_cb (GtkTreeSelection *sel, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	favchannel *channel;
	char *channame;
	int pos;

	if (!selected_net)
		return;

	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 0, &channame, -1);
		channel = servlist_favchan_find (selected_net, channame, &pos);
		g_free (channame);
		if (channel)
			selected_net->selected = pos;
		selected_chan = channel;
	}
}

static void
servlist_start_editing (GtkTreeView *tree)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;

	sel = gtk_tree_view_get_selection (tree);

	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
		if (path)
		{
			gtk_tree_view_set_cursor (tree, path,
									gtk_tree_view_get_column (tree, 0), TRUE);
			gtk_tree_path_free (path);
		}
	}
}

static void
servlist_addserver (void)
{
	GtkTreeIter iter;
	GtkListStore *store;

	if (!selected_net)
		return;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (edit_trees[SERVER_TREE])));
	servlist_server_add (selected_net, DEFAULT_SERVER);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, DEFAULT_SERVER, 1, TRUE, -1);

	/* select this server */
	servlist_select_and_show (GTK_TREE_VIEW (edit_trees[SERVER_TREE]), &iter, store);
	servlist_start_editing (GTK_TREE_VIEW (edit_trees[SERVER_TREE]));

	servlist_server_row_cb (gtk_tree_view_get_selection (GTK_TREE_VIEW (networks_tree)), NULL);
}

static void
servlist_addcommand (void)
{
	GtkTreeIter iter;
	GtkListStore *store;

	if (!selected_net)
		return;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (edit_trees[CMD_TREE])));
	servlist_command_add (selected_net, "ECHO hello");

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, "ECHO hello", 1, TRUE, -1);

	servlist_select_and_show (GTK_TREE_VIEW (edit_trees[CMD_TREE]), &iter, store);
	servlist_start_editing (GTK_TREE_VIEW (edit_trees[CMD_TREE]));

	servlist_command_row_cb (gtk_tree_view_get_selection (GTK_TREE_VIEW (networks_tree)), NULL);
}

static void
servlist_addchannel (void)
{
	GtkTreeIter iter;
	GtkListStore *store;

	if (!selected_net)
		return;

	store = GTK_LIST_STORE(gtk_tree_view_get_model (GTK_TREE_VIEW (edit_trees[CHANNEL_TREE])));
	servlist_favchan_add (selected_net, "#channel");

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, "#channel", 1, "", 2, TRUE, -1);

	/* select this server */
	servlist_select_and_show (GTK_TREE_VIEW (edit_trees[CHANNEL_TREE]), &iter, store);
	servlist_start_editing (GTK_TREE_VIEW (edit_trees[CHANNEL_TREE]));

	servlist_channel_row_cb (gtk_tree_view_get_selection (GTK_TREE_VIEW (networks_tree)), NULL);
}

static void
servlist_addnet_cb (GtkWidget *item, GtkTreeView *treeview)
{
	GtkTreeIter iter;
	GtkListStore *store;
	ircnet *net;

	net = servlist_net_add (_("New Network"), "", TRUE);
	net->encoding = g_strdup (IRC_DEFAULT_CHARSET);
	servlist_server_add (net, DEFAULT_SERVER);

	store = (GtkListStore *)gtk_tree_view_get_model (treeview);
	gtk_list_store_prepend (store, &iter);
	gtk_list_store_set (store, &iter, 0, net->name, 1, 1, -1);

	/* select this network */
	servlist_select_and_show (GTK_TREE_VIEW (networks_tree), &iter, store);
	servlist_start_editing (GTK_TREE_VIEW (networks_tree));

	servlist_network_row_cb (gtk_tree_view_get_selection (GTK_TREE_VIEW (networks_tree)), NULL);
}

static void
servlist_deletenetwork (ircnet *net)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* remove from GUI */
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (networks_tree));
	if (gtk_tree_selection_get_selected (sel, &model, &iter))
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	/* remove from list */
	servlist_net_remove (net);

	/* force something to be selected */
	gtk_tree_model_get_iter_first (model, &iter);
	servlist_select_and_show (GTK_TREE_VIEW (networks_tree), &iter,
									  GTK_LIST_STORE (model));
	servlist_network_row_cb (sel, NULL);
}

static void
servlist_deletenetdialog_cb (GtkDialog *dialog, gint arg1, ircnet *net)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
	if (arg1 == GTK_RESPONSE_OK)
		servlist_deletenetwork (net);
}

static GSList *
servlist_move_item (GtkTreeView *view, GSList *list, gpointer item, int delta)
{
	GtkTreeModel *store;
	GtkTreeIter iter1, iter2;
	GtkTreeSelection *sel;
	GtkTreePath *path;
	int pos;

	/* Keep tree in sync w/ list, there has to be an easier way to get iters */
	sel = gtk_tree_view_get_selection (view);
	gtk_tree_selection_get_selected (sel, &store, &iter1);
	path = gtk_tree_model_get_path (store, &iter1);
	if (delta == 1)
		gtk_tree_path_next (path);
	else
		gtk_tree_path_prev (path);
	gtk_tree_model_get_iter (store, &iter2, path);
	gtk_tree_path_free (path);
	
	pos = g_slist_index (list, item);
	if (pos >= 0)
	{
		pos += delta;
		if (pos >= 0)
		{
			list = g_slist_remove (list, item);
			list = g_slist_insert (list, item, pos);

			gtk_list_store_swap (GTK_LIST_STORE (store), &iter1, &iter2);
		}
	}
	
	return list;
}

static gboolean
servlist_net_keypress_cb (GtkWidget *wid, GdkEventKey *evt, gpointer tree)
{
	gboolean handled = FALSE;
	
	if (!selected_net || prefs.hex_gui_slist_fav)
		return FALSE;

	if (evt->state & STATE_SHIFT)
	{
		if (evt->keyval == GDK_KEY_Up)
		{
			handled = TRUE;
			network_list = servlist_move_item (GTK_TREE_VIEW (tree), network_list, selected_net, -1);
		}
		else if (evt->keyval == GDK_KEY_Down)
		{
			handled = TRUE;
			network_list = servlist_move_item (GTK_TREE_VIEW (tree), network_list, selected_net, +1);
		}
	}

	return handled;
}

static gint
servlist_compare (ircnet *net1, ircnet *net2)
{
	gchar *net1_casefolded, *net2_casefolded;
	int result=0;

	net1_casefolded=g_utf8_casefold(net1->name,-1),
	net2_casefolded=g_utf8_casefold(net2->name,-1),

	result=g_utf8_collate(net1_casefolded,net2_casefolded);

	g_free(net1_casefolded);
	g_free(net2_casefolded);

	return result;

}

static void
servlist_sort (GtkWidget *button, gpointer none)
{
	network_list=g_slist_sort(network_list,(GCompareFunc)servlist_compare);
	servlist_networks_populate (networks_tree, network_list);
}

static gboolean
servlist_has_selection (GtkTreeView *tree)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* make sure something is selected */
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	return gtk_tree_selection_get_selected (sel, &model, &iter);
}

static void
servlist_favor (GtkWidget *button, gpointer none)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (!selected_net)
		return;

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (networks_tree));
	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		if (selected_net->flags & FLAG_FAVORITE)
		{
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, 2, 400, -1);
			selected_net->flags &= ~FLAG_FAVORITE;
		}
		else
		{
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, 2, 800, -1);
			selected_net->flags |= FLAG_FAVORITE;
		}
	}
}

static void
servlist_update_from_entry (char **str, GtkWidget *entry)
{
	g_free (*str);

	if (gtk_entry_get_text (GTK_ENTRY (entry))[0] == 0)
		*str = NULL;
	else
		*str = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
}

static char *
servlist_edit_current_password (ircnet *net)
{
	if (!net)
		return NULL;
	if (net->flags & FLAG_USE_KEYRING)
		return secretstore_get_network_password (net->name);
	return servlist_password_decrypt_for_storage (net->pass);
}

static void
servlist_edit_update (ircnet *net)
{
	gboolean use_keyring;
	gboolean ask_pass;
	gboolean keyring_changed;
	char *password = NULL;
	servlist_update_from_entry (&net->nick, edit_entry_nick);
	servlist_update_from_entry (&net->nick2, edit_entry_nick2);
	servlist_update_from_entry (&net->user, edit_entry_user);
	servlist_update_from_entry (&net->real, edit_entry_real);
	if (net && net->name)
	{
		ask_pass = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (edit_check_ask_pass));
		use_keyring = !ask_pass && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (edit_check_use_keyring));
		keyring_changed = !!(net->flags & FLAG_USE_KEYRING) != !!use_keyring;
		if (ask_pass)
		{
			secretstore_delete_network_password (net->name);
			if (net->pass)
			{
				memset (net->pass, 0, strlen (net->pass));
				g_free (net->pass);
				net->pass = NULL;
			}
			net->flags &= ~FLAG_USE_KEYRING;
			net->flags |= FLAG_PROMPT_PASSWORD;
			return;
		}
		net->flags &= ~FLAG_PROMPT_PASSWORD;
		if (!edit_pass_changed && !keyring_changed)
			return;
		if (edit_pass_changed)
			password = g_strdup (gtk_entry_get_text (GTK_ENTRY (edit_entry_pass)));
		else
			password = servlist_edit_current_password (net);
		if (use_keyring)
		{
			if (password && *password)
			{
				if (!secretstore_set_network_password (net->name, password))
				{
					fe_message (_("No system keyring is available. ZoiteChat can save this password using local encrypted fallback storage, but it is less protected than your desktop keyring."), FE_MSG_WARN);
					memset (password, 0, strlen (password));
					g_free (password);
					return;
				}
			}
			else
				secretstore_delete_network_password (net->name);
			net->flags |= FLAG_USE_KEYRING;
			g_free (net->pass);
			net->pass = NULL;
		}
		else
		{
			char *enc = NULL;
			if (password && *password)
			{
				enc = servlist_password_encrypt_for_storage (password);
				if (!enc)
				{
					fe_message (_("Could not encrypt this password."), FE_MSG_WARN);
					memset (password, 0, strlen (password));
					g_free (password);
					return;
				}
			}
			secretstore_delete_network_password (net->name);
			net->flags &= ~FLAG_USE_KEYRING;
			g_free (net->pass);
			net->pass = enc;
		}
		if (password)
		{
			memset (password, 0, strlen (password));
			g_free (password);
		}
	}
}

static void
servlist_edit_close_cb (GtkWidget *button, gpointer userdata)
{
	if (selected_net)
		servlist_edit_update (selected_net);
	if (edit_loaded_password)
	{
		memset (edit_loaded_password, 0, strlen (edit_loaded_password));
		g_free (edit_loaded_password);
		edit_loaded_password = NULL;
	}

	gtk_widget_destroy (edit_win);
	edit_win = NULL;
	edit_entry_pass = NULL;
	edit_check_show_pass = NULL;
	edit_check_ask_pass = NULL;
	edit_button_encrypt_pass = NULL;
	edit_button_import_pass = NULL;
}

static gint
servlist_editwin_delete_cb (GtkWidget *win, GdkEventAny *event, gpointer none)
{
	servlist_edit_close_cb (NULL, NULL);
	return FALSE;
}

static gboolean
servlist_configure_cb (GtkWindow *win, GdkEventConfigure *event, gpointer none)
{
	/* remember the window size */
	gtk_window_get_size (win, &netlist_win_width, &netlist_win_height);
	return FALSE;
}

static gboolean
servlist_edit_configure_cb (GtkWindow *win, GdkEventConfigure *event, gpointer none)
{
	/* remember the window size */
	gtk_window_get_size (win, &netedit_win_width, &netedit_win_height);
	return FALSE;
}

static void
servlist_edit_cb (GtkWidget *but, gpointer none)
{
	if (!servlist_has_selection (GTK_TREE_VIEW (networks_tree)))
		return;
	if (!selected_net || !selected_net->name)
		return;
	if ((selected_net->flags & FLAG_USE_KEYRING) && !secretstore_require_unlock (selected_net->name))
		return;

	edit_win = servlist_open_edit (serverlist_win, selected_net);
	gtkutil_set_icon (edit_win);
	servlist_servers_populate (selected_net, edit_trees[SERVER_TREE]);
	servlist_channels_populate (selected_net, edit_trees[CHANNEL_TREE]);
	servlist_commands_populate (selected_net, edit_trees[CMD_TREE]);
	g_signal_connect (G_OBJECT (edit_win), "delete-event",
						 	G_CALLBACK (servlist_editwin_delete_cb), 0);
	g_signal_connect (G_OBJECT (edit_win), "configure-event",
							G_CALLBACK (servlist_edit_configure_cb), 0);
	gtk_widget_show (edit_win);
}

static void
servlist_deletenet_cb (GtkWidget *item, ircnet *net)
{
	GtkWidget *dialog;

	if (!servlist_has_selection (GTK_TREE_VIEW (networks_tree)))
		return;

	net = selected_net;
	if (!net)
		return;
	dialog = gtk_message_dialog_new (GTK_WINDOW (serverlist_win),
												GTK_DIALOG_DESTROY_WITH_PARENT |
												GTK_DIALOG_MODAL,
												GTK_MESSAGE_QUESTION,
												GTK_BUTTONS_OK_CANCEL,
							_("Really remove network \"%s\" and all its servers?"),
												net->name);
	theme_manager_attach_window (dialog);
	g_signal_connect (dialog, "response",
							G_CALLBACK (servlist_deletenetdialog_cb), net);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_widget_show (dialog);
}

static void
servlist_deleteserver (ircserver *serv, GtkTreeModel *model)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;

	/* don't remove the last server */
	if (selected_net && g_slist_length (selected_net->servlist) < 2)
		return;

	/* remove from GUI */
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (edit_trees[SERVER_TREE]));
	if (gtk_tree_selection_get_selected (sel, &model, &iter))
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	/* remove from list */
	if (selected_net)
		servlist_server_remove (selected_net, serv);
}

static void
servlist_editbutton_cb (GtkWidget *item, GtkNotebook *notebook)
{
	servlist_start_editing (GTK_TREE_VIEW (edit_trees[gtk_notebook_get_current_page(notebook)]));
}

static void
servlist_deleteserver_cb (void)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *servname;
	ircserver *serv;
	int pos;

	/* find the selected item in the GUI */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (edit_trees[SERVER_TREE]));
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (edit_trees[SERVER_TREE]));

	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 0, &servname, -1);
		serv = servlist_server_find (selected_net, servname, &pos);
		g_free (servname);
		if (serv)
		{
			servlist_deleteserver (serv, model);
		}
	}
}

static void
servlist_deletecommand (commandentry *entry, GtkTreeModel *model)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;

	/* remove from GUI */
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (edit_trees[CMD_TREE]));
	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}

	/* remove from list */
	if (selected_net)
	{
		servlist_command_remove (selected_net, entry);
	}
}

static void
servlist_deletecommand_cb (void)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *command;
	commandentry *entry;
	int pos;

	/* find the selected item in the GUI */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (edit_trees[CMD_TREE]));
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (edit_trees[CMD_TREE]));

	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 0, &command, -1);			/* query the content of the selection */
		entry = servlist_command_find (selected_net, command, &pos);
		g_free (command);
		if (entry)
		{
			servlist_deletecommand (entry, model);
		}
	}
}

static void
servlist_deletechannel (favchannel *favchan, GtkTreeModel *model)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;

	/* remove from GUI */
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (edit_trees[CHANNEL_TREE]));
	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}

	/* remove from list */
	if (selected_net)
	{
		servlist_favchan_remove (selected_net, favchan);
	}
}

static void
servlist_deletechannel_cb (void)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *name;
	char *key;
	favchannel *favchan;
	int pos;

	/* find the selected item in the GUI */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (edit_trees[CHANNEL_TREE]));
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (edit_trees[CHANNEL_TREE]));

	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 0, &name, 1, &key, -1);			/* query the content of the selection */
		favchan = servlist_favchan_find (selected_net, name, &pos);
		g_free (name);
		if (favchan)
		{
			servlist_deletechannel (favchan, model);
		}
	}
}

static ircnet *
servlist_find_selected_net (GtkTreeSelection *sel)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *netname;
	int pos;
	ircnet *net = NULL;

	if (gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 0, &netname, -1);
		net = servlist_net_find (netname, &pos, strcmp);
		g_free (netname);
		if (net)
			prefs.hex_gui_slist_select = pos;
	}

	return net;
}

static void
servlist_network_row_cb (GtkTreeSelection *sel, gpointer user_data)
{
	ircnet *net;

	selected_net = NULL;

	net = servlist_find_selected_net (sel);
	if (net)
		selected_net = net;
}

static int
servlist_savegui (void)
{
	char *sp;
	const char *nick1, *nick2;

	/* check for blank username, ircd will not allow this */
	if (gtk_entry_get_text (GTK_ENTRY (entry_guser))[0] == 0)
		return 1;

	/* if (gtk_entry_get_text (GTK_ENTRY (entry_greal))[0] == 0)
		return 1; */

	nick1 = gtk_entry_get_text (GTK_ENTRY (entry_nick1));
	nick2 = gtk_entry_get_text (GTK_ENTRY (entry_nick2));

	/* ensure unique nicknames */
	if (!rfc_casecmp (nick1, nick2))
		return 2;

	safe_strcpy (prefs.hex_irc_nick1, nick1, sizeof(prefs.hex_irc_nick1));
	safe_strcpy (prefs.hex_irc_nick2, nick2, sizeof(prefs.hex_irc_nick2));
	safe_strcpy (prefs.hex_irc_nick3, gtk_entry_get_text (GTK_ENTRY (entry_nick3)), sizeof(prefs.hex_irc_nick3));
	safe_strcpy (prefs.hex_irc_user_name, gtk_entry_get_text (GTK_ENTRY (entry_guser)), sizeof(prefs.hex_irc_user_name));
	sp = strchr (prefs.hex_irc_user_name, ' ');
	if (sp)
		sp[0] = 0;	/* spaces will break the login */
	/* strcpy (prefs.hex_irc_real_name, gtk_entry_get_text (GTK_ENTRY (entry_greal))); */
	servlist_save ();
	if (!save_config ())
		fe_message (_("Could not save zoitechat.conf."), FE_MSG_WARN);

	return 0;
}

static gboolean
servlist_get_iter_from_name (GtkTreeModel *model, gchar *name, GtkTreeIter *iter)
{
	GtkTreePath *path = gtk_tree_path_new_from_string (name);

	if (!gtk_tree_model_get_iter (model, iter, path))
	{
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_path_free (path);
	return TRUE;
}

static void
servlist_addbutton_cb (GtkWidget *item, GtkNotebook *notebook)
{
		switch (gtk_notebook_get_current_page (notebook))
		{
				case SERVER_TREE:
						servlist_addserver ();
						break;
				case CHANNEL_TREE:
						servlist_addchannel ();
						break;
				case CMD_TREE:
						servlist_addcommand ();
						break;
				default:
						break;
		}
}

static void
servlist_deletebutton_cb (GtkWidget *item, GtkNotebook *notebook)
{
		switch (gtk_notebook_get_current_page (notebook))
		{
				case SERVER_TREE:
						servlist_deleteserver_cb ();
						break;
				case CHANNEL_TREE:
						servlist_deletechannel_cb ();
						break;
				case CMD_TREE:
						servlist_deletecommand_cb ();
						break;
				default:
						break;
		}
}

static gboolean
servlist_keypress_cb (GtkWidget *wid, GdkEventKey *evt, GtkNotebook *notebook)
{
	gboolean handled = FALSE;
	int delta = 0;
	
	if (!selected_net)
		return FALSE;

	if (evt->state & STATE_SHIFT)
	{
		if (evt->keyval == GDK_KEY_Up)
		{
			handled = TRUE;
			delta = -1;
		}
		else if (evt->keyval == GDK_KEY_Down)
		{
			handled = TRUE;
			delta = +1;
		}
	}
	
	if (handled)
	{
		switch (gtk_notebook_get_current_page (notebook))
		{
			case SERVER_TREE:
				if (selected_serv)
					selected_net->servlist = servlist_move_item (GTK_TREE_VIEW (edit_trees[SERVER_TREE]), 
																selected_net->servlist, selected_serv, delta);
				break;
			case CHANNEL_TREE:
				if (selected_chan)
					selected_net->favchanlist = servlist_move_item (GTK_TREE_VIEW (edit_trees[CHANNEL_TREE]), 
																	selected_net->favchanlist, selected_chan, delta);
				break;
			case CMD_TREE:
				if (selected_cmd)
					selected_net->commandlist = servlist_move_item (GTK_TREE_VIEW (edit_trees[CMD_TREE]), 
																	selected_net->commandlist, selected_cmd, delta);
				break;
		}
	}
	
	return handled;
}

void
servlist_autojoinedit (ircnet *net, char *channel, gboolean add)
{
	favchannel *fav;

	if (add)
	{
		servlist_favchan_add (net, channel);
		servlist_save ();
	}
	else
	{
		fav = servlist_favchan_find (net, channel, NULL);
		if (fav)
		{
			servlist_favchan_remove (net, fav);
			servlist_save ();
		}
	}
}

static void
servlist_toggle_global_user (gboolean sensitive)
{
	gtk_widget_set_sensitive (edit_entry_nick, sensitive);
	gtk_widget_set_sensitive (edit_label_nick, sensitive);

	gtk_widget_set_sensitive (edit_entry_nick2, sensitive);
	gtk_widget_set_sensitive (edit_label_nick2, sensitive);

	gtk_widget_set_sensitive (edit_entry_user, sensitive);
	gtk_widget_set_sensitive (edit_label_user, sensitive);

	gtk_widget_set_sensitive (edit_entry_real, sensitive);
	gtk_widget_set_sensitive (edit_label_real, sensitive);
}

static void
servlist_connect_cb (GtkWidget *button, gpointer userdata)
{
	int servlist_err;

	if (!selected_net)
		return;

	servlist_err = servlist_savegui ();
	if (servlist_err == 1)
	{
		fe_message (_("User name cannot be left blank."), FE_MSG_ERROR);
		return;
	}

 	if (!is_session (servlist_sess))
		servlist_sess = NULL;	/* open a new one */

	{
		GSList *list;
		session *sess;
		session *chosen = servlist_sess;

		servlist_sess = NULL;	/* open a new one */

		for (list = sess_list; list; list = list->next)
		{
			sess = list->data;
			if (sess->server->network == selected_net)
			{
				servlist_sess = sess;
				if (sess->server->connected)
					servlist_sess = NULL;	/* open a new one */
				break;
			}
		}

		/* use the chosen one, if it's empty */
		if (!servlist_sess &&
			  chosen &&
			 !chosen->server->connected &&
			  chosen->server->server_session->channel[0] == 0)
		{
			servlist_sess = chosen;
		}
	}

	servlist_connect (servlist_sess, selected_net, TRUE);

	gtk_widget_destroy (serverlist_win);
	serverlist_win = NULL;
	selected_net = NULL;
}

static void
servlist_celledit_cb (GtkCellRendererText *cell, gchar *arg1, gchar *arg2,
							 gpointer user_data)
{
	GtkTreeModel *model = (GtkTreeModel *)user_data;
	GtkTreeIter iter;
	GtkTreePath *path;
	char *netname;
	ircnet *net;

	if (!arg1 || !arg2)
		return;

	path = gtk_tree_path_new_from_string (arg1);
	if (!path)
		return;

	if (!gtk_tree_model_get_iter (model, &iter, path))
	{
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_model_get (model, &iter, 0, &netname, -1);

	net = servlist_net_find (netname, NULL, strcmp);
	g_free (netname);
	if (net)
	{
		/* delete empty item */
		if (arg2[0] == 0)
		{
			servlist_deletenetwork (net);
			gtk_tree_path_free (path);
			return;
		}

		netname = net->name;
		net->name = g_strdup (arg2);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, net->name, -1);
		g_free (netname);
	}

	gtk_tree_path_free (path);
}

static void
servlist_check_cb (GtkWidget *but, gpointer num_p)
{
	int num = GPOINTER_TO_INT (num_p);

	if (!selected_net)
		return;

	if ((1 << num) == FLAG_CYCLE || (1 << num) == FLAG_USE_PROXY)
	{
		/* these ones are reversed, so it's compat with 2.0.x */
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (but)))
			selected_net->flags &= ~(1 << num);
		else
			selected_net->flags |= (1 << num);
	} else
	{
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (but)))
			selected_net->flags |= (1 << num);
		else
			selected_net->flags &= ~(1 << num);
	}

	if ((1 << num) == FLAG_USE_GLOBAL)
	{
		servlist_toggle_global_user (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (but)));
	}
}

typedef enum
{
	SERVLIST_ALIGN_START,
	SERVLIST_ALIGN_CENTER,
	SERVLIST_ALIGN_FILL
} servlist_align;

static GtkAlign
servlist_align_to_gtk (servlist_align align)
{
	switch (align)
	{
	case SERVLIST_ALIGN_FILL:
		return GTK_ALIGN_FILL;
	case SERVLIST_ALIGN_CENTER:
		return GTK_ALIGN_CENTER;
	case SERVLIST_ALIGN_START:
	default:
		return GTK_ALIGN_START;
	}
}

static void
servlist_table_attach (GtkWidget *table, GtkWidget *child,
					   guint left_attach, guint right_attach,
					   guint top_attach, guint bottom_attach,
					   gboolean hexpand, gboolean vexpand,
					   servlist_align halign, servlist_align valign,
					   guint xpad, guint ypad)
{
	gtk_widget_set_hexpand (child, hexpand);
	gtk_widget_set_vexpand (child, vexpand);
	gtk_widget_set_halign (child, servlist_align_to_gtk (halign));
	gtk_widget_set_valign (child, servlist_align_to_gtk (valign));
	gtk_widget_set_margin_start (child, xpad);
	gtk_widget_set_margin_end (child, xpad);
	gtk_widget_set_margin_top (child, ypad);
	gtk_widget_set_margin_bottom (child, ypad);
	gtk_grid_attach (GTK_GRID (table), child, left_attach, top_attach,
					 right_attach - left_attach, bottom_attach - top_attach);
}

static GtkWidget *
servlist_create_check (int num, int state, GtkWidget *table, int row, int col, char *labeltext)
{
	GtkWidget *but;

	but = gtk_check_button_new_with_label (labeltext);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (but), state);
	g_signal_connect (G_OBJECT (but), "toggled",
							G_CALLBACK (servlist_check_cb), GINT_TO_POINTER (num));
	servlist_table_attach (table, but, col, col + 2, row, row + 1,
						   TRUE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_CENTER,
						   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);
	gtk_widget_show (but);

	return but;
}

static GtkWidget *
servlist_create_entry (GtkWidget *table, char *labeltext, int row,
							  char *def, GtkWidget **label_ret, char *tip)
{
	GtkWidget *label, *entry;

	label = gtk_label_new_with_mnemonic (labeltext);
	if (label_ret)
		*label_ret = label;
	gtk_widget_show (label);
	servlist_table_attach (table, label, 0, 1, row, row + 1,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

	entry = gtk_entry_new ();
	gtk_widget_set_tooltip_text (entry, tip);
	gtk_widget_show (entry);
	gtk_entry_set_text (GTK_ENTRY (entry), def ? def : "");
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

	servlist_table_attach (table, entry, 1, 2, row, row + 1,
						   TRUE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_CENTER,
						   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);

	return entry;
}

static gint
servlist_delete_cb (GtkWidget *win, GdkEventAny *event, gpointer userdata)
{
	servlist_savegui ();
	serverlist_win = NULL;
	selected_net = NULL;

	if (sess_list == NULL)
		zoitechat_exit ();

	return FALSE;
}

static void
servlist_close_cb (GtkWidget *button, gpointer userdata)
{
	servlist_savegui ();
	gtk_widget_destroy (serverlist_win);
	serverlist_win = NULL;
	selected_net = NULL;

	if (sess_list == NULL)
		zoitechat_exit ();
}

/* convert "host:port" format to "host/port" */

static char *
servlist_sanitize_hostname (char *host)
{
	char *ret, *c, *e;

	ret = g_strdup (host);

	c = strchr  (ret, ':');
	e = strrchr (ret, ':');

	/* if only one colon exists it's probably not IPv6 */
	if (c && c == e)
		*c = '/';

	return g_strstrip(ret);
}

/* remove leading slash */
static char *
servlist_sanitize_command (char *cmd)
{
	if (cmd[0] == '/')
	{
		return (g_strdup (cmd + 1));
	}
	else
	{
		return (g_strdup (cmd));
	}
}

static void
servlist_editserver_cb (GtkCellRendererText *cell, gchar *name, gchar *newval, gpointer user_data)
{
	GtkTreeModel *model = (GtkTreeModel *)user_data;
	GtkTreeIter iter;
	char *servname;
	ircserver *serv;

	if (!selected_net)
	{
		return;
	}

	if (!servlist_get_iter_from_name (model, name, &iter))
	{
		return;
	}

	gtk_tree_model_get (model, &iter, 0, &servname, -1);
	serv = servlist_server_find (selected_net, servname, NULL);
	g_free (servname);

	if (serv)
	{
		/* delete empty item */
		if (newval[0] == 0)
		{
			servlist_deleteserver (serv, model);
			return;
		}

		servname = serv->hostname;
		serv->hostname = servlist_sanitize_hostname (newval);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, serv->hostname, -1);
		g_free (servname);
	}
}

static void
servlist_editcommand_cb (GtkCellRendererText *cell, gchar *name, gchar *newval, gpointer user_data)
{
	GtkTreeModel *model = (GtkTreeModel *)user_data;
	GtkTreeIter iter;
	char *cmd;
	commandentry *entry;

	if (!selected_net)
	{
		return;
	}

	if (!servlist_get_iter_from_name (model, name, &iter))
	{
		return;
	}

	gtk_tree_model_get (model, &iter, 0, &cmd, -1);
	entry = servlist_command_find (selected_net, cmd, NULL);
	g_free (cmd);

	if (entry)
	{
		/* delete empty item */
		if (newval[0] == 0)
		{
			servlist_deletecommand (entry, model);
			return;
		}

		cmd = entry->command;
		entry->command = servlist_sanitize_command (newval);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, entry->command, -1);
		g_free (cmd);
	}
}

static void
servlist_editchannel_cb (GtkCellRendererText *cell, gchar *name, gchar *newval, gpointer user_data)
{
	GtkTreeModel *model = (GtkTreeModel *)user_data;
	GtkTreeIter iter;
	char *chan;
	char *key;
	favchannel *favchan;

	if (!selected_net)
	{
		return;
	}

	if (!servlist_get_iter_from_name (model, name, &iter))
	{
		return;
	}

	gtk_tree_model_get (model, &iter, 0, &chan, 1, &key, -1);
	favchan = servlist_favchan_find (selected_net, chan, NULL);
	g_free (chan);

	if (favchan)
	{
		/* delete empty item */
		if (newval[0] == 0)
		{
			servlist_deletechannel (favchan, model);
			return;
		}

		chan = favchan->name;
		favchan->name = g_strdup (newval);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, favchan->name, -1);
		g_free (chan);
	}
}

static void
servlist_editkey_cb (GtkCellRendererText *cell, gchar *name, gchar *newval, gpointer user_data)
{
	GtkTreeModel *model = (GtkTreeModel *)user_data;
	GtkTreeIter iter;
	char *chan;
	char *key;
	favchannel *favchan;

	if (!selected_net)
	{
		return;
	}

	if (!servlist_get_iter_from_name (model, name, &iter))
	{
		return;
	}

	gtk_tree_model_get (model, &iter, 0, &chan, 1, &key, -1);
	favchan = servlist_favchan_find (selected_net, chan, NULL);
	g_free (chan);

	if (favchan)
	{
		key = favchan->key;

		if (strlen (newval))	/* check key length, the field can be empty in order to delete the key! */
		{
			favchan->key = g_strdup (newval);
		}
		else					/* if key's empty, make sure we actually remove the key */
		{
			favchan->key = NULL;
		}

		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, favchan->key, -1);
		g_free (key);
	}
}

static gboolean
servlist_edit_tabswitch_cb (GtkNotebook *nb, gpointer *newtab, guint newindex, gpointer user_data)
{
	/* remember the active tab */
	netedit_active_tab = newindex;

	return FALSE;
}

static void
servlist_combo_cb (GtkEntry *entry, gpointer userdata)
{
	if (!selected_net)
		return;

	g_free (selected_net->encoding);
	selected_net->encoding = g_strdup (gtk_entry_get_text (entry));
}

/* Fills up the network's authentication type so that it's guaranteed to be either NULL or a valid value. */
static void
servlist_logintypecombo_cb (GtkComboBox *cb, gpointer *userdata)
{
	int index;

	if (!selected_net)
	{
		return;
	}

	index = gtk_combo_box_get_active (cb);	/* starts at 0, returns -1 for invalid selections */

	if (index == -1)
		return; /* Invalid */

	/* The selection is valid. It can be 0, which is the default type, but we need to allow
	 * that so that you can revert from other types. servlist_save() will dump 0 anyway.
	 */
	selected_net->logintype = login_types_conf[index];

	if (login_types_conf[index] == LOGIN_CUSTOM)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (userdata), 2);		/* FIXME avoid hardcoding? */
	}
	
	/* EXTERNAL uses a cert, not a pass */
	if (login_types_conf[index] == LOGIN_SASLEXTERNAL)
		gtk_widget_set_sensitive (edit_entry_pass, FALSE);
	else
		gtk_widget_set_sensitive (edit_entry_pass, TRUE);
}

static void
servlist_username_changed_cb (GtkEntry *entry, gpointer userdata)
{
	GtkWidget *connect_btn = GTK_WIDGET (userdata);

	if (gtk_entry_get_text (entry)[0] == 0)
	{
		gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, ICON_SERVLIST_ERROR);
		gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY,
										_("User name cannot be left blank."));
		gtk_widget_set_sensitive (connect_btn, FALSE);
	}
	else
	{
		gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
		gtk_widget_set_sensitive (connect_btn, TRUE);
	}
}

static void
servlist_nick_changed_cb (GtkEntry *entry, gpointer userdata)
{
	GtkWidget *connect_btn = GTK_WIDGET (userdata);
	const gchar *nick1 = gtk_entry_get_text (GTK_ENTRY (entry_nick1));
	const gchar *nick2 = gtk_entry_get_text (GTK_ENTRY (entry_nick2));

	if (!nick1[0] || !nick2[0])
	{
		entry = GTK_ENTRY(!nick1[0] ? entry_nick1 : entry_nick2);
		gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, ICON_SERVLIST_ERROR);
		gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY,
		                                 _("You cannot have an empty nick name."));
		gtk_widget_set_sensitive (connect_btn, FALSE);
	}
	else if (!rfc_casecmp (nick1, nick2))
	{
		gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, ICON_SERVLIST_ERROR);
		gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY,
										_("You must have two unique nick names."));
		gtk_widget_set_sensitive (connect_btn, FALSE);
	}
	else
	{
		gtk_entry_set_icon_from_icon_name (GTK_ENTRY(entry_nick1), GTK_ENTRY_ICON_SECONDARY, NULL);
		gtk_entry_set_icon_from_icon_name (GTK_ENTRY(entry_nick2), GTK_ENTRY_ICON_SECONDARY, NULL);
		gtk_widget_set_sensitive (connect_btn, TRUE);
	}
}

static GtkWidget *
servlist_create_charsetcombo (void)
{
	GtkWidget *cb;
	int i;

	cb = gtk_combo_box_text_new_with_entry ();
	i = 0;
	while (pages[i])
	{
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb), (char *)pages[i]);
		i++;
	}

	gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN(cb))), selected_net->encoding ? selected_net->encoding : pages[0]);
	
	g_signal_connect (G_OBJECT (gtk_bin_get_child (GTK_BIN (cb))), "changed",
							G_CALLBACK (servlist_combo_cb), NULL);

	return cb;
}

static GtkWidget *
servlist_create_logintypecombo (GtkWidget *data)
{
	GtkWidget *cb;
	int i;

	cb = gtk_combo_box_text_new ();

	i = 0;

	while (login_types[i])
	{
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb), (char *)login_types[i]);
		i++;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (cb), servlist_get_login_desc_index (selected_net->logintype));

	gtk_widget_set_tooltip_text (cb, _("The way you identify yourself to the server. For custom login methods use connect commands."));
	g_signal_connect (G_OBJECT (GTK_BIN (cb)), "changed", G_CALLBACK (servlist_logintypecombo_cb), data);

	return cb;
}

static void
no_servlist (GtkWidget * igad, gpointer serv)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (igad)))
		prefs.hex_gui_slist_skip = TRUE;
	else
		prefs.hex_gui_slist_skip = FALSE;
}

static void
fav_servlist (GtkWidget * igad, gpointer serv)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (igad)))
		prefs.hex_gui_slist_fav = TRUE;
	else
		prefs.hex_gui_slist_fav = FALSE;

	servlist_networks_populate (networks_tree, network_list);
}

static GtkWidget *
bold_label (char *text)
{
	char buf[128];
	GtkWidget *label;

	g_snprintf (buf, sizeof (buf), "<b>%s</b>", text);
	label = gtk_label_new (buf);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
	gtk_widget_show (label);

	return label;
}

static GtkWidget *
servlist_open_edit (GtkWidget *parent, ircnet *net)
{
	GtkWidget *editwindow;
	GtkWidget *vbox5;
	GtkWidget *table3;
	GtkWidget *label34;
	GtkWidget *label_logintype;
	GtkWidget *comboboxentry_charset;
	GtkWidget *combobox_logintypes;
	GtkWidget *hbox1;
	GtkWidget *scrolledwindow2;
	GtkWidget *scrolledwindow4;
	GtkWidget *scrolledwindow5;
	GtkWidget *treeview_servers;
	GtkWidget *treeview_channels;
	GtkWidget *treeview_commands;
	GtkWidget *vbuttonbox1;
	GtkWidget *buttonadd;
	GtkWidget *buttonremove;
	GtkWidget *buttonedit;
	GtkWidget *hbox_cert_buttons;
	GtkWidget *hseparator2;
	GtkWidget *hbuttonbox4;
	GtkWidget *button10;
	GtkWidget *check;
	GtkWidget *notebook;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	char buf[128];

	editwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	theme_manager_attach_window (editwindow);
	gtk_container_set_border_width (GTK_CONTAINER (editwindow), 4);
	g_snprintf (buf, sizeof (buf), _("Edit %s - %s"), net->name, _(DISPLAY_NAME));
	gtk_window_set_title (GTK_WINDOW (editwindow), buf);
	gtk_window_set_default_size (GTK_WINDOW (editwindow), netedit_win_width, netedit_win_height);
	gtk_window_set_transient_for (GTK_WINDOW (editwindow), GTK_WINDOW (parent));
	gtk_window_set_modal (GTK_WINDOW (editwindow), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (editwindow), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_role (GTK_WINDOW (editwindow), "editserv");

	vbox5 = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (editwindow), vbox5);


	/* Tabs and buttons */
	hbox1 = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox5), hbox1, TRUE, TRUE, 4);

	scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
	scrolledwindow4 = gtk_scrolled_window_new (NULL, NULL);
	scrolledwindow5 = gtk_scrolled_window_new (NULL, NULL);

	notebook = gtk_notebook_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), scrolledwindow2, gtk_label_new (_("Servers")));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), scrolledwindow4, gtk_label_new (_("Autojoin channels")));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), scrolledwindow5, gtk_label_new (_("Connect commands")));
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_BOTTOM);
	gtk_box_pack_start (GTK_BOX (hbox1), notebook, TRUE, TRUE, SERVLIST_X_PADDING);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_SHADOW_IN);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow4), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow4),	GTK_SHADOW_IN);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow5), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow5), GTK_SHADOW_IN);
	gtk_widget_set_tooltip_text (scrolledwindow5, _("%n=Nick name\n%p=Password\n%r=Real name\n%u=User name"));


	/* Server Tree */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	model = GTK_TREE_MODEL (store);

	edit_trees[SERVER_TREE] = treeview_servers = gtk_tree_view_new_with_model (model);
	g_signal_connect (G_OBJECT (treeview_servers), "key-press-event",
							G_CALLBACK (servlist_keypress_cb), notebook);
	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview_servers))),
							"changed", G_CALLBACK (servlist_server_row_cb), NULL);
	g_object_unref (model);
	gtk_container_add (GTK_CONTAINER (scrolledwindow2), treeview_servers);
	gtk_widget_set_size_request (treeview_servers, -1, 80);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview_servers),
												  FALSE);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
							G_CALLBACK (servlist_editserver_cb), model);
	gtk_tree_view_insert_column_with_attributes (
								GTK_TREE_VIEW (treeview_servers), -1,
						 		0, renderer,
						 		"text", 0,
								"editable", 1,
								NULL);

	/* Channel Tree */
	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	model = GTK_TREE_MODEL (store);

	edit_trees[CHANNEL_TREE] = treeview_channels = gtk_tree_view_new_with_model (model);
	g_signal_connect (G_OBJECT (treeview_channels), "key-press-event",
							G_CALLBACK (servlist_keypress_cb), notebook);
	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview_channels))),
							"changed", G_CALLBACK (servlist_channel_row_cb), NULL);
	g_object_unref (model);
	gtk_container_add (GTK_CONTAINER (scrolledwindow4), treeview_channels);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview_channels), TRUE);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
							G_CALLBACK (servlist_editchannel_cb), model);
	gtk_tree_view_insert_column_with_attributes (
								GTK_TREE_VIEW (treeview_channels), -1,
						 		_("Channel"), renderer,
						 		"text", 0,
								"editable", 2,
								NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
							G_CALLBACK (servlist_editkey_cb), model);
	gtk_tree_view_insert_column_with_attributes (
								GTK_TREE_VIEW (treeview_channels), -1,
						 		_("Key (Password)"), renderer,
						 		"text", 1,
								"editable", 2,
								NULL);

	gtk_tree_view_column_set_expand (gtk_tree_view_get_column (GTK_TREE_VIEW (treeview_channels), 0), TRUE);
	gtk_tree_view_column_set_expand (gtk_tree_view_get_column (GTK_TREE_VIEW (treeview_channels), 1), TRUE);


	/* Command Tree */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	model = GTK_TREE_MODEL (store);

	edit_trees[CMD_TREE] = treeview_commands = gtk_tree_view_new_with_model (model);
	g_signal_connect (G_OBJECT (treeview_commands), "key-press-event",
							G_CALLBACK (servlist_keypress_cb), notebook);
	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview_commands))),
							"changed", G_CALLBACK (servlist_command_row_cb), NULL);
	g_object_unref (model);
	gtk_container_add (GTK_CONTAINER (scrolledwindow5), treeview_commands);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview_commands),
												  FALSE);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
							G_CALLBACK (servlist_editcommand_cb), model);
	gtk_tree_view_insert_column_with_attributes (
								GTK_TREE_VIEW (treeview_commands), -1,
						 		0, renderer,
						 		"text", 0,
								"editable", 1,
								NULL);


	/* Button Box */
	vbuttonbox1 = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox1), 3);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox1), GTK_BUTTONBOX_START);
	gtk_box_pack_start (GTK_BOX (hbox1), vbuttonbox1, FALSE, FALSE, 3);

	buttonadd = servlist_icon_button_new (_("_Add"), ICON_SERVLIST_ADD);
	g_signal_connect (G_OBJECT (buttonadd), "clicked",
							G_CALLBACK (servlist_addbutton_cb), notebook);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), buttonadd);
	gtk_widget_set_can_default (buttonadd, TRUE);

	buttonremove = servlist_icon_button_new (_("_Remove"), ICON_SERVLIST_REMOVE);
	g_signal_connect (G_OBJECT (buttonremove), "clicked",
							G_CALLBACK (servlist_deletebutton_cb), notebook);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), buttonremove);
	gtk_widget_set_can_default (buttonremove, TRUE);

	buttonedit = gtk_button_new_with_mnemonic (_("_Edit"));
	g_signal_connect (G_OBJECT (buttonedit), "clicked",
							G_CALLBACK (servlist_editbutton_cb), notebook);
	gtk_container_add (GTK_CONTAINER (vbuttonbox1), buttonedit);
	gtk_widget_set_can_default (buttonedit, TRUE);


	/* Checkboxes and entries */
	table3 = gtkutil_grid_new (19, 2, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox5), table3, FALSE, FALSE, 0);
	gtk_grid_set_row_spacing (GTK_GRID (table3), 2);
	gtk_grid_set_column_spacing (GTK_GRID (table3), 8);

	check = servlist_create_check (0, !(net->flags & FLAG_CYCLE), table3, 0, 0, _("Connect to selected server only"));
	gtk_widget_set_tooltip_text (check, _("Don't cycle through all the servers when the connection fails."));
	servlist_create_check (3, net->flags & FLAG_AUTO_CONNECT, table3, 1, 0, _("Connect to this network automatically"));
	servlist_create_check (4, !(net->flags & FLAG_USE_PROXY), table3, 2, 0, _("Bypass proxy server"));
	check = servlist_create_check (2, net->flags & FLAG_USE_SSL, table3, 3, 0, _("Use SSL for all the servers on this network"));
#ifndef USE_OPENSSL
	gtk_widget_set_sensitive (check, FALSE);
#endif
	check = servlist_create_check (5, net->flags & FLAG_ALLOW_INVALID, table3, 4, 0, _("Accept invalid SSL certificates"));
#ifndef USE_OPENSSL
	gtk_widget_set_sensitive (check, FALSE);
#endif
	servlist_create_check (1, net->flags & FLAG_USE_GLOBAL, table3, 5, 0, _("Use global user information"));

	edit_check_ask_pass = gtk_check_button_new_with_mnemonic (_("Ask for password on connect"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edit_check_ask_pass), net->flags & FLAG_PROMPT_PASSWORD);
	servlist_table_attach (table3, edit_check_ask_pass, 0, 2, 6, 7,
					   FALSE, FALSE,
					   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
					   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);
	g_signal_connect (G_OBJECT (edit_check_ask_pass), "toggled",
				  G_CALLBACK (servlist_toggle_ask_pass_cb), NULL);

	edit_check_use_keyring = gtk_check_button_new_with_mnemonic (_("Use system keyring"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edit_check_use_keyring), net->flags & FLAG_USE_KEYRING);
	servlist_table_attach (table3, edit_check_use_keyring, 0, 2, 7, 8,
					   FALSE, FALSE,
					   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
					   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);
	g_signal_connect (G_OBJECT (edit_check_use_keyring), "toggled",
				  G_CALLBACK (servlist_toggle_keyring_cb), NULL);

	edit_entry_nick = servlist_create_entry (table3, _("_Nick name:"), 8, net->nick, &edit_label_nick, 0);
	edit_entry_nick2 = servlist_create_entry (table3, _("Second choice:"), 9, net->nick2, &edit_label_nick2, 0);
	edit_entry_real = servlist_create_entry (table3, _("Rea_l name:"), 10, net->real, &edit_label_real, 0);
	edit_entry_user = servlist_create_entry (table3, _("_User name:"), 11, net->user, &edit_label_user, 0);

	label_logintype = gtk_label_new (_("Login method:"));
	servlist_table_attach (table3, label_logintype, 0, 1, 12, 13,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);
	gtk_widget_set_halign (label_logintype, GTK_ALIGN_START);
	gtk_widget_set_valign (label_logintype, GTK_ALIGN_CENTER);
	combobox_logintypes = servlist_create_logintypecombo (notebook);
	servlist_table_attach (table3, combobox_logintypes, 1, 2, 12, 13,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_FILL,
						   4, 2);

	edit_entry_pass = servlist_create_entry (table3, _("Password:"), 13, NULL, 0, _("Password used for login. If in doubt, leave blank."));
	if (edit_loaded_password)
	{
		memset (edit_loaded_password, 0, strlen (edit_loaded_password));
		g_free (edit_loaded_password);
		edit_loaded_password = NULL;
	}
	edit_pass_changed = 0;
	g_signal_connect (G_OBJECT (edit_entry_pass), "changed",
					  G_CALLBACK (servlist_password_changed_cb), NULL);
	if (net->flags & FLAG_USE_KEYRING)
	{
		char *stored = secretstore_get_network_password (net->name);
		if (stored && *stored)
		{
			edit_loaded_password = g_strdup (stored);
			servlist_entry_set_text_silent (edit_entry_pass, "***");
		}
		if (stored)
		{
			memset (stored, 0, strlen (stored));
			g_free (stored);
		}
	}
	else if (net->pass && *net->pass)
	{
		servlist_entry_set_text_silent (edit_entry_pass, "***");
	}
	edit_pass_changed = 0;
	gtk_entry_set_visibility (GTK_ENTRY (edit_entry_pass), FALSE);
	if (selected_net && selected_net->logintype == LOGIN_SASLEXTERNAL)
		gtk_widget_set_sensitive (edit_entry_pass, FALSE);
	edit_check_show_pass = gtk_check_button_new_with_mnemonic (_("Show password"));
	servlist_table_attach (table3, edit_check_show_pass, 0, 2, 14, 15,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   4, 2);
	g_signal_connect (G_OBJECT (edit_check_show_pass), "toggled",
					  G_CALLBACK (servlist_toggle_show_password_cb), edit_entry_pass);

	edit_button_encrypt_pass = gtk_button_new_with_mnemonic (_("Encrypt saved password"));
	servlist_table_attach (table3, edit_button_encrypt_pass, 0, 1, 15, 16,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);
	g_signal_connect (G_OBJECT (edit_button_encrypt_pass), "clicked",
					  G_CALLBACK (servlist_encrypt_password_cb), net);
	edit_button_import_pass = gtk_button_new_with_mnemonic (_("Move password to keyring"));
	servlist_table_attach (table3, edit_button_import_pass, 1, 2, 15, 16,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   4, 2);
	g_signal_connect (G_OBJECT (edit_button_import_pass), "clicked",
					  G_CALLBACK (servlist_import_password_cb), net);

	label34 = gtk_label_new (_("Character set:"));
	servlist_table_attach (table3, label34, 0, 1, 16, 17,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);
	gtk_widget_set_halign (label34, GTK_ALIGN_START);
	gtk_widget_set_valign (label34, GTK_ALIGN_CENTER);
	comboboxentry_charset = servlist_create_charsetcombo ();
	servlist_table_attach (table3, comboboxentry_charset, 1, 2, 16, 17,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_FILL,
						   4, 2);

	hbox_cert_buttons = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 6);
	edit_button_cert_generate = gtk_button_new_with_mnemonic (_("Generate client SSL cert"));
	g_signal_connect (G_OBJECT (edit_button_cert_generate), "clicked",
							G_CALLBACK (servlist_generate_client_cert_cb), net);
	gtk_box_pack_start (GTK_BOX (hbox_cert_buttons), edit_button_cert_generate, FALSE, FALSE, 0);

	edit_button_cert_import = gtk_button_new_with_mnemonic (_("Import client SSL cert"));
	g_signal_connect (G_OBJECT (edit_button_cert_import), "clicked",
							G_CALLBACK (servlist_import_client_cert_cb), net);
	gtk_box_pack_start (GTK_BOX (hbox_cert_buttons), edit_button_cert_import, FALSE, FALSE, 0);

	edit_button_cert_info = gtk_button_new_with_mnemonic (_("Client SSL cert info"));
	g_signal_connect (G_OBJECT (edit_button_cert_info), "clicked",
							G_CALLBACK (servlist_cert_info_cb), net);
	gtk_box_pack_start (GTK_BOX (hbox_cert_buttons), edit_button_cert_info, FALSE, FALSE, 0);

	edit_button_cert_delete = gtk_button_new_with_mnemonic (_("Delete cert"));
	g_signal_connect (G_OBJECT (edit_button_cert_delete), "clicked",
							G_CALLBACK (servlist_delete_client_cert_cb), net);
	gtk_box_pack_start (GTK_BOX (hbox_cert_buttons), edit_button_cert_delete, FALSE, FALSE, 0);

	servlist_table_attach (table3, hbox_cert_buttons, 0, 2, 17, 18,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   SERVLIST_X_PADDING, SERVLIST_Y_PADDING);


	/* Rule and Close button */
	hseparator2 = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (vbox5), hseparator2, FALSE, FALSE, 8);

	hbuttonbox4 = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox4), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (vbox5), hbuttonbox4, FALSE, FALSE, 0);

	button10 = servlist_icon_button_new (_("_Close"), ICON_SERVLIST_CLOSE);
	g_signal_connect (G_OBJECT (button10), "clicked",
							G_CALLBACK (servlist_edit_close_cb), 0);
	gtk_container_add (GTK_CONTAINER (hbuttonbox4), button10);
	gtk_widget_set_can_default (button10, TRUE);

	if (net->flags & FLAG_USE_GLOBAL)
	{
		servlist_toggle_global_user (FALSE);
	}
	servlist_toggle_keyring_cb (GTK_TOGGLE_BUTTON (edit_check_use_keyring), NULL);
	servlist_update_password_tools (net);

	gtk_widget_grab_focus (button10);
	gtk_widget_grab_default (button10);

	gtk_widget_show_all (editwindow);
	servlist_update_cert_buttons (net);

	/* We can't set the active tab without child elements being shown, so this must be *after* gtk_widget_show()s! */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), netedit_active_tab);

	/* We need to connect this *after* setting the active tab so that the value doesn't get overriden. */
	g_signal_connect (G_OBJECT (notebook), "switch-page", G_CALLBACK (servlist_edit_tabswitch_cb), notebook);

	return editwindow;
}

static GtkWidget *
servlist_open_networks (void)
{
	GtkWidget *servlist;
	GtkWidget *vbox1;
	GtkWidget *label2;
	GtkWidget *table1;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *label5;
	GtkWidget *label6;
	/* GtkWidget *label7; */
	GtkWidget *entry1;
	GtkWidget *entry2;
	GtkWidget *entry3;
	GtkWidget *entry4;
	/* GtkWidget *entry5; */
	GtkWidget *vbox2;
	GtkWidget *label1;
	GtkWidget *table4;
	GtkWidget *scrolledwindow3;
	GtkWidget *treeview_networks;
	GtkWidget *checkbutton_skip;
	GtkWidget *checkbutton_fav;
	GtkWidget *hbox;
	GtkWidget *vbuttonbox2;
	GtkWidget *button_add;
	GtkWidget *button_remove;
	GtkWidget *button_edit;
	GtkWidget *button_sort;
	GtkWidget *hseparator1;
	GtkWidget *hbuttonbox1;
	GtkWidget *button_connect;
	GtkWidget *button_close;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	char buf[128];

	servlist = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	theme_manager_attach_window (servlist);
	gtk_container_set_border_width (GTK_CONTAINER (servlist), 4);
	g_snprintf(buf, sizeof(buf), _("Network List - %s"), _(DISPLAY_NAME));
	gtk_window_set_title (GTK_WINDOW (servlist), buf);
	gtk_window_set_default_size (GTK_WINDOW (servlist), netlist_win_width, netlist_win_height);
	gtk_window_set_role (GTK_WINDOW (servlist), "servlist");
	gtk_window_set_type_hint (GTK_WINDOW (servlist), GDK_WINDOW_TYPE_HINT_DIALOG);
	if (current_sess)
		gtk_window_set_transient_for (GTK_WINDOW (servlist), GTK_WINDOW (current_sess->gui->window));

	vbox1 = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (servlist), vbox1);

	label2 = bold_label (_("User Information"));
	gtk_box_pack_start (GTK_BOX (vbox1), label2, FALSE, FALSE, 0);

	table1 = gtkutil_grid_new (5, 2, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (vbox1), table1, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 8);
	gtk_grid_set_row_spacing (GTK_GRID (table1), 2);
	gtk_grid_set_column_spacing (GTK_GRID (table1), 4);

	label3 = gtk_label_new_with_mnemonic (_("_Nick name:"));
	gtk_widget_show (label3);
	servlist_table_attach (table1, label3, 0, 1, 0, 1,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   0, 0);
	gtk_widget_set_halign (label3, GTK_ALIGN_START);
	gtk_widget_set_valign (label3, GTK_ALIGN_CENTER);

	label4 = gtk_label_new (_("Second choice:"));
	gtk_widget_show (label4);
	servlist_table_attach (table1, label4, 0, 1, 1, 2,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   0, 0);
	gtk_widget_set_halign (label4, GTK_ALIGN_START);
	gtk_widget_set_valign (label4, GTK_ALIGN_CENTER);

	label5 = gtk_label_new (_("Third choice:"));
	gtk_widget_show (label5);
	servlist_table_attach (table1, label5, 0, 1, 2, 3,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   0, 0);
	gtk_widget_set_halign (label5, GTK_ALIGN_START);
	gtk_widget_set_valign (label5, GTK_ALIGN_CENTER);

	label6 = gtk_label_new_with_mnemonic (_("_User name:"));
	gtk_widget_show (label6);
	servlist_table_attach (table1, label6, 0, 1, 3, 4,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_START, SERVLIST_ALIGN_CENTER,
						   0, 0);
	gtk_widget_set_halign (label6, GTK_ALIGN_START);
	gtk_widget_set_valign (label6, GTK_ALIGN_CENTER);

	/* label7 = gtk_label_new_with_mnemonic (_("Rea_l name:"));
	gtk_widget_show (label7);
	gtk_table_attach (GTK_TABLE (table1), label7, 0, 1, 4, 5,
							(GtkAttachOptions) (GTK_FILL),
							(GtkAttachOptions) (0), 0, 0);
	*/

	entry_nick1 = entry1 = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry1), prefs.hex_irc_nick1);
	gtk_widget_show (entry1);
	servlist_table_attach (table1, entry1, 1, 2, 0, 1,
						   TRUE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_CENTER,
						   0, 0);

	entry_nick2 = entry2 = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry2), prefs.hex_irc_nick2);
	gtk_widget_show (entry2);
	servlist_table_attach (table1, entry2, 1, 2, 1, 2,
						   TRUE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_CENTER,
						   0, 0);

	entry_nick3 = entry3 = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry3), prefs.hex_irc_nick3);
	gtk_widget_show (entry3);
	servlist_table_attach (table1, entry3, 1, 2, 2, 3,
						   TRUE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_CENTER,
						   0, 0);

	entry_guser = entry4 = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry4), prefs.hex_irc_user_name);
	gtk_widget_show (entry4);
	servlist_table_attach (table1, entry4, 1, 2, 3, 4,
						   TRUE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_CENTER,
						   0, 0);

	/* entry_greal = entry5 = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry5), prefs.hex_irc_real_name);
	gtk_widget_show (entry5);
	gtk_table_attach (GTK_TABLE (table1), entry5, 1, 2, 4, 5,
							(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
							(GtkAttachOptions) (0), 0, 0); */

	vbox2 = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, TRUE, TRUE, 0);

	label1 = bold_label (_("Networks"));
	gtk_box_pack_start (GTK_BOX (vbox2), label1, FALSE, FALSE, 0);

	table4 = gtkutil_grid_new (2, 2, FALSE);
	gtk_widget_show (table4);
	gtk_box_pack_start (GTK_BOX (vbox2), table4, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table4), 8);
	gtk_grid_set_row_spacing (GTK_GRID (table4), 2);
	gtk_grid_set_column_spacing (GTK_GRID (table4), 3);

	scrolledwindow3 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow3);
	servlist_table_attach (table4, scrolledwindow3, 0, 1, 0, 1,
						   TRUE, TRUE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_FILL,
						   0, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow3),
											  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow3),
													 GTK_SHADOW_IN);

	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INT);
	model = GTK_TREE_MODEL (store);

	networks_tree = treeview_networks = gtk_tree_view_new_with_model (model);
	g_object_unref (model);
	gtk_widget_show (treeview_networks);
	gtk_container_add (GTK_CONTAINER (scrolledwindow3), treeview_networks);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview_networks),
												  FALSE);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
							G_CALLBACK (servlist_celledit_cb), model);
	gtk_tree_view_insert_column_with_attributes (
								GTK_TREE_VIEW (treeview_networks), -1,
						 		0, renderer,
						 		"text", 0,
								"editable", 1,
								"weight", 2,
								NULL);

	hbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
	servlist_table_attach (table4, hbox, 0, 2, 1, 2,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_CENTER,
						   0, 0);
	gtk_widget_show (hbox);

	checkbutton_skip =
		gtk_check_button_new_with_mnemonic (_("Skip network list on startup"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton_skip),
											prefs.hex_gui_slist_skip);
	gtk_container_add (GTK_CONTAINER (hbox), checkbutton_skip);
	g_signal_connect (G_OBJECT (checkbutton_skip), "toggled",
							G_CALLBACK (no_servlist), 0);
	gtk_widget_show (checkbutton_skip);

	checkbutton_fav =
		gtk_check_button_new_with_mnemonic (_("Show favorites only"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton_fav),
											prefs.hex_gui_slist_fav);
	gtk_container_add (GTK_CONTAINER (hbox), checkbutton_fav);
	g_signal_connect (G_OBJECT (checkbutton_fav), "toggled",
							G_CALLBACK (fav_servlist), 0);
	gtk_widget_show (checkbutton_fav);

	vbuttonbox2 = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox2), 3);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox2), GTK_BUTTONBOX_START);
	gtk_widget_show (vbuttonbox2);
	servlist_table_attach (table4, vbuttonbox2, 1, 2, 0, 1,
						   FALSE, FALSE,
						   SERVLIST_ALIGN_FILL, SERVLIST_ALIGN_FILL,
						   0, 0);

	button_add = servlist_icon_button_new (_("_Add"), ICON_SERVLIST_ADD);
	g_signal_connect (G_OBJECT (button_add), "clicked",
							G_CALLBACK (servlist_addnet_cb), networks_tree);
	gtk_widget_show (button_add);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), button_add);
	gtk_widget_set_can_default (button_add, TRUE);

	button_remove = servlist_icon_button_new (_("_Remove"), ICON_SERVLIST_REMOVE);
	g_signal_connect (G_OBJECT (button_remove), "clicked",
							G_CALLBACK (servlist_deletenet_cb), 0);
	gtk_widget_show (button_remove);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), button_remove);
	gtk_widget_set_can_default (button_remove, TRUE);

	button_edit = gtk_button_new_with_mnemonic (_("_Edit..."));
	g_signal_connect (G_OBJECT (button_edit), "clicked",
							G_CALLBACK (servlist_edit_cb), 0);
	gtk_widget_show (button_edit);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), button_edit);
	gtk_widget_set_can_default (button_edit, TRUE);

	button_sort = gtk_button_new_with_mnemonic (_("_Sort"));
	gtk_widget_set_tooltip_text (button_sort, _("Sorts the network list in alphabetical order. "
				"Use Shift+Up and Shift+Down keys to move a row."));
	g_signal_connect (G_OBJECT (button_sort), "clicked",
							G_CALLBACK (servlist_sort), 0);
	gtk_widget_show (button_sort);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), button_sort);
	gtk_widget_set_can_default (button_sort, TRUE);

	button_sort = gtk_button_new_with_mnemonic (_("_Favor"));
	gtk_widget_set_tooltip_text (button_sort, _("Mark or unmark this network as a favorite."));
	g_signal_connect (G_OBJECT (button_sort), "clicked",
							G_CALLBACK (servlist_favor), 0);
	gtk_widget_show (button_sort);
	gtk_container_add (GTK_CONTAINER (vbuttonbox2), button_sort);
	gtk_widget_set_can_default (button_sort, TRUE);

	hseparator1 = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_show (hseparator1);
	gtk_box_pack_start (GTK_BOX (vbox1), hseparator1, FALSE, TRUE, 4);

	hbuttonbox1 = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_SPREAD);
	gtk_widget_show (hbuttonbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox1, FALSE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox1), 8);

	button_close = servlist_icon_button_new (_("_Close"), ICON_SERVLIST_CLOSE);
	gtk_widget_show (button_close);
	g_signal_connect (G_OBJECT (button_close), "clicked",
							G_CALLBACK (servlist_close_cb), 0);
	gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_close);
	gtk_widget_set_can_default (button_close, TRUE);

button_connect = gtkutil_button (hbuttonbox1, ICON_SERVLIST_CONNECT, NULL,
												servlist_connect_cb, NULL, _("C_onnect"));
	gtk_widget_set_can_default (button_connect, TRUE);

	g_signal_connect (G_OBJECT (entry_guser), "changed", 
					G_CALLBACK(servlist_username_changed_cb), button_connect);
	g_signal_connect (G_OBJECT (entry_nick1), "changed",
					G_CALLBACK(servlist_nick_changed_cb), button_connect);
	g_signal_connect (G_OBJECT (entry_nick2), "changed",
					G_CALLBACK(servlist_nick_changed_cb), button_connect);

	/* Run validity checks now */
	servlist_nick_changed_cb (GTK_ENTRY(entry_nick2), button_connect);
	servlist_username_changed_cb (GTK_ENTRY(entry_guser), button_connect);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label3), entry1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label6), entry4);
	/* gtk_label_set_mnemonic_widget (GTK_LABEL (label7), entry5); */

	gtk_widget_grab_focus (networks_tree);
	gtk_widget_grab_default (button_close);
	return servlist;
}

/* First-run onboarding ---------------------------------------------------- */

#define ONBOARDING_ATLAS_HOST "atlas.zoitechat.org"
#define ONBOARDING_ATLAS_CLIENT_PATH "/api/v1/client/networks"
#define ONBOARDING_ATLAS_PUBLIC_PATH "/api/v1/networks"
#define ONBOARDING_ATLAS_MAX_RESPONSE (2 * 1024 * 1024)
#define ONBOARDING_REGISTRATION_TIMEOUT_SECONDS 60

typedef enum
{
	ONBOARDING_ACCOUNT_SKIP = 0,
	ONBOARDING_ACCOUNT_EXISTING,
	ONBOARDING_ACCOUNT_NEW
} ServlistOnboardingAccountMode;

typedef struct
{
	ircnet *net;
	char *slug;
	char *name;
	char *description;
	char *homepage;
	char *endpoint_host;
	int endpoint_port;
	gboolean endpoint_tls;
	gboolean atlas_approved;
	int users;
	char *services_family;
	int services_confidence;
	gboolean services_present;
	gboolean supports_sasl;
	gboolean has_nickserv;
	char *help_channel;
	GPtrArray *starter_channels;
	GPtrArray *popular_channels;
	gboolean detail_loaded;
	gboolean onboarding_verified;
	gboolean registration_verified;
	char *registration_service;
	char *registration_template;
	gboolean registration_email_required;
	char *identify_method;
} ServlistOnboardingChoice;

typedef struct
{
	char *client_json;
	char *public_json;
	char *error;
} ServlistAtlasFetchResult;

typedef struct
{
	char *slug;
	char *json;
	char *error;
	guint generation;
} ServlistAtlasDetailResult;

typedef struct
{
	char *slug;
	guint generation;
} ServlistAtlasDetailRequest;

typedef struct
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *back_button;
	GtkWidget *next_button;
	GtkWidget *connect_button;
	GtkWidget *use_atlas;
	GtkWidget *network_combo;
	GtkWidget *atlas_status;
	GtkWidget *network_info;
	GtkWidget *nick_entry;
	GtkWidget *realname_entry;
	GtkWidget *account_skip;
	GtkWidget *account_existing;
	GtkWidget *account_new;
	GtkWidget *account_info;
	GtkWidget *account_label;
	GtkWidget *account_entry;
	GtkWidget *password_label;
	GtkWidget *password_entry;
	GtkWidget *password_confirm_label;
	GtkWidget *password_confirm_entry;
	GtkWidget *email_label;
	GtkWidget *email_entry;
	GtkWidget *remember_password;
	GtkWidget *channel_suggestions;
	GtkWidget *channel_entry;
	GtkWidget *summary_label;
	GPtrArray *choices;
	GCancellable *atlas_cancel;
	GCancellable *detail_cancel;
	gboolean atlas_pending;
	guint detail_pending;
	guint detail_generation;
	gboolean closing;
	session *sess;
} ServlistOnboardingState;

typedef struct
{
	server *serv;
	ServlistOnboardingAccountMode account_mode;
	ircnet *net;
	char *network_name;
	char *nick;
	char *password;
	char *email;
	char *service;
	char *command_template;
	char *homepage;
	char *services_family;
	char *identify_method;
	int services_confidence;
	int preferred_login;
	gboolean email_required;
	gboolean remember_password;
	gboolean verified_recipe;
	gboolean common_recipe;
	gboolean has_nickserv;
	gint64 deadline;
} ServlistRegistrationGuide;

static void servlist_open_networks_window (session *sess);
static void servlist_onboarding_update_network_info (ServlistOnboardingState *state);
static void servlist_onboarding_account_changed (GtkToggleButton *button, gpointer userdata);
static void servlist_onboarding_refresh_channel_suggestions (ServlistOnboardingState *state);
static void servlist_onboarding_start_detail (ServlistOnboardingState *state, ServlistOnboardingChoice *choice);

static void
servlist_onboarding_choice_free (ServlistOnboardingChoice *choice)
{
	if (!choice)
		return;
	g_free (choice->slug);
	g_free (choice->name);
	g_free (choice->description);
	g_free (choice->homepage);
	g_free (choice->endpoint_host);
	g_free (choice->services_family);
	g_free (choice->help_channel);
	if (choice->starter_channels)
		g_ptr_array_free (choice->starter_channels, TRUE);
	if (choice->popular_channels)
		g_ptr_array_free (choice->popular_channels, TRUE);
	g_free (choice->registration_service);
	g_free (choice->registration_template);
	g_free (choice->identify_method);
	g_free (choice);
}

static void
servlist_atlas_fetch_result_free (ServlistAtlasFetchResult *result)
{
	if (!result)
		return;
	g_free (result->client_json);
	g_free (result->public_json);
	g_free (result->error);
	g_free (result);
}

static void
servlist_atlas_detail_result_free (ServlistAtlasDetailResult *result)
{
	if (!result)
		return;
	g_free (result->slug);
	g_free (result->json);
	g_free (result->error);
	g_free (result);
}

static void
servlist_atlas_detail_request_free (ServlistAtlasDetailRequest *request)
{
	if (!request)
		return;
	g_free (request->slug);
	g_free (request);
}

static void
servlist_onboarding_state_free (ServlistOnboardingState *state)
{
	if (!state)
		return;
	if (state->choices)
		g_ptr_array_free (state->choices, TRUE);
	if (state->atlas_cancel)
		g_object_unref (state->atlas_cancel);
	if (state->detail_cancel)
		g_object_unref (state->detail_cancel);
	g_free (state);
}

static void
servlist_onboarding_close_state (ServlistOnboardingState *state)
{
	state->closing = TRUE;
	state->dialog = NULL;
	if (state->atlas_cancel)
		g_cancellable_cancel (state->atlas_cancel);
	if (state->detail_cancel)
		g_cancellable_cancel (state->detail_cancel);
	if (!state->atlas_pending && state->detail_pending == 0)
		servlist_onboarding_state_free (state);
}

static ServlistOnboardingChoice *
servlist_onboarding_choice_new (ircnet *net, const char *name)
{
	ServlistOnboardingChoice *choice = g_new0 (ServlistOnboardingChoice, 1);
	choice->net = net;
	choice->name = g_strdup (name ? name : "");
	choice->users = -1;
	choice->services_confidence = -1;
	choice->endpoint_port = 6697;
	choice->starter_channels = g_ptr_array_new_with_free_func (g_free);
	choice->popular_channels = g_ptr_array_new_with_free_func (g_free);
	return choice;
}

static gboolean
servlist_onboarding_token_is_safe (const char *text)
{
	const unsigned char *p;

	if (!text || !*text)
		return FALSE;
	for (p = (const unsigned char *) text; *p; p++)
	{
		if (g_ascii_iscntrl (*p) || g_ascii_isspace (*p))
			return FALSE;
	}
	return TRUE;
}

static gboolean
servlist_onboarding_slug_is_safe (const char *text)
{
	const unsigned char *p;
	gsize len;

	if (!text || !*text)
		return FALSE;
	len = strlen (text);
	if (len > 80 || !g_ascii_isalnum (text[0]))
		return FALSE;
	for (p = (const unsigned char *) text; *p; p++)
	{
		if (!g_ascii_isalnum (*p) && *p != '-')
			return FALSE;
	}
	return TRUE;
}

static gboolean
servlist_onboarding_endpoint_is_safe (const char *text)
{
	const unsigned char *p;

	if (!servlist_onboarding_token_is_safe (text) || strlen (text) > 255)
		return FALSE;
	for (p = (const unsigned char *) text; *p; p++)
	{
		if (*p == '/' || *p == '\\' || *p == ',' || *p == '+')
			return FALSE;
	}
	return TRUE;
}

static gboolean
servlist_onboarding_service_target_is_safe (const char *text)
{
	const unsigned char *p;

	if (!servlist_onboarding_token_is_safe (text) || strlen (text) > 64)
		return FALSE;
	for (p = (const unsigned char *) text; *p; p++)
	{
		if (*p == ':' || *p == ',' || *p == '/' || *p == '\\')
			return FALSE;
	}
	return TRUE;
}

static gboolean
servlist_onboarding_registration_template_is_safe (const char *command)
{
	const char *p;

	if (!command || !*command || strlen (command) > 300 ||
	    strchr (command, '\r') || strchr (command, '\n'))
		return FALSE;

	while (g_ascii_isspace (*command))
		command++;
	if (g_ascii_strncasecmp (command, "REGISTER", 8) ||
	    (command[8] && !g_ascii_isspace (command[8])))
		return FALSE;
	if (!strstr (command, "{password}"))
		return FALSE;

	for (p = command; (p = strchr (p, '{')) != NULL; )
	{
		const char *end = strchr (p, '}');
		char *token;
		gboolean allowed;

		if (!end)
			return FALSE;
		token = g_strndup (p, end - p + 1);
		allowed = !strcmp (token, "{password}") || !strcmp (token, "{email}") ||
		          !strcmp (token, "{nick}") || !strcmp (token, "{nickname}") ||
		          !strcmp (token, "{account}");
		g_free (token);
		if (!allowed)
			return FALSE;
		p = end + 1;
	}
	return TRUE;
}

static void
servlist_onboarding_set_string (char **dest, const char *value)
{
	if (!value || !*value)
		return;
	g_free (*dest);
	*dest = g_strdup (value);
}

static gboolean
servlist_onboarding_ptr_array_contains (GPtrArray *array, const char *value)
{
	guint i;

	if (!array || !value)
		return FALSE;
	for (i = 0; i < array->len; i++)
	{
		if (!g_ascii_strcasecmp (g_ptr_array_index (array, i), value))
			return TRUE;
	}
	return FALSE;
}

static void
servlist_onboarding_add_starter_channel (ServlistOnboardingChoice *choice, const char *channel)
{
	if (!choice || !channel || !strchr ("#&+!", channel[0]))
		return;
	if (!servlist_onboarding_ptr_array_contains (choice->starter_channels, channel))
		g_ptr_array_add (choice->starter_channels, g_strdup (channel));
}

/* Small bounded JSON reader. Atlas responses are deliberately parsed here
 * without adding json-glib or libsoup to every ZoiteChat platform build. It
 * understands the JSON types needed by the read-only Atlas feed and ignores
 * additive fields it does not know about. */
static const char *
servlist_json_skip_ws (const char *p, const char *end)
{
	while (p < end && g_ascii_isspace (*p))
		p++;
	return p;
}

static const char *
servlist_json_string_end (const char *p, const char *end)
{
	if (p >= end || *p != '"')
		return NULL;
	p++;
	while (p < end)
	{
		if (*p == '\\')
		{
			p += 2;
			continue;
		}
		if (*p == '"')
			return p + 1;
		p++;
	}
	return NULL;
}

static const char *
servlist_json_value_end (const char *p, const char *end)
{
	char open;
	char close;
	int depth;

	p = servlist_json_skip_ws (p, end);
	if (p >= end)
		return NULL;
	if (*p == '"')
		return servlist_json_string_end (p, end);
	if (*p != '{' && *p != '[')
	{
		while (p < end && *p != ',' && *p != '}' && *p != ']')
			p++;
		return p;
	}

	open = *p;
	close = open == '{' ? '}' : ']';
	depth = 1;
	p++;
	while (p < end && depth > 0)
	{
		if (*p == '"')
		{
			p = servlist_json_string_end (p, end);
			if (!p)
				return NULL;
			continue;
		}
		if (*p == open)
			depth++;
		else if (*p == close)
			depth--;
		else if ((open == '{' && *p == '[') || (open == '[' && *p == '{'))
		{
			const char *nested = servlist_json_value_end (p, end);
			if (!nested)
				return NULL;
			p = nested;
			continue;
		}
		p++;
	}
	return depth == 0 ? p : NULL;
}

static char *
servlist_json_dup_string (const char *p, const char *end, const char **after)
{
	GString *out;
	gunichar uc;
	char utf8[7];
	int n;

	p = servlist_json_skip_ws (p, end);
	if (p >= end || *p != '"')
		return NULL;
	p++;
	out = g_string_new (NULL);
	while (p < end)
	{
		if (*p == '"')
		{
			p++;
			if (after)
				*after = p;
			return g_string_free (out, FALSE);
		}
		if (*p != '\\')
		{
			g_string_append_c (out, *p++);
			continue;
		}
		p++;
		if (p >= end)
			break;
		switch (*p++)
		{
		case '"': g_string_append_c (out, '"'); break;
		case '\\': g_string_append_c (out, '\\'); break;
		case '/': g_string_append_c (out, '/'); break;
		case 'b': g_string_append_c (out, '\b'); break;
		case 'f': g_string_append_c (out, '\f'); break;
		case 'n': g_string_append_c (out, '\n'); break;
		case 'r': g_string_append_c (out, '\r'); break;
		case 't': g_string_append_c (out, '\t'); break;
		case 'u':
			if (end - p < 4 || !g_ascii_isxdigit (p[0]) || !g_ascii_isxdigit (p[1]) ||
			    !g_ascii_isxdigit (p[2]) || !g_ascii_isxdigit (p[3]))
				goto invalid;
			{
				char hex[5];
				memcpy (hex, p, 4);
				hex[4] = 0;
				uc = (gunichar) strtoul (hex, NULL, 16);
				p += 4;
				if (uc >= 0xd800 && uc <= 0xdbff)
				{
					gunichar low;
					if (end - p < 6 || p[0] != '\\' || p[1] != 'u' ||
					    !g_ascii_isxdigit (p[2]) || !g_ascii_isxdigit (p[3]) ||
					    !g_ascii_isxdigit (p[4]) || !g_ascii_isxdigit (p[5]))
						goto invalid;
					memcpy (hex, p + 2, 4);
					hex[4] = 0;
					low = (gunichar) strtoul (hex, NULL, 16);
					if (low < 0xdc00 || low > 0xdfff)
						goto invalid;
					p += 6;
					uc = 0x10000 + ((uc - 0xd800) << 10) + (low - 0xdc00);
				}
				else if (uc >= 0xdc00 && uc <= 0xdfff)
					goto invalid;
				n = g_unichar_to_utf8 (uc, utf8);
				utf8[n] = 0;
				g_string_append (out, utf8);
			}
			break;
		default:
			goto invalid;
		}
	}
invalid:
	g_string_free (out, TRUE);
	return NULL;
}

static const char *
servlist_json_object_member (const char *object, const char *end, const char *wanted)
{
	const char *p;
	char *key;

	p = servlist_json_skip_ws (object, end);
	if (p >= end || *p != '{')
		return NULL;
	p++;
	while (p < end)
	{
		const char *after_key;
		const char *value;
		const char *next;

		p = servlist_json_skip_ws (p, end);
		if (p >= end || *p == '}')
			return NULL;
		key = servlist_json_dup_string (p, end, &after_key);
		if (!key)
			return NULL;
		p = servlist_json_skip_ws (after_key, end);
		if (p >= end || *p != ':')
		{
			g_free (key);
			return NULL;
		}
		value = servlist_json_skip_ws (p + 1, end);
		if (!strcmp (key, wanted))
		{
			g_free (key);
			return value;
		}
		g_free (key);
		next = servlist_json_value_end (value, end);
		if (!next)
			return NULL;
		p = servlist_json_skip_ws (next, end);
		if (p < end && *p == ',')
			p++;
	}
	return NULL;
}

static char *
servlist_json_member_string (const char *object, const char *end, const char *key)
{
	const char *value = servlist_json_object_member (object, end, key);
	return value ? servlist_json_dup_string (value, end, NULL) : NULL;
}

static int
servlist_json_member_int (const char *object, const char *end, const char *key, int fallback)
{
	const char *value = servlist_json_object_member (object, end, key);
	char *tail;
	long parsed;

	if (!value)
		return fallback;
	parsed = strtol (value, &tail, 10);
	if (tail == value || parsed < G_MININT || parsed > G_MAXINT)
		return fallback;
	return (int) parsed;
}

static gboolean
servlist_json_member_bool (const char *object, const char *end, const char *key, gboolean fallback)
{
	const char *value = servlist_json_object_member (object, end, key);
	if (!value)
		return fallback;
	if (!strncmp (value, "true", 4) || *value == '1')
		return TRUE;
	if (!strncmp (value, "false", 5) || *value == '0')
		return FALSE;
	return fallback;
}

static gboolean
servlist_json_array_contains_string (const char *array, const char *end, const char *wanted)
{
	const char *p;

	p = servlist_json_skip_ws (array, end);
	if (p >= end || *p != '[')
		return FALSE;
	p++;
	while (p < end)
	{
		const char *after;
		char *value;

		p = servlist_json_skip_ws (p, end);
		if (p >= end || *p == ']')
			break;
		if (*p == '"')
		{
			value = servlist_json_dup_string (p, end, &after);
			if (!value)
				return FALSE;
			if (!g_ascii_strcasecmp (value, wanted))
			{
				g_free (value);
				return TRUE;
			}
			g_free (value);
			p = after;
		}
		else
		{
			p = servlist_json_value_end (p, end);
			if (!p)
				return FALSE;
		}
		p = servlist_json_skip_ws (p, end);
		if (p < end && *p == ',')
			p++;
	}
	return FALSE;
}

static gboolean
servlist_json_array_has_prefix_ci (const char *array, const char *end, const char *prefix)
{
	const char *p;
	gsize prefix_len;

	if (!prefix)
		return FALSE;
	prefix_len = strlen (prefix);
	p = servlist_json_skip_ws (array, end);
	if (p >= end || *p != '[')
		return FALSE;
	p++;
	while (p < end)
	{
		const char *after;
		char *value;

		p = servlist_json_skip_ws (p, end);
		if (p >= end || *p == ']')
			break;
		if (*p == '"')
		{
			value = servlist_json_dup_string (p, end, &after);
			if (!value)
				return FALSE;
			if (!g_ascii_strncasecmp (value, prefix, prefix_len))
			{
				g_free (value);
				return TRUE;
			}
			g_free (value);
			p = after;
		}
		else
		{
			p = servlist_json_value_end (p, end);
			if (!p)
				return FALSE;
		}
		p = servlist_json_skip_ws (p, end);
		if (p < end && *p == ',')
			p++;
	}
	return FALSE;
}

static gboolean
servlist_json_array_contains_text_ci (const char *array, const char *end, const char *wanted)
{
	const char *p;
	char *wanted_folded;
	gboolean found = FALSE;

	if (!wanted)
		return FALSE;
	wanted_folded = g_ascii_strdown (wanted, -1);
	p = servlist_json_skip_ws (array, end);
	if (p >= end || *p != '[')
		goto done;
	p++;
	while (p < end)
	{
		const char *after;
		char *value;
		char *folded;

		p = servlist_json_skip_ws (p, end);
		if (p >= end || *p == ']')
			break;
		if (*p == '"')
		{
			value = servlist_json_dup_string (p, end, &after);
			if (!value)
				break;
			folded = g_ascii_strdown (value, -1);
			found = strstr (folded, wanted_folded) != NULL;
			g_free (folded);
			g_free (value);
			if (found)
				break;
			p = after;
		}
		else
		{
			p = servlist_json_value_end (p, end);
			if (!p)
				break;
		}
		p = servlist_json_skip_ws (p, end);
		if (p < end && *p == ',')
			p++;
	}

done:
	g_free (wanted_folded);
	return found;
}

static void
servlist_json_add_string_array (const char *array, const char *end, ServlistOnboardingChoice *choice)
{
	const char *p;

	p = servlist_json_skip_ws (array, end);
	if (p >= end || *p != '[')
		return;
	p++;
	while (p < end)
	{
		const char *after;
		char *value;

		p = servlist_json_skip_ws (p, end);
		if (p >= end || *p == ']')
			break;
		if (*p != '"')
		{
			p = servlist_json_value_end (p, end);
			if (!p)
				break;
		}
		else
		{
			value = servlist_json_dup_string (p, end, &after);
			if (!value)
				break;
			servlist_onboarding_add_starter_channel (choice, value);
			g_free (value);
			p = after;
		}
		p = servlist_json_skip_ws (p, end);
		if (p < end && *p == ',')
			p++;
	}
}

static ServlistOnboardingChoice *
servlist_onboarding_find_choice (ServlistOnboardingState *state, const char *name, const char *slug)
{
	guint i;

	for (i = 0; i < state->choices->len; i++)
	{
		ServlistOnboardingChoice *choice = g_ptr_array_index (state->choices, i);
		if (slug && *slug && choice->slug && !g_ascii_strcasecmp (choice->slug, slug))
			return choice;
		if (name && *name && choice->name && !g_ascii_strcasecmp (choice->name, name))
			return choice;
	}
	return NULL;
}

static void
servlist_onboarding_parse_endpoint (ServlistOnboardingChoice *choice, const char *object, const char *end)
{
	const char *array;
	const char *item;
	const char *item_end;
	char *host;
	int port;
	gboolean tls;

	array = servlist_json_object_member (object, end, "servers");
	if (!array)
		array = servlist_json_object_member (object, end, "endpoints");
	if (array)
	{
		item = servlist_json_skip_ws (array, end);
		if (*item == '[')
		{
			item = servlist_json_skip_ws (item + 1, end);
			if (item < end && *item == '{')
			{
				item_end = servlist_json_value_end (item, end);
				if (item_end)
				{
					object = item;
					end = item_end;
				}
			}
		}
	}

	host = servlist_json_member_string (object, end, "host");
	if (!host)
		host = servlist_json_member_string (object, end, "hostname");
	if (!host)
		host = servlist_json_member_string (object, end, "server");
	if (!host || !servlist_onboarding_endpoint_is_safe (host))
	{
		g_free (host);
		return;
	}

#ifdef USE_OPENSSL
	port = servlist_json_member_int (object, end, "tls_port", 0);
	tls = port > 0;
	if (port <= 0)
	{
		port = servlist_json_member_int (object, end, "plain_port", 0);
		tls = FALSE;
	}
#else
	port = servlist_json_member_int (object, end, "plain_port", 0);
	tls = FALSE;
#endif
	if (port <= 0)
		port = servlist_json_member_int (object, end, "port", 0);
	if (port <= 0 || port > 65535)
		port = tls ? 6697 : 6667;
#ifdef USE_OPENSSL
	tls = servlist_json_member_bool (object, end, "tls", tls);
#endif

	g_free (choice->endpoint_host);
	choice->endpoint_host = host;
	choice->endpoint_port = port;
	choice->endpoint_tls = tls;
	choice->atlas_approved = TRUE;
}

static void
servlist_onboarding_parse_services (ServlistOnboardingChoice *choice, const char *object, const char *end)
{
	const char *services = servlist_json_object_member (object, end, "services");
	const char *caps = servlist_json_object_member (object, end, "capabilities");
	const char *nicks;
	char *family;

	if (services && *services == '{')
	{
		const char *services_end = servlist_json_value_end (services, end);
		family = services_end ? servlist_json_member_string (services, services_end, "family") : NULL;
		if (family)
		{
			servlist_onboarding_set_string (&choice->services_family, family);
			g_free (family);
		}
		choice->services_confidence = services_end ? servlist_json_member_int (services, services_end, "confidence", choice->services_confidence) : choice->services_confidence;
		choice->services_present = services_end ? servlist_json_member_bool (services, services_end, "services_present", choice->services_present) : choice->services_present;
		nicks = services_end ? servlist_json_object_member (services, services_end, "service_nicks") : NULL;
		if (!nicks && services_end)
			nicks = servlist_json_object_member (services, services_end, "nicks");
		if (nicks && servlist_json_array_contains_string (nicks, services_end, "NickServ"))
			choice->has_nickserv = TRUE;
	}
	else
	{
		family = servlist_json_member_string (object, end, "services_family");
		if (family)
		{
			servlist_onboarding_set_string (&choice->services_family, family);
			g_free (family);
		}
		choice->services_confidence = servlist_json_member_int (object, end, "services_confidence", choice->services_confidence);
		choice->services_present = servlist_json_member_bool (object, end, "services_present", choice->services_present);
	}

	if (caps && servlist_json_array_has_prefix_ci (caps, end, "sasl"))
		choice->supports_sasl = TRUE;
}

static void
servlist_onboarding_parse_verified_metadata (ServlistOnboardingChoice *choice, const char *object, const char *end)
{
	const char *onboarding = servlist_json_object_member (object, end, "onboarding");
	const char *onboarding_end;
	const char *starter;
	const char *account;
	const char *account_end;
	const char *registration;
	const char *registration_end;
	char *value;

	if (!onboarding || *onboarding != '{')
		return;
	onboarding_end = servlist_json_value_end (onboarding, end);
	if (!onboarding_end)
		return;
	choice->onboarding_verified = servlist_json_member_bool (onboarding, onboarding_end, "verified", FALSE);
	if (!choice->onboarding_verified)
		return;

	value = servlist_json_member_string (onboarding, onboarding_end, "help_channel");
	if (value && strchr ("#&+!", value[0]))
		servlist_onboarding_set_string (&choice->help_channel, value);
	g_free (value);
	starter = servlist_json_object_member (onboarding, onboarding_end, "starter_channels");
	if (starter)
		servlist_json_add_string_array (starter, onboarding_end, choice);

	account = servlist_json_object_member (onboarding, onboarding_end, "account");
	if (!account || *account != '{')
		return;
	account_end = servlist_json_value_end (account, onboarding_end);
	if (!account_end)
		return;
	value = servlist_json_member_string (account, account_end, "identify_method");
	if (value)
	{
		servlist_onboarding_set_string (&choice->identify_method, value);
		if (!g_ascii_strcasecmp (value, "sasl"))
			choice->supports_sasl = TRUE;
		g_free (value);
	}

	registration = servlist_json_object_member (account, account_end, "registration");
	if (!registration || *registration != '{')
		return;
	registration_end = servlist_json_value_end (registration, account_end);
	if (!registration_end || !servlist_json_member_bool (registration, registration_end, "verified", FALSE))
		return;

	value = servlist_json_member_string (registration, registration_end, "service");
	if (value && servlist_onboarding_service_target_is_safe (value))
		servlist_onboarding_set_string (&choice->registration_service, value);
	g_free (value);
	value = servlist_json_member_string (registration, registration_end, "command");
	if (value && servlist_onboarding_registration_template_is_safe (value))
		servlist_onboarding_set_string (&choice->registration_template, value);
	g_free (value);
	choice->registration_email_required = servlist_json_member_bool (registration, registration_end, "requires_email", FALSE);
	choice->registration_verified = choice->registration_service && choice->registration_template &&
	                               (!choice->registration_email_required || strstr (choice->registration_template, "{email}"));
}

static void
servlist_onboarding_apply_atlas_document (ServlistOnboardingState *state, const char *json, gboolean client_feed)
{
	const char *end;
	const char *array;
	const char *p;

	if (!json || !*json)
		return;
	end = json + strlen (json);
	array = servlist_json_object_member (json, end, "networks");
	if (!array)
		array = servlist_json_object_member (json, end, "data");
	if (!array || *array != '[')
		return;
	p = array + 1;
	while (p < end)
	{
		const char *object_end;
		char *name;
		char *slug;
		char *description;
		char *homepage;
		ServlistOnboardingChoice *choice;
		gboolean added = FALSE;

		p = servlist_json_skip_ws (p, end);
		if (p >= end || *p == ']')
			break;
		if (*p != '{')
		{
			p = servlist_json_value_end (p, end);
			if (!p)
				break;
			goto next_value;
		}
		object_end = servlist_json_value_end (p, end);
		if (!object_end)
			break;
		name = servlist_json_member_string (p, object_end, "name");
		if (!name)
			name = servlist_json_member_string (p, object_end, "network");
		slug = servlist_json_member_string (p, object_end, "slug");
		if (!slug)
			slug = servlist_json_member_string (p, object_end, "id");
		choice = servlist_onboarding_find_choice (state, name, slug);
		if (!choice && client_feed && name && *name)
		{
			choice = servlist_onboarding_choice_new (NULL, name);
			g_ptr_array_add (state->choices, choice);
			added = TRUE;
		}
		if (choice)
		{
			if (slug && servlist_onboarding_slug_is_safe (slug))
				servlist_onboarding_set_string (&choice->slug, slug);
			description = servlist_json_member_string (p, object_end, "description");
			homepage = servlist_json_member_string (p, object_end, "homepage");
			if (!homepage)
				homepage = servlist_json_member_string (p, object_end, "homepage_url");
			if (!homepage)
				homepage = servlist_json_member_string (p, object_end, "website");
			if (description)
				servlist_onboarding_set_string (&choice->description, description);
			if (homepage)
				servlist_onboarding_set_string (&choice->homepage, homepage);
			choice->users = servlist_json_member_int (p, object_end, "users", choice->users);
			if (choice->users < 0)
				choice->users = servlist_json_member_int (p, object_end, "users_total", choice->users);
			servlist_onboarding_parse_services (choice, p, object_end);
			if (client_feed)
			{
				servlist_onboarding_parse_endpoint (choice, p, object_end);
				servlist_onboarding_parse_verified_metadata (choice, p, object_end);
			}
			g_free (description);
			g_free (homepage);
			if (added && !choice->atlas_approved)
			{
				g_ptr_array_remove (state->choices, choice);
				choice = NULL;
			}
		}
		g_free (name);
		g_free (slug);
		p = object_end;
next_value:
		p = servlist_json_skip_ws (p, end);
		if (p < end && *p == ',')
			p++;
	}
}

static void
servlist_onboarding_add_popular_channel (ServlistOnboardingChoice *choice, const char *channel)
{
	if (!choice || !channel || !*channel || !strchr ("#&+!", channel[0]))
		return;
	if (servlist_onboarding_ptr_array_contains (choice->starter_channels, channel) ||
	    servlist_onboarding_ptr_array_contains (choice->popular_channels, channel))
		return;
	if (choice->popular_channels->len >= 12)
		return;
	g_ptr_array_add (choice->popular_channels, g_strdup (channel));
}

static void
servlist_onboarding_apply_atlas_detail (ServlistOnboardingChoice *choice, const char *json)
{
	const char *end;
	const char *data;
	const char *data_end;
	const char *evidence;
	const char *channels;
	const char *p;
	char *value;

	if (!choice || !json || !*json)
		return;
	end = json + strlen (json);
	data = servlist_json_object_member (json, end, "data");
	if (!data || *data != '{')
		return;
	data_end = servlist_json_value_end (data, end);
	if (!data_end)
		return;

	value = servlist_json_member_string (data, data_end, "description");
	if (value)
	{
		servlist_onboarding_set_string (&choice->description, value);
		g_free (value);
	}
	value = servlist_json_member_string (data, data_end, "homepage_url");
	if (value)
	{
		servlist_onboarding_set_string (&choice->homepage, value);
		g_free (value);
	}
	choice->users = servlist_json_member_int (data, data_end, "users_total", choice->users);
	servlist_onboarding_parse_services (choice, data, data_end);

	evidence = servlist_json_object_member (data, data_end, "services_evidence");
	if (evidence && servlist_json_array_contains_text_ci (evidence, data_end, "nickserv"))
		choice->has_nickserv = TRUE;
	if (choice->services_family && !g_ascii_strcasecmp (choice->services_family, "Ergo integrated services") &&
	    choice->services_confidence >= 80)
		choice->has_nickserv = TRUE;

	servlist_onboarding_parse_verified_metadata (choice, data, data_end);

	g_ptr_array_set_size (choice->popular_channels, 0);
	channels = servlist_json_object_member (data, data_end, "channels");
	if (channels && *channels == '[')
	{
		p = channels + 1;
		while (p < data_end && choice->popular_channels->len < 12)
		{
			const char *item_end;
			char *name;

			p = servlist_json_skip_ws (p, data_end);
			if (p >= data_end || *p == ']')
				break;
			if (*p != '{')
			{
				p = servlist_json_value_end (p, data_end);
				if (!p)
					break;
			}
			else
			{
				item_end = servlist_json_value_end (p, data_end);
				if (!item_end)
					break;
				name = servlist_json_member_string (p, item_end, "name");
				if (name)
				{
					servlist_onboarding_add_popular_channel (choice, name);
					g_free (name);
				}
				p = item_end;
			}
			p = servlist_json_skip_ws (p, data_end);
			if (p < data_end && *p == ',')
				p++;
		}
	}

	choice->detail_loaded = TRUE;
}

static gboolean
servlist_onboarding_common_registration (ServlistOnboardingChoice *choice)
{
	if (!choice || !choice->has_nickserv || !choice->services_family || choice->services_confidence < 90)
		return FALSE;
	return !g_ascii_strcasecmp (choice->services_family, "Anope") ||
	       !g_ascii_strcasecmp (choice->services_family, "Atheme") ||
	       !g_ascii_strcasecmp (choice->services_family, "Ergo integrated services");
}

static gboolean
servlist_onboarding_can_prepare_registration (ServlistOnboardingChoice *choice)
{
	return choice && (choice->registration_verified || servlist_onboarding_common_registration (choice));
}

#ifdef WIN32
static gboolean
servlist_atlas_winhttp_cancelled (GCancellable *cancellable, GError **error)
{
	if (!cancellable || !g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if (error)
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation was cancelled");
	return TRUE;
}

static void
servlist_atlas_winhttp_error (GError **error, GCancellable *cancellable,
                              const char *operation, DWORD code)
{
	if (servlist_atlas_winhttp_cancelled (cancellable, error) || !error)
		return;

	g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
	             "Network Atlas %s failed (WinHTTP error %lu)",
	             operation, (unsigned long) code);
}

static char *
servlist_atlas_http_get (const char *path, GCancellable *cancellable, GError **error)
{
	static const wchar_t *accept_types[] = { L"application/json", NULL };
	HINTERNET session = NULL;
	HINTERNET connection = NULL;
	HINTERNET request = NULL;
	GByteArray *response = NULL;
	WCHAR wide_host[256];
	WCHAR wide_path[512];
	char buffer[8192];
	DWORD status = 0;
	DWORD status_size = sizeof status;
	DWORD read = 0;
	char *result = NULL;

	if (servlist_atlas_winhttp_cancelled (cancellable, error))
		return NULL;

	if (!path ||
	    MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, ONBOARDING_ATLAS_HOST, -1,
	                         wide_host, G_N_ELEMENTS (wide_host)) == 0 ||
	    MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
	                         wide_path, G_N_ELEMENTS (wide_path)) == 0)
	{
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		                     "Network Atlas request address is invalid");
		return NULL;
	}

	session = WinHttpOpen (L"ZoiteChat Network Atlas",
	                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
	                       WINHTTP_NO_PROXY_NAME,
	                       WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
	{
		servlist_atlas_winhttp_error (error, cancellable, "session setup", GetLastError ());
		goto done;
	}
	if (!WinHttpSetTimeouts (session, 8000, 8000, 8000, 8000))
	{
		servlist_atlas_winhttp_error (error, cancellable, "timeout setup", GetLastError ());
		goto done;
	}

	connection = WinHttpConnect (session, wide_host, 443, 0);
	if (!connection)
	{
		servlist_atlas_winhttp_error (error, cancellable, "connection", GetLastError ());
		goto done;
	}

	request = WinHttpOpenRequest (connection, L"GET", wide_path, NULL,
	                              WINHTTP_NO_REFERER, accept_types,
	                              WINHTTP_FLAG_SECURE);
	if (!request)
	{
		servlist_atlas_winhttp_error (error, cancellable, "request setup", GetLastError ());
		goto done;
	}

	if (!WinHttpSendRequest (request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
	                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
	{
		servlist_atlas_winhttp_error (error, cancellable, "request", GetLastError ());
		goto done;
	}
	if (!WinHttpReceiveResponse (request, NULL))
	{
		servlist_atlas_winhttp_error (error, cancellable, "response", GetLastError ());
		goto done;
	}

	if (!WinHttpQueryHeaders (request,
	                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
	                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
	                          WINHTTP_NO_HEADER_INDEX))
	{
		servlist_atlas_winhttp_error (error, cancellable, "status check", GetLastError ());
		goto done;
	}
	if (status != 200)
	{
		if (error)
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			             "Network Atlas returned HTTP status %lu", (unsigned long) status);
		goto done;
	}

	response = g_byte_array_new ();
	for (;;)
	{
		if (servlist_atlas_winhttp_cancelled (cancellable, error))
			goto done;

		read = 0;
		if (!WinHttpReadData (request, buffer, (DWORD) sizeof buffer, &read))
		{
			servlist_atlas_winhttp_error (error, cancellable, "read", GetLastError ());
			goto done;
		}
		if (read == 0)
			break;
		if ((guint64) response->len + read > ONBOARDING_ATLAS_MAX_RESPONSE)
		{
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			                     "Network Atlas response was too large");
			goto done;
		}
		g_byte_array_append (response, (const guint8 *) buffer, read);
	}

	g_byte_array_append (response, (const guint8 *) "\0", 1);
	result = (char *) g_byte_array_free (response, FALSE);
	response = NULL;

done:
	if (response)
		g_byte_array_free (response, TRUE);
	if (request)
		WinHttpCloseHandle (request);
	if (connection)
		WinHttpCloseHandle (connection);
	if (session)
		WinHttpCloseHandle (session);
	return result;
}
#else
static char *
servlist_atlas_http_get (const char *path, GCancellable *cancellable, GError **error)
{
	GSocketClient *client;
	GSocketConnection *connection;
	GOutputStream *output;
	GInputStream *input;
	GByteArray *response;
	char request[512];
	char buffer[8192];
	gsize written;
	gssize got;
	char *raw;
	char *body;
	gsize body_offset;

	client = g_socket_client_new ();
	g_socket_client_set_tls (client, TRUE);
	g_socket_client_set_timeout (client, 8);
	connection = g_socket_client_connect_to_host (client, ONBOARDING_ATLAS_HOST, 443, cancellable, error);
	g_object_unref (client);
	if (!connection)
		return NULL;

	output = g_io_stream_get_output_stream (G_IO_STREAM (connection));
	input = g_io_stream_get_input_stream (G_IO_STREAM (connection));
	g_snprintf (request, sizeof request,
	            "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: ZoiteChat/%s\r\nAccept: application/json\r\nConnection: close\r\n\r\n",
	            path, ONBOARDING_ATLAS_HOST, PACKAGE_VERSION);
	if (!g_output_stream_write_all (output, request, strlen (request), &written, cancellable, error) ||
	    !g_output_stream_flush (output, cancellable, error))
	{
		g_object_unref (connection);
		return NULL;
	}

	response = g_byte_array_new ();
	for (;;)
	{
		got = g_input_stream_read (input, buffer, sizeof buffer, cancellable, error);
		if (got < 0)
		{
			g_byte_array_free (response, TRUE);
			g_object_unref (connection);
			return NULL;
		}
		if (got == 0)
			break;
		if (response->len + (guint) got > ONBOARDING_ATLAS_MAX_RESPONSE)
		{
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			                     "Network Atlas response was too large");
			g_byte_array_free (response, TRUE);
			g_object_unref (connection);
			return NULL;
		}
		g_byte_array_append (response, (const guint8 *) buffer, (guint) got);
	}
	g_object_unref (connection);
	g_byte_array_append (response, (const guint8 *) "\0", 1);
	raw = (char *) g_byte_array_free (response, FALSE);
	if (!g_str_has_prefix (raw, "HTTP/1.0 200 ") && !g_str_has_prefix (raw, "HTTP/1.1 200 "))
	{
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Network Atlas returned a non-success HTTP status");
		g_free (raw);
		return NULL;
	}
	body = strstr (raw, "\r\n\r\n");
	if (!body)
	{
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Network Atlas returned an invalid HTTP response");
		g_free (raw);
		return NULL;
	}
	body += 4;
	body_offset = (gsize) (body - raw);
	memmove (raw, body, strlen (raw + body_offset) + 1);
	return raw;
}
#endif

static void
servlist_atlas_detail_thread (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	ServlistAtlasDetailRequest *request = task_data;
	ServlistAtlasDetailResult *result = g_new0 (ServlistAtlasDetailResult, 1);
	GError *error = NULL;
	char *path;
	(void) source_object;

	result->slug = g_strdup (request->slug);
	result->generation = request->generation;
	path = g_strdup_printf (ONBOARDING_ATLAS_PUBLIC_PATH "/%s", request->slug);
	result->json = servlist_atlas_http_get (path, cancellable, &error);
	g_free (path);
	if (!result->json && error && !g_cancellable_is_cancelled (cancellable))
		result->error = g_strdup (error->message);
	g_clear_error (&error);
	g_task_return_pointer (task, result, (GDestroyNotify) servlist_atlas_detail_result_free);
}

static void
servlist_atlas_fetch_thread (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	ServlistAtlasFetchResult *result = g_new0 (ServlistAtlasFetchResult, 1);
	GError *client_error = NULL;
	GError *public_error = NULL;
	(void) source_object;
	(void) task_data;

	result->client_json = servlist_atlas_http_get (ONBOARDING_ATLAS_CLIENT_PATH, cancellable, &client_error);
	if (!g_cancellable_is_cancelled (cancellable))
		result->public_json = servlist_atlas_http_get (ONBOARDING_ATLAS_PUBLIC_PATH, cancellable, &public_error);
	if (!result->client_json && !result->public_json && !g_cancellable_is_cancelled (cancellable))
	{
		result->error = g_strdup (client_error ? client_error->message :
		                          public_error ? public_error->message : "Network Atlas is unavailable");
	}
	g_clear_error (&client_error);
	g_clear_error (&public_error);
	g_task_return_pointer (task, result, (GDestroyNotify) servlist_atlas_fetch_result_free);
}

static ServlistOnboardingChoice *
servlist_onboarding_selected_choice (ServlistOnboardingState *state)
{
	int index = gtk_combo_box_get_active (GTK_COMBO_BOX (state->network_combo));
	if (index < 0 || (guint) index >= state->choices->len)
		return NULL;
	return g_ptr_array_index (state->choices, (guint) index);
}

static void
servlist_onboarding_update_network_info (ServlistOnboardingState *state)
{
	ServlistOnboardingChoice *choice = servlist_onboarding_selected_choice (state);
	GString *text = g_string_new (NULL);

	if (!choice)
		g_string_append (text, _("Choose an IRC network to continue."));
	else
	{
		if (choice->description && *choice->description)
			g_string_append (text, choice->description);
		else
			g_string_append_printf (text, _("%s is available in ZoiteChat's network list."), choice->name);
		if (choice->users >= 0)
			g_string_append_printf (text, _("\n\nAtlas recently saw about %d users on this network."), choice->users);
		if (choice->services_present && choice->services_family)
		{
			if (choice->services_confidence >= 0)
				g_string_append_printf (text, _("\nAccount services: %s (%d%% detection confidence)."),
				                        choice->services_family, choice->services_confidence);
			else
				g_string_append_printf (text, _("\nAccount services: %s."), choice->services_family);
		}
		if (choice->onboarding_verified)
			g_string_append (text, _("\nThis network publishes verified setup guidance through the ZoiteChat Network Atlas."));
		else if (choice->atlas_approved)
			g_string_append (text, _("\nConnection endpoint approved by the ZoiteChat Network Atlas."));
	}
	gtk_label_set_text (GTK_LABEL (state->network_info), text->str);
	g_string_free (text, TRUE);
}

static void
servlist_onboarding_detail_ready (GObject *source, GAsyncResult *async_result, gpointer userdata)
{
	ServlistOnboardingState *state = userdata;
	ServlistAtlasDetailResult *result;
	GError *error = NULL;
	ServlistOnboardingChoice *choice = NULL;
	(void) source;

	result = g_task_propagate_pointer (G_TASK (async_result), &error);
	if (state->detail_pending > 0)
		state->detail_pending--;

	if (!state->closing && result && result->slug)
	{
		choice = servlist_onboarding_find_choice (state, NULL, result->slug);
		if (choice && result->json)
			servlist_onboarding_apply_atlas_detail (choice, result->json);
		else if (choice)
			choice->detail_loaded = TRUE;
		if (choice && result->generation == state->detail_generation)
		{
			servlist_onboarding_update_network_info (state);
			if (state->account_skip)
				servlist_onboarding_account_changed (GTK_TOGGLE_BUTTON (state->account_skip), state);
			if (state->channel_suggestions && gtk_notebook_get_current_page (GTK_NOTEBOOK (state->notebook)) == 4)
				servlist_onboarding_refresh_channel_suggestions (state);
		}
	}

	if (result)
		servlist_atlas_detail_result_free (result);
	g_clear_error (&error);
	if (state->closing && !state->atlas_pending && state->detail_pending == 0)
		servlist_onboarding_state_free (state);
}

static void
servlist_onboarding_start_detail (ServlistOnboardingState *state, ServlistOnboardingChoice *choice)
{
	ServlistAtlasDetailRequest *request;
	GTask *task;

	if (!state || !choice || !servlist_onboarding_slug_is_safe (choice->slug) || choice->detail_loaded ||
	    !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (state->use_atlas)))
		return;

	if (state->detail_cancel)
	{
		g_cancellable_cancel (state->detail_cancel);
		g_object_unref (state->detail_cancel);
	}
	state->detail_cancel = g_cancellable_new ();
	state->detail_generation++;
	state->detail_pending++;
	request = g_new0 (ServlistAtlasDetailRequest, 1);
	request->slug = g_strdup (choice->slug);
	request->generation = state->detail_generation;
	task = g_task_new (NULL, state->detail_cancel, servlist_onboarding_detail_ready, state);
	g_task_set_task_data (task, request, (GDestroyNotify) servlist_atlas_detail_request_free);
	g_task_set_return_on_cancel (task, FALSE);
	g_task_run_in_thread (task, servlist_atlas_detail_thread);
	g_object_unref (task);
}

static void
servlist_onboarding_network_changed (GtkComboBox *combo, gpointer userdata)
{
	ServlistOnboardingState *state = userdata;
	(void) combo;
	servlist_onboarding_update_network_info (state);
	if (state->account_skip)
		servlist_onboarding_account_changed (GTK_TOGGLE_BUTTON (state->account_skip), state);
	servlist_onboarding_start_detail (state, servlist_onboarding_selected_choice (state));
}

static void
servlist_onboarding_refresh_combo (ServlistOnboardingState *state)
{
	int old = gtk_combo_box_get_active (GTK_COMBO_BOX (state->network_combo));
	guint i;

	gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (state->network_combo));
	for (i = 0; i < state->choices->len; i++)
	{
		ServlistOnboardingChoice *choice = g_ptr_array_index (state->choices, i);
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (state->network_combo), choice->name);
	}
	if (old < 0 || (guint) old >= state->choices->len)
		old = prefs.hex_gui_slist_select;
	if (old < 0 || (guint) old >= state->choices->len)
		old = 0;
	gtk_combo_box_set_active (GTK_COMBO_BOX (state->network_combo), old);
	servlist_onboarding_update_network_info (state);
}

static void
servlist_onboarding_atlas_ready (GObject *source, GAsyncResult *async_result, gpointer userdata)
{
	ServlistOnboardingState *state = userdata;
	ServlistAtlasFetchResult *result;
	GError *error = NULL;
	(void) source;

	result = g_task_propagate_pointer (G_TASK (async_result), &error);
	state->atlas_pending = FALSE;
	if (state->closing)
	{
		if (result)
			servlist_atlas_fetch_result_free (result);
		g_clear_error (&error);
		if (state->detail_pending == 0)
			servlist_onboarding_state_free (state);
		return;
	}

	if (error || !result || (!result->client_json && !result->public_json))
	{
		const char *message = error ? error->message : result && result->error ? result->error : _("Atlas is unavailable");
		char *status = g_strdup_printf (_("Network Atlas unavailable (%s). The built-in network list still works normally."), message);
		gtk_label_set_text (GTK_LABEL (state->atlas_status), status);
		g_free (status);
	}
	else
	{
		if (result->client_json)
			servlist_onboarding_apply_atlas_document (state, result->client_json, TRUE);
		if (result->public_json)
			servlist_onboarding_apply_atlas_document (state, result->public_json, FALSE);
		servlist_onboarding_refresh_combo (state);
		gtk_label_set_text (GTK_LABEL (state->atlas_status),
		                    _("Network Atlas loaded. Current approved endpoints and setup hints are available where published."));
	}
	if (result)
		servlist_atlas_fetch_result_free (result);
	g_clear_error (&error);
}

static void
servlist_onboarding_start_atlas (ServlistOnboardingState *state)
{
	GTask *task;

	if (state->atlas_pending)
		return;
	state->atlas_pending = TRUE;
	gtk_label_set_text (GTK_LABEL (state->atlas_status), _("Checking the ZoiteChat Network Atlas..."));
	task = g_task_new (NULL, state->atlas_cancel, servlist_onboarding_atlas_ready, state);
	g_task_set_return_on_cancel (task, FALSE);
	g_task_run_in_thread (task, servlist_atlas_fetch_thread);
	g_object_unref (task);
}

static void
servlist_onboarding_account_changed (GtkToggleButton *button, gpointer userdata)
{
	ServlistOnboardingState *state = userdata;
	ServlistOnboardingChoice *choice;
	gboolean existing;
	gboolean creating;
	gboolean can_prepare;
	gboolean needs_email;
	GString *info;
	(void) button;

	choice = servlist_onboarding_selected_choice (state);
	existing = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (state->account_existing));
	creating = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (state->account_new));
	can_prepare = creating && servlist_onboarding_can_prepare_registration (choice);
	needs_email = can_prepare && (!choice->registration_verified || choice->registration_email_required);
	gtk_widget_set_visible (state->account_label, existing);
	gtk_widget_set_visible (state->account_entry, existing);
	gtk_widget_set_visible (state->password_label, existing || can_prepare);
	gtk_widget_set_visible (state->password_entry, existing || can_prepare);
	gtk_widget_set_visible (state->password_confirm_label, can_prepare);
	gtk_widget_set_visible (state->password_confirm_entry, can_prepare);
	gtk_widget_set_visible (state->remember_password, existing || can_prepare);
	gtk_widget_set_visible (state->email_label, needs_email);
	gtk_widget_set_visible (state->email_entry, needs_email);

	info = g_string_new (NULL);
	if (!choice)
		g_string_append (info, _("Choose a network first."));
	else if (existing)
	{
		if (choice->supports_sasl || (choice->net && (choice->net->logintype == LOGIN_SASL ||
		    choice->net->logintype == LOGIN_SASL_SCRAM_SHA_1 ||
		    choice->net->logintype == LOGIN_SASL_SCRAM_SHA_256 ||
		    choice->net->logintype == LOGIN_SASL_SCRAM_SHA_512)))
			g_string_append (info, _("ZoiteChat can log in to your network account during connection with SASL, so you are already identified when you enter channels."));
		else if (choice->has_nickserv)
			g_string_append (info, _("This network appears to use NickServ. ZoiteChat can save the password and identify you after connecting."));
		else
			g_string_append (info, _("ZoiteChat will preserve this network's existing login method. If it uses an unusual account system, Advanced setup is still available."));
	}
	else if (creating)
	{
		if (choice->registration_verified)
		{
			g_string_append_printf (info, _("This network publishes an operator-verified registration recipe through Atlas. ZoiteChat will connect first and ask before sending the one-time registration request to %s."),
			                        choice->registration_service ? choice->registration_service : "NickServ");
		}
		else if (servlist_onboarding_common_registration (choice))
		{
			g_string_append_printf (info, _("Atlas detected %s with %d%% confidence and found NickServ. These services normally register accounts with a password and email address. ZoiteChat can prepare that common registration request, but the network may customize or disable registration, so nothing is sent until you confirm."),
			                        choice->services_family, choice->services_confidence);
		}
		else if (choice->has_nickserv)
		{
			g_string_append (info, _("Atlas found NickServ, but ZoiteChat does not have enough verified information to safely guess a password-bearing registration command. After connecting, ZoiteChat can ask NickServ for its registration help instead."));
		}
		else if (choice->services_present && choice->services_family)
		{
			g_string_append_printf (info, _("Atlas detected %s account services"), choice->services_family);
			if (choice->services_confidence >= 0)
				g_string_append_printf (info, _(" with %d%% confidence"), choice->services_confidence);
			g_string_append (info, _(". ZoiteChat will connect you first, but will not guess a command containing your password."));
		}
		else if (state->atlas_pending || (choice->slug && !choice->detail_loaded))
			g_string_append (info, _("ZoiteChat is still checking Atlas for this network's account setup information. You can continue while it loads."));
		else
			g_string_append (info, _("This network has not published enough account-registration information for automatic setup. You can still connect now and use the network's own help."));
	}
	else
		g_string_append (info, _("An IRC account is optional on many networks. You can start chatting with only a nickname and set up an account later."));
	gtk_label_set_text (GTK_LABEL (state->account_info), info->str);
	g_string_free (info, TRUE);
}

static void
servlist_onboarding_clear_box (GtkWidget *box)
{
	GList *children = gtk_container_get_children (GTK_CONTAINER (box));
	GList *node;
	for (node = children; node; node = node->next)
		gtk_widget_destroy (GTK_WIDGET (node->data));
	g_list_free (children);
}

static void
servlist_onboarding_refresh_channel_suggestions (ServlistOnboardingState *state)
{
	ServlistOnboardingChoice *choice = servlist_onboarding_selected_choice (state);
	guint i;
	guint shown = 0;

	servlist_onboarding_clear_box (state->channel_suggestions);
	if (choice && choice->starter_channels->len > 0)
	{
		GtkWidget *heading = gtk_label_new (_("Recommended starting channels"));
		gtk_widget_set_halign (heading, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (state->channel_suggestions), heading, FALSE, FALSE, 0);
		for (i = 0; i < choice->starter_channels->len && shown < 8; i++)
		{
			const char *channel = g_ptr_array_index (choice->starter_channels, i);
			GtkWidget *check = gtk_check_button_new_with_label (channel);
			g_object_set_data_full (G_OBJECT (check), "zoitechat-onboarding-channel", g_strdup (channel), g_free);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), shown == 0);
			gtk_box_pack_start (GTK_BOX (state->channel_suggestions), check, FALSE, FALSE, 0);
			shown++;
		}
	}
	if (choice && choice->help_channel && !servlist_onboarding_ptr_array_contains (choice->starter_channels, choice->help_channel))
	{
		GtkWidget *check = gtk_check_button_new_with_label (choice->help_channel);
		g_object_set_data_full (G_OBJECT (check), "zoitechat-onboarding-channel", g_strdup (choice->help_channel), g_free);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), shown == 0);
		gtk_box_pack_start (GTK_BOX (state->channel_suggestions), check, FALSE, FALSE, 0);
		shown++;
	}
	if (choice && choice->popular_channels->len > 0)
	{
		GtkWidget *heading = gtk_label_new (_("Popular public channels recently seen by Atlas"));
		GtkWidget *note = gtk_label_new (_("These are based on recent channel counts, not endorsements. Nothing here is joined unless you select it."));
		gtk_widget_set_halign (heading, GTK_ALIGN_START);
		gtk_label_set_line_wrap (GTK_LABEL (note), TRUE);
		gtk_label_set_xalign (GTK_LABEL (note), 0.0);
		gtk_box_pack_start (GTK_BOX (state->channel_suggestions), heading, FALSE, FALSE, shown ? 8 : 0);
		gtk_box_pack_start (GTK_BOX (state->channel_suggestions), note, FALSE, FALSE, 0);
		for (i = 0; i < choice->popular_channels->len && i < 8; i++)
		{
			const char *channel = g_ptr_array_index (choice->popular_channels, i);
			GtkWidget *check = gtk_check_button_new_with_label (channel);
			g_object_set_data_full (G_OBJECT (check), "zoitechat-onboarding-channel", g_strdup (channel), g_free);
			gtk_box_pack_start (GTK_BOX (state->channel_suggestions), check, FALSE, FALSE, 0);
			shown++;
		}
	}
	if (!shown)
	{
		GtkWidget *label = gtk_label_new (choice && choice->slug && !choice->detail_loaded ?
			_("Atlas is still loading channel information. You can wait a moment, type a channel below, or connect without joining one yet.") :
			_("No beginner channel recommendations are available for this network. You can leave this blank and choose a room after connecting."));
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_label_set_xalign (GTK_LABEL (label), 0.0);
		gtk_box_pack_start (GTK_BOX (state->channel_suggestions), label, FALSE, FALSE, 0);
	}
	gtk_widget_show_all (state->channel_suggestions);
}

static ServlistOnboardingAccountMode
servlist_onboarding_account_mode (ServlistOnboardingState *state)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (state->account_existing)))
		return ONBOARDING_ACCOUNT_EXISTING;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (state->account_new)))
		return ONBOARDING_ACCOUNT_NEW;
	return ONBOARDING_ACCOUNT_SKIP;
}

static void
servlist_onboarding_update_summary (ServlistOnboardingState *state)
{
	ServlistOnboardingChoice *choice = servlist_onboarding_selected_choice (state);
	ServlistOnboardingAccountMode mode = servlist_onboarding_account_mode (state);
	const char *nick = gtk_entry_get_text (GTK_ENTRY (state->nick_entry));
	GString *summary = g_string_new (NULL);

	g_string_append_printf (summary, _("Network: %s\nNickname: %s\n"),
	                        choice ? choice->name : _("(none)"), nick && *nick ? nick : _("(none)"));
	if (mode == ONBOARDING_ACCOUNT_EXISTING)
		g_string_append (summary, _("Account: use an existing network account\n"));
	else if (mode == ONBOARDING_ACCOUNT_NEW)
	{
		if (choice && choice->registration_verified)
			g_string_append (summary, _("Account: connect first, then use operator-verified Atlas registration guidance\n"));
		else if (servlist_onboarding_common_registration (choice))
			g_string_append (summary, _("Account: connect first, then confirm the common registration flow Atlas detected\n"));
		else if (choice && choice->has_nickserv)
			g_string_append (summary, _("Account: connect first, then ask NickServ for registration help\n"));
		else
			g_string_append (summary, _("Account: connect first, then use the network's own account help\n"));
	}
	else
		g_string_append (summary, _("Account: skip for now\n"));
	g_string_append (summary, _("Channels: only the rooms you selected or entered\n\n"));
	g_string_append (summary, _("ZoiteChat will save this network and nickname locally. Network Atlas lookups, when enabled, contain no nickname or password."));
	gtk_label_set_text (GTK_LABEL (state->summary_label), summary->str);
	g_string_free (summary, TRUE);
}

static void
servlist_onboarding_update_buttons (ServlistOnboardingState *state)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (state->notebook));
	int pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (state->notebook));
	gtk_widget_set_sensitive (state->back_button, page > 0);
	gtk_widget_set_visible (state->next_button, page < pages - 1);
	gtk_widget_set_visible (state->connect_button, page == pages - 1);
	if (page == pages - 1)
		servlist_onboarding_update_summary (state);
}

static GtkWidget *
servlist_onboarding_page (const char *title, const char *intro)
{
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	GtkWidget *heading;
	GtkWidget *label;
	char *markup = g_markup_printf_escaped ("<span size='x-large' weight='bold'>%s</span>", title);
	gtk_container_set_border_width (GTK_CONTAINER (box), 18);
	heading = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (heading), markup);
	gtk_label_set_xalign (GTK_LABEL (heading), 0.0);
	gtk_box_pack_start (GTK_BOX (box), heading, FALSE, FALSE, 0);
	g_free (markup);
	label = gtk_label_new (intro);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	return box;
}

static GtkWidget *
servlist_onboarding_labeled_entry (GtkWidget *grid, int row, const char *label_text, GtkWidget **out_label)
{
	GtkWidget *label = gtk_label_new (label_text);
	GtkWidget *entry = gtk_entry_new ();
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, row, 1, 1);
	if (out_label)
		*out_label = label;
	return entry;
}

static ircnet *
servlist_onboarding_ensure_network (ServlistOnboardingChoice *choice)
{
	ircnet *net;
	char *server_name;

	if (choice->net)
		return choice->net;
	if (!choice->atlas_approved || !choice->endpoint_host)
		return NULL;

	net = servlist_net_find (choice->name, NULL, g_ascii_strcasecmp);
	if (!net)
	{
		net = servlist_net_add (choice->name, NULL, FALSE);
		net->encoding = g_strdup (IRC_DEFAULT_CHARSET);
		if (choice->endpoint_tls)
			net->flags |= FLAG_USE_SSL;
		else
			net->flags &= ~FLAG_USE_SSL;
		if (choice->supports_sasl)
			net->logintype = LOGIN_SASL;
		server_name = g_strdup_printf (choice->endpoint_tls ? "%s/+%d" : "%s/%d",
		                               choice->endpoint_host, choice->endpoint_port);
		servlist_server_add (net, server_name);
		g_free (server_name);
	}
	choice->net = net;
	return net;
}

static void
servlist_onboarding_set_identity (ircnet *net, const char *nick, const char *realname, const char *account_name)
{
	const char *profile = realname && *realname ? realname : nick;
	const char *network_user = account_name && *account_name ? account_name : nick;
	g_strlcpy (prefs.hex_irc_nick1, nick, sizeof prefs.hex_irc_nick1);
	g_strlcpy (prefs.hex_irc_nick2, nick, sizeof prefs.hex_irc_nick2);
	g_strlcat (prefs.hex_irc_nick2, "_", sizeof prefs.hex_irc_nick2);
	g_strlcpy (prefs.hex_irc_nick3, nick, sizeof prefs.hex_irc_nick3);
	g_strlcat (prefs.hex_irc_nick3, "__", sizeof prefs.hex_irc_nick3);
	g_strlcpy (prefs.hex_irc_user_name, nick, sizeof prefs.hex_irc_user_name);
	g_strlcpy (prefs.hex_irc_real_name, profile, sizeof prefs.hex_irc_real_name);
	g_free (net->nick);
	net->nick = g_strdup (prefs.hex_irc_nick1);
	g_free (net->nick2);
	net->nick2 = g_strdup (prefs.hex_irc_nick2);
	g_free (net->user);
	net->user = g_strdup (network_user);
	g_free (net->real);
	net->real = g_strdup (profile);
	/* The assistant deliberately creates a complete per-network identity.
	 * This also lets an existing account name differ from the visible nick,
	 * which ZoiteChat's SASL path represents with the network user field. */
	net->flags &= ~FLAG_USE_GLOBAL;
}

static gboolean
servlist_onboarding_store_password (ircnet *net, const char *password)
{
	char *stored;

	if (!net || !password || !*password)
		return TRUE;

	if (secretstore_is_keyring_available () && secretstore_set_network_password (net->name, password))
	{
		if (net->pass)
		{
			memset (net->pass, 0, strlen (net->pass));
			g_free (net->pass);
			net->pass = NULL;
		}
		net->flags |= FLAG_USE_KEYRING;
		return TRUE;
	}

	stored = servlist_password_encrypt_for_storage (password);
	if (!stored)
		return FALSE;
	if (net->pass)
	{
		memset (net->pass, 0, strlen (net->pass));
		g_free (net->pass);
	}
	net->pass = stored;
	net->flags &= ~FLAG_USE_KEYRING;
	return TRUE;
}

static void
servlist_onboarding_clear_password (ircnet *net)
{
	if (!net)
		return;
	if (net->flags & FLAG_USE_KEYRING)
		secretstore_delete_network_password (net->name);
	net->flags &= ~FLAG_USE_KEYRING;
	if (net->pass)
	{
		memset (net->pass, 0, strlen (net->pass));
		g_free (net->pass);
		net->pass = NULL;
	}
}

static void
servlist_onboarding_add_channels_text (ircnet *net, const char *channels)
{
	char **parts;
	int i;

	if (!net || !channels || !*channels)
		return;
	parts = g_strsplit_set (channels, ", \t", -1);
	for (i = 0; parts[i]; i++)
	{
		if (*parts[i] && strchr ("#&+!", parts[i][0]) && !servlist_favchan_find (net, parts[i], NULL))
			servlist_favchan_add (net, parts[i]);
	}
	g_strfreev (parts);
}

static void
servlist_onboarding_add_selected_channels (ServlistOnboardingState *state, ircnet *net)
{
	GList *children = gtk_container_get_children (GTK_CONTAINER (state->channel_suggestions));
	GList *node;
	for (node = children; node; node = node->next)
	{
		GtkWidget *widget = node->data;
		const char *channel = g_object_get_data (G_OBJECT (widget), "zoitechat-onboarding-channel");
		if (channel && GTK_IS_TOGGLE_BUTTON (widget) && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) &&
		    !servlist_favchan_find (net, (char *) channel, NULL))
			servlist_favchan_add (net, (char *) channel);
	}
	g_list_free (children);
	servlist_onboarding_add_channels_text (net, gtk_entry_get_text (GTK_ENTRY (state->channel_entry)));
}

static int
servlist_onboarding_preferred_login (ServlistOnboardingChoice *choice, ircnet *net)
{
	if (choice && choice->identify_method)
	{
		if (!g_ascii_strcasecmp (choice->identify_method, "sasl"))
			return LOGIN_SASL;
		if (!g_ascii_strcasecmp (choice->identify_method, "nickserv"))
			return LOGIN_MSG_NICKSERV;
		if (!g_ascii_strcasecmp (choice->identify_method, "server_password"))
			return LOGIN_PASS;
	}
	if (choice && choice->supports_sasl)
		return LOGIN_SASL;
	if (net && net->logintype)
		return net->logintype;
	if (choice && choice->has_nickserv)
		return LOGIN_MSG_NICKSERV;
	return LOGIN_DEFAULT_REAL;
}

static void
servlist_onboarding_connect_password_once (session *sess, ircnet *net, const char *password)
{
	char *old_pass = net->pass;
	guint32 old_flags = net->flags;
	char *temporary = g_strdup (password ? password : "");

	net->pass = temporary;
	net->flags &= ~(FLAG_USE_KEYRING | FLAG_PROMPT_PASSWORD);
	servlist_connect (sess, net, TRUE);
	net->flags = old_flags;
	net->pass = old_pass;
	memset (temporary, 0, strlen (temporary));
	g_free (temporary);
}

static char *
servlist_onboarding_replace_all (const char *source, const char *needle, const char *replacement)
{
	GString *out = g_string_new (NULL);
	const char *p = source;
	const char *hit;
	gsize needle_len = strlen (needle);

	while ((hit = strstr (p, needle)) != NULL)
	{
		g_string_append_len (out, p, hit - p);
		g_string_append (out, replacement ? replacement : "");
		p = hit + needle_len;
	}
	g_string_append (out, p);
	return g_string_free (out, FALSE);
}

static char *
servlist_onboarding_expand_registration (ServlistRegistrationGuide *guide)
{
	char *step1;
	char *step2;
	char *step3;
	char *step4;
	char *step5;

	if (!guide || !guide->command_template || !servlist_onboarding_registration_template_is_safe (guide->command_template))
		return NULL;
	step1 = servlist_onboarding_replace_all (guide->command_template, "{nick}", guide->nick ? guide->nick : "");
	step2 = servlist_onboarding_replace_all (step1, "{nickname}", guide->nick ? guide->nick : "");
	g_free (step1);
	step3 = servlist_onboarding_replace_all (step2, "{account}", guide->nick ? guide->nick : "");
	g_free (step2);
	step4 = servlist_onboarding_replace_all (step3, "{password}", guide->password ? guide->password : "");
	g_free (step3);
	step5 = servlist_onboarding_replace_all (step4, "{email}", guide->email ? guide->email : "");
	g_free (step4);
	if (strchr (step5, '{') || strchr (step5, '}') || strchr (step5, '\r') || strchr (step5, '\n') || strlen (step5) > 350)
	{
		memset (step5, 0, strlen (step5));
		g_free (step5);
		return NULL;
	}
	return step5;
}

static void
servlist_registration_guide_free (ServlistRegistrationGuide *guide)
{
	if (!guide)
		return;
	g_free (guide->network_name);
	g_free (guide->nick);
	if (guide->password)
	{
		memset (guide->password, 0, strlen (guide->password));
		g_free (guide->password);
	}
	if (guide->email)
	{
		memset (guide->email, 0, strlen (guide->email));
		g_free (guide->email);
	}
	g_free (guide->service);
	g_free (guide->command_template);
	g_free (guide->homepage);
	g_free (guide->services_family);
	g_free (guide->identify_method);
	g_free (guide);
}

static gboolean
servlist_onboarding_login_uses_promptable_sasl (int login)
{
	return login == LOGIN_SASL || login == LOGIN_SASL_SCRAM_SHA_1 ||
	       login == LOGIN_SASL_SCRAM_SHA_256 || login == LOGIN_SASL_SCRAM_SHA_512;
}

static void
servlist_registration_configure_future_login (ServlistRegistrationGuide *guide)
{
	guide->net->logintype = guide->preferred_login;
	guide->net->flags &= ~FLAG_PROMPT_PASSWORD;
	if (guide->remember_password)
	{
		if (!servlist_onboarding_store_password (guide->net, guide->password))
			fe_message (_("The registration request was sent, but ZoiteChat could not save the password for future logins."), FE_MSG_WARN);
	}
	else
	{
		servlist_onboarding_clear_password (guide->net);
		if (servlist_onboarding_login_uses_promptable_sasl (guide->preferred_login))
			guide->net->flags |= FLAG_PROMPT_PASSWORD;
	}
	if (!servlist_save ())
		fe_message (_("Could not save the updated network login settings."), FE_MSG_WARN);
}

static gboolean
servlist_onboarding_persist_success (ServlistRegistrationGuide *guide)
{
	gboolean ok = TRUE;

	if (!guide || !guide->net)
		return FALSE;

	if (guide->account_mode == ONBOARDING_ACCOUNT_EXISTING)
	{
		guide->net->logintype = guide->preferred_login;
		guide->net->flags &= ~FLAG_PROMPT_PASSWORD;
		if (guide->remember_password)
		{
			if (!servlist_onboarding_store_password (guide->net, guide->password))
			{
				fe_message (_("You are connected, but ZoiteChat could not save the account password. Your network settings will still be kept."), FE_MSG_WARN);
				ok = FALSE;
			}
		}
		else
		{
			servlist_onboarding_clear_password (guide->net);
			if (servlist_onboarding_login_uses_promptable_sasl (guide->preferred_login))
				guide->net->flags |= FLAG_PROMPT_PASSWORD;
		}
	}

	prefs.hex_gui_onboarding_pending = FALSE;
	if (!servlist_save ())
	{
		fe_message (_("You are connected, but ZoiteChat could not save the server list."), FE_MSG_WARN);
		ok = FALSE;
	}
	if (!save_config ())
	{
		fe_message (_("You are connected, but ZoiteChat could not save zoitechat.conf."), FE_MSG_WARN);
		ok = FALSE;
	}
	return ok;
}

static void
servlist_onboarding_connected_show (ServlistRegistrationGuide *guide)
{
	GtkWidget *dialog;
	GtkWidget *area;
	GtkWidget *label;
	GString *text;

	if (!guide)
		return;
	dialog = gtk_dialog_new_with_buttons (_("You're connected to IRC"), NULL, GTK_DIALOG_MODAL,
	                                     _("Start chatting"), GTK_RESPONSE_OK,
	                                     NULL);
	if (current_sess && current_sess->gui && current_sess->gui->window)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (current_sess->gui->window));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 560, -1);
	area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	text = g_string_new (NULL);
	g_string_append_printf (text, _("You're online on %s as %s.\n\n"), guide->network_name, guide->nick);
	if (guide->account_mode == ONBOARDING_ACCOUNT_EXISTING)
		g_string_append (text, _("ZoiteChat used the account details you supplied. Watch the server tab for any login warning from the network.\n\n"));
	else if (guide->account_mode == ONBOARDING_ACCOUNT_NEW)
		g_string_append (text, _("Your first connection is working. If you sent a registration request, keep the server tab open long enough to read NickServ's reply and finish any verification it requested.\n\n"));
	else
		g_string_append (text, _("You can use IRC without registering an account. You can set one up later from the network's help or Network List.\n\n"));
	g_string_append (text, _("If you selected channels, ZoiteChat is joining them now. Click a channel in the left-hand list, type in the box at the bottom, and press Enter to talk. A private conversation with one person is called a query or direct message.\n\nYou can always open Network List later to add another network, change channels, or edit account login settings."));
	label = gtk_label_new (text->str);
	g_string_free (text, TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_container_set_border_width (GTK_CONTAINER (label), 16);
	gtk_box_pack_start (GTK_BOX (area), label, TRUE, TRUE, 0);
	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean
servlist_registration_confirm_automatic_login (ServlistRegistrationGuide *guide)
{
	GtkWidget *dialog;
	GtkWidget *area;
	GtkWidget *label;
	int response;

	dialog = gtk_dialog_new_with_buttons (_("Confirm your IRC account"), NULL, GTK_DIALOG_MODAL,
	                                     _("I'll finish this later"), GTK_RESPONSE_CANCEL,
	                                     _("Set up automatic login"), GTK_RESPONSE_OK,
	                                     NULL);
	if (current_sess && current_sess->gui && current_sess->gui->window)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (current_sess->gui->window));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 560, -1);
	area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	label = gtk_label_new (_("Read NickServ's reply in the server tab first. If it says the account was registered and asks you to confirm an email or code, complete that step before continuing.\n\nWhen the account is ready, ZoiteChat can save the password and configure normal automatic login. If you are unsure whether registration succeeded, choose 'I'll finish this later'. Your password will not be stored automatically."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_container_set_border_width (GTK_CONTAINER (label), 16);
	gtk_box_pack_start (GTK_BOX (area), label, TRUE, TRUE, 0);
	gtk_widget_show_all (dialog);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	return response == GTK_RESPONSE_OK;
}

static void
servlist_registration_show (ServlistRegistrationGuide *guide)
{
	GtkWidget *dialog;
	GtkWidget *area;
	GtkWidget *label;
	GString *text;
	int response;
	gboolean can_send;
	gboolean can_ask_help;

	can_send = guide->verified_recipe || guide->common_recipe;
	can_ask_help = guide->has_nickserv && guide->serv && guide->serv->p_message;
	dialog = gtk_dialog_new_with_buttons (_("Finish setting up your IRC account"), NULL, GTK_DIALOG_MODAL,
	                                     _("Not now"), GTK_RESPONSE_CANCEL,
	                                     NULL);
	if (can_send)
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Send registration request"), GTK_RESPONSE_OK);
	else if (can_ask_help)
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Ask NickServ how to register"), GTK_RESPONSE_APPLY);
	if (guide->homepage)
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Open network website"), GTK_RESPONSE_HELP);
	if (current_sess && current_sess->gui && current_sess->gui->window)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (current_sess->gui->window));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 560, -1);
	area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	text = g_string_new (NULL);
	g_string_append_printf (text, _("You are connected to %s as %s.\n\n"), guide->network_name, guide->nick);
	if (guide->verified_recipe)
	{
		g_string_append_printf (text, _("This network publishes an operator-verified account registration recipe through the ZoiteChat Network Atlas. If you continue, ZoiteChat will send one registration request to %s. The command is never added to reconnect commands."), guide->service);
		if (guide->email_required)
			g_string_append (text, _("\n\nThe network requires an email address. Read NickServ's reply and complete any email verification it requests."));
	}
	else if (guide->common_recipe)
	{
		g_string_append_printf (text, _("Atlas detected %s account services with %d%% confidence and found NickServ. This services family normally uses REGISTER with a password and email address. The network can customize or disable registration, so ZoiteChat will only send the request if you choose to continue."),
		                        guide->services_family, guide->services_confidence);
		g_string_append (text, _("\n\nAfter sending it, read NickServ's reply. If it asks for email verification, complete that before relying on automatic login."));
	}
	else
	{
		if (guide->services_family)
		{
			if (guide->services_confidence >= 0)
				g_string_append_printf (text, _("Atlas detected %s account services with %d%% confidence, but ZoiteChat does not have enough verified information to send your password in a registration command."),
				                        guide->services_family, guide->services_confidence);
			else
				g_string_append_printf (text, _("Atlas detected %s account services, but ZoiteChat does not have enough verified information to send your password in a registration command."), guide->services_family);
		}
		else
			g_string_append (text, _("Atlas does not currently have enough account-registration information for this network. ZoiteChat will not guess a password-bearing command."));
		if (can_ask_help)
			g_string_append (text, _("\n\nZoiteChat can safely ask NickServ for its REGISTER help without sending a password."));
	}
	label = gtk_label_new (text->str);
	g_string_free (text, TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_container_set_border_width (GTK_CONTAINER (label), 16);
	gtk_box_pack_start (GTK_BOX (area), label, TRUE, TRUE, 0);
	gtk_widget_show_all (dialog);

	for (;;)
	{
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		if (response == GTK_RESPONSE_HELP && guide->homepage)
		{
			fe_open_url (guide->homepage);
			continue;
		}
		if (response == GTK_RESPONSE_APPLY && can_ask_help)
		{
			guide->serv->p_message (guide->serv, "NickServ", "HELP REGISTER");
			fe_message (_("Asked NickServ for registration help. Read its reply in the server tab."), FE_MSG_INFO);
			break;
		}
		if (response == GTK_RESPONSE_OK && can_send)
		{
			char *expanded = servlist_onboarding_expand_registration (guide);
			if (!expanded || !servlist_onboarding_service_target_is_safe (guide->service))
			{
				if (expanded)
				{
					memset (expanded, 0, strlen (expanded));
					g_free (expanded);
				}
				fe_message (_("The Atlas registration instructions could not be expanded safely."), FE_MSG_ERROR);
				break;
			}
			guide->serv->p_message (guide->serv, guide->service, expanded);
			memset (expanded, 0, strlen (expanded));
			g_free (expanded);
			fe_message (_("Registration request sent. Read NickServ's reply in the server tab before confirming automatic login."), FE_MSG_INFO);
			if (servlist_registration_confirm_automatic_login (guide))
			{
				servlist_registration_configure_future_login (guide);
				fe_message (_("Automatic account login is now configured for future connections."), FE_MSG_INFO);
			}
			break;
		}
		break;
	}
	gtk_widget_destroy (dialog);
}

static int
servlist_registration_wait_cb (gpointer data)
{
	ServlistRegistrationGuide *guide = data;
	if (!is_server (guide->serv))
	{
		servlist_registration_guide_free (guide);
		return 0;
	}
	if (guide->serv->end_of_motd)
	{
		servlist_onboarding_persist_success (guide);
		if (guide->account_mode == ONBOARDING_ACCOUNT_NEW)
		{
			servlist_registration_show (guide);
			servlist_onboarding_connected_show (guide);
		}
		else
			servlist_onboarding_connected_show (guide);
		servlist_registration_guide_free (guide);
		return 0;
	}
	if (g_get_monotonic_time () >= guide->deadline)
	{
		fe_message (_("The first connection did not finish in time, so ZoiteChat did not mark onboarding complete. You can retry the connection or reopen Network List."), FE_MSG_WARN);
		servlist_registration_guide_free (guide);
		return 0;
	}
	return 1;
}

static gboolean
servlist_onboarding_validate_page (ServlistOnboardingState *state, int page)
{
	ServlistOnboardingChoice *choice;
	ServlistOnboardingAccountMode mode;
	const char *nick;
	const char *password;
	const char *password_confirm;
	const char *email;

	if (page == 0)
	{
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (state->use_atlas)))
			servlist_onboarding_start_atlas (state);
		return TRUE;
	}
	if (page == 1)
	{
		choice = servlist_onboarding_selected_choice (state);
		if (!choice)
		{
			fe_message (_("Choose an IRC network before continuing."), FE_MSG_ERROR);
			return FALSE;
		}
		return TRUE;
	}
	if (page == 2)
	{
		nick = gtk_entry_get_text (GTK_ENTRY (state->nick_entry));
		if (!nick || !*nick || strpbrk (nick, " \t\r\n,:"))
		{
			fe_message (_("Choose a nickname without spaces, commas, or colons."), FE_MSG_ERROR);
			gtk_widget_grab_focus (state->nick_entry);
			return FALSE;
		}
		return TRUE;
	}
	if (page == 3)
	{
		choice = servlist_onboarding_selected_choice (state);
		mode = servlist_onboarding_account_mode (state);
		password = gtk_entry_get_text (GTK_ENTRY (state->password_entry));
		password_confirm = gtk_entry_get_text (GTK_ENTRY (state->password_confirm_entry));
		email = gtk_entry_get_text (GTK_ENTRY (state->email_entry));
		if (mode == ONBOARDING_ACCOUNT_NEW && choice && choice->slug && !choice->detail_loaded &&
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (state->use_atlas)))
		{
			servlist_onboarding_start_detail (state, choice);
			fe_message (_("ZoiteChat is still checking this network's Atlas account information. Give it a moment, or choose 'Skip account setup for now' to connect immediately."), FE_MSG_INFO);
			return FALSE;
		}
		if (mode == ONBOARDING_ACCOUNT_EXISTING)
		{
			const char *account = gtk_entry_get_text (GTK_ENTRY (state->account_entry));
			if (!account || !*account || strpbrk (account, " \t\r\n"))
			{
				fe_message (_("Enter the account name you registered on this network. It cannot contain spaces."), FE_MSG_ERROR);
				return FALSE;
			}
			if (!password || !*password)
			{
				fe_message (_("Enter the password for your existing network account, or choose 'Skip account setup for now'."), FE_MSG_ERROR);
				return FALSE;
			}
		}
		if (mode == ONBOARDING_ACCOUNT_NEW && servlist_onboarding_can_prepare_registration (choice))
		{
			gboolean needs_email = !choice->registration_verified || choice->registration_email_required;
			if (!password || !*password || strpbrk (password, " \t\r\n") || strlen (password) < 10)
			{
				fe_message (_("Choose a registration password of at least 10 characters with no spaces. Do not reuse an important password from another service."), FE_MSG_ERROR);
				return FALSE;
			}
			if (!password_confirm || strcmp (password, password_confirm))
			{
				fe_message (_("The two registration password entries do not match."), FE_MSG_ERROR);
				gtk_widget_grab_focus (state->password_confirm_entry);
				return FALSE;
			}
			if (needs_email && (!email || !*email || !strchr (email, '@') || strpbrk (email, " \t\r\n")))
			{
				fe_message (_("Enter a valid email address for account registration."), FE_MSG_ERROR);
				return FALSE;
			}
		}
		return TRUE;
	}
	return TRUE;
}

static void
servlist_open_networks_window (session *sess)
{
	servlist_sess = sess;
	serverlist_win = servlist_open_networks ();
	gtkutil_set_icon (serverlist_win);
	servlist_networks_populate (networks_tree, network_list);
	g_signal_connect (G_OBJECT (serverlist_win), "delete-event", G_CALLBACK (servlist_delete_cb), 0);
	g_signal_connect (G_OBJECT (serverlist_win), "configure-event", G_CALLBACK (servlist_configure_cb), 0);
	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (networks_tree))),
	                  "changed", G_CALLBACK (servlist_network_row_cb), NULL);
	g_signal_connect (G_OBJECT (networks_tree), "key-press-event",
	                  G_CALLBACK (servlist_net_keypress_cb), networks_tree);
	gtk_widget_show (serverlist_win);
}

static gboolean
servlist_open_onboarding (session *sess)
{
	ServlistOnboardingState *state;
	GtkWidget *area;
	GtkWidget *page;
	GtkWidget *grid;
	GtkWidget *label;
	GtkWidget *scroll;
	GSList *group;
	GSList *list;
	int response;
	int page_index;
	ircnet *net;

	state = g_new0 (ServlistOnboardingState, 1);
	state->sess = sess;
	state->choices = g_ptr_array_new_with_free_func ((GDestroyNotify) servlist_onboarding_choice_free);
	state->atlas_cancel = g_cancellable_new ();
	for (list = network_list; list; list = list->next)
	{
		net = list->data;
		g_ptr_array_add (state->choices, servlist_onboarding_choice_new (net, net->name));
	}

	state->dialog = gtk_dialog_new_with_buttons (_("Welcome to ZoiteChat"), NULL, GTK_DIALOG_MODAL,
	                                             _("_Advanced setup..."), GTK_RESPONSE_HELP,
	                                             _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                             NULL);
	gtk_window_set_default_size (GTK_WINDOW (state->dialog), 650, 520);
	if (sess && sess->gui && sess->gui->window)
		gtk_window_set_transient_for (GTK_WINDOW (state->dialog), GTK_WINDOW (sess->gui->window));
	state->back_button = gtk_dialog_add_button (GTK_DIALOG (state->dialog), _("_Back"), GTK_RESPONSE_NO);
	state->next_button = gtk_dialog_add_button (GTK_DIALOG (state->dialog), _("_Next"), GTK_RESPONSE_YES);
	state->connect_button = gtk_dialog_add_button (GTK_DIALOG (state->dialog), _("C_onnect"), GTK_RESPONSE_OK);
	gtk_widget_set_can_default (state->next_button, TRUE);
	gtk_widget_set_can_default (state->connect_button, TRUE);

	area = gtk_dialog_get_content_area (GTK_DIALOG (state->dialog));
	state->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (state->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (state->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (area), state->notebook, TRUE, TRUE, 0);

	page = servlist_onboarding_page (_("Welcome to IRC"),
	        _("IRC is a collection of independent chat communities called networks. You do not need to know server addresses, commands, or IRC terminology to get started. This assistant will choose a community, set your public chat name, explain accounts, pick rooms, and connect you."));
	label = gtk_label_new (_("ZoiteChat can use the public Network Atlas for current network information and verified setup guidance. This sends a normal HTTPS request to zoitechat.org. Your nickname and password are never included in that request."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_box_pack_start (GTK_BOX (page), label, FALSE, FALSE, 0);
	state->use_atlas = gtk_check_button_new_with_label (_("Use the ZoiteChat Network Atlas for current setup information"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->use_atlas), TRUE);
	gtk_box_pack_start (GTK_BOX (page), state->use_atlas, FALSE, FALSE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (state->notebook), page, NULL);

	page = servlist_onboarding_page (_("Choose a network"),
	        _("A network is an independent IRC community. Pick one that sounds useful to you. You can connect to several networks later."));
	state->network_combo = gtk_combo_box_text_new ();
	gtk_box_pack_start (GTK_BOX (page), state->network_combo, FALSE, FALSE, 0);
	state->atlas_status = gtk_label_new (_("Using ZoiteChat's built-in network list."));
	gtk_label_set_line_wrap (GTK_LABEL (state->atlas_status), TRUE);
	gtk_label_set_xalign (GTK_LABEL (state->atlas_status), 0.0);
	gtk_box_pack_start (GTK_BOX (page), state->atlas_status, FALSE, FALSE, 0);
	state->network_info = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (state->network_info), TRUE);
	gtk_label_set_xalign (GTK_LABEL (state->network_info), 0.0);
	gtk_box_pack_start (GTK_BOX (page), state->network_info, TRUE, TRUE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (state->notebook), page, NULL);
	g_signal_connect (state->network_combo, "changed", G_CALLBACK (servlist_onboarding_network_changed), state);
	servlist_onboarding_refresh_combo (state);

	page = servlist_onboarding_page (_("Choose your chat name"),
	        _("Your nickname is the public name people see in chat. It does not need to be your real name. If your first choice is already in use, ZoiteChat keeps two automatic fallback versions."));
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_box_pack_start (GTK_BOX (page), grid, FALSE, FALSE, 0);
	state->nick_entry = servlist_onboarding_labeled_entry (grid, 0, _("Nickname:"), NULL);
	/* Leave room for the automatic _ and __ fallback nicknames. */
	gtk_entry_set_max_length (GTK_ENTRY (state->nick_entry), NICKLEN - 3);
	gtk_entry_set_text (GTK_ENTRY (state->nick_entry), prefs.hex_irc_nick1);
	label = gtk_label_new (_("Examples: alex, MapleLeaf, codecat. Spaces are not allowed."));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);
	state->realname_entry = servlist_onboarding_labeled_entry (grid, 2, _("Profile name:"), NULL);
	gtk_entry_set_max_length (GTK_ENTRY (state->realname_entry), sizeof prefs.hex_irc_real_name - 1);
	gtk_entry_set_text (GTK_ENTRY (state->realname_entry), prefs.hex_irc_nick1);
	label = gtk_label_new (_("This IRC 'real name' field can be visible in WHOIS. ZoiteChat starts it with your nickname instead of exposing your computer account's real name; change it if you want."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 1, 3, 1, 1);
	gtk_notebook_append_page (GTK_NOTEBOOK (state->notebook), page, NULL);

	page = servlist_onboarding_page (_("Do you want a network account?"),
	        _("A nickname lets you chat immediately. A network account can reserve that nickname and lets the network recognize you when you return. Accounts belong to the IRC network, not to ZoiteChat."));
	state->account_skip = gtk_radio_button_new_with_label (NULL, _("Skip account setup for now"));
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (state->account_skip));
	state->account_existing = gtk_radio_button_new_with_label (group, _("I already have an account on this network"));
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (state->account_existing));
	state->account_new = gtk_radio_button_new_with_label (group, _("I'm new here. Help me reserve this nickname"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->account_skip), TRUE);
	gtk_box_pack_start (GTK_BOX (page), state->account_skip, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (page), state->account_existing, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (page), state->account_new, FALSE, FALSE, 0);
	state->account_info = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (state->account_info), TRUE);
	gtk_label_set_xalign (GTK_LABEL (state->account_info), 0.0);
	gtk_box_pack_start (GTK_BOX (page), state->account_info, FALSE, FALSE, 0);
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 8);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_box_pack_start (GTK_BOX (page), grid, FALSE, FALSE, 0);
	state->account_entry = servlist_onboarding_labeled_entry (grid, 0, _("Account name:"), &state->account_label);
	gtk_entry_set_text (GTK_ENTRY (state->account_entry), prefs.hex_irc_nick1);
	state->password_entry = servlist_onboarding_labeled_entry (grid, 1, _("Account password:"), &state->password_label);
	gtk_entry_set_visibility (GTK_ENTRY (state->password_entry), FALSE);
	gtk_entry_set_input_purpose (GTK_ENTRY (state->password_entry), GTK_INPUT_PURPOSE_PASSWORD);
	gtk_entry_set_max_length (GTK_ENTRY (state->password_entry), 200);
	state->password_confirm_entry = servlist_onboarding_labeled_entry (grid, 2, _("Confirm password:"), &state->password_confirm_label);
	gtk_entry_set_visibility (GTK_ENTRY (state->password_confirm_entry), FALSE);
	gtk_entry_set_input_purpose (GTK_ENTRY (state->password_confirm_entry), GTK_INPUT_PURPOSE_PASSWORD);
	gtk_entry_set_max_length (GTK_ENTRY (state->password_confirm_entry), 200);
	state->email_entry = servlist_onboarding_labeled_entry (grid, 3, _("Email address:"), &state->email_label);
	gtk_entry_set_max_length (GTK_ENTRY (state->email_entry), 254);
	state->remember_password = gtk_check_button_new_with_label (_("Remember this password for future connections"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->remember_password), TRUE);
	gtk_grid_attach (GTK_GRID (grid), state->remember_password, 1, 4, 1, 1);
	gtk_notebook_append_page (GTK_NOTEBOOK (state->notebook), page, NULL);
	g_signal_connect (state->account_skip, "toggled", G_CALLBACK (servlist_onboarding_account_changed), state);
	g_signal_connect (state->account_existing, "toggled", G_CALLBACK (servlist_onboarding_account_changed), state);
	g_signal_connect (state->account_new, "toggled", G_CALLBACK (servlist_onboarding_account_changed), state);
	servlist_onboarding_account_changed (GTK_TOGGLE_BUTTON (state->account_skip), state);

	page = servlist_onboarding_page (_("Choose where to start chatting"),
	        _("Channels are chat rooms inside a network. Networks can publish beginner-friendly starter rooms through Atlas. You can also type a channel name if someone invited you to one."));
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (scroll, -1, 180);
	state->channel_suggestions = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (scroll), state->channel_suggestions);
	gtk_box_pack_start (GTK_BOX (page), scroll, TRUE, TRUE, 0);
	grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_box_pack_start (GTK_BOX (page), grid, FALSE, FALSE, 0);
	state->channel_entry = servlist_onboarding_labeled_entry (grid, 0, _("Other channels (optional):"), NULL);
	gtk_entry_set_placeholder_text (GTK_ENTRY (state->channel_entry), _("#channel, #another"));
	gtk_notebook_append_page (GTK_NOTEBOOK (state->notebook), page, NULL);

	page = servlist_onboarding_page (_("Ready to connect"),
	        _("That's enough setup to get onto IRC. Nothing here is permanent: the Network List can change networks, nicknames, account login, servers, and channels later."));
	state->summary_label = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (state->summary_label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (state->summary_label), 0.0);
	gtk_box_pack_start (GTK_BOX (page), state->summary_label, TRUE, TRUE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (state->notebook), page, NULL);

	gtk_widget_show_all (state->dialog);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (state->notebook), 0);
	servlist_onboarding_update_buttons (state);

	for (;;)
	{
		response = gtk_dialog_run (GTK_DIALOG (state->dialog));
		page_index = gtk_notebook_get_current_page (GTK_NOTEBOOK (state->notebook));
		if (response == GTK_RESPONSE_HELP)
		{
			prefs.hex_gui_onboarding_disable = TRUE;
			prefs.hex_gui_onboarding_pending = FALSE;
			save_config ();
			gtk_widget_destroy (state->dialog);
			servlist_onboarding_close_state (state);
			servlist_open_networks_window (sess);
			return TRUE;
		}
		if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT)
		{
			gtk_widget_destroy (state->dialog);
			servlist_onboarding_close_state (state);
			return TRUE;
		}
		if (response == GTK_RESPONSE_NO)
		{
			if (page_index > 0)
				gtk_notebook_set_current_page (GTK_NOTEBOOK (state->notebook), page_index - 1);
			servlist_onboarding_update_buttons (state);
			continue;
		}
		if (response == GTK_RESPONSE_YES)
		{
			if (!servlist_onboarding_validate_page (state, page_index))
				continue;
			if (page_index == 1)
				servlist_onboarding_account_changed (GTK_TOGGLE_BUTTON (state->account_skip), state);
			if (page_index == 2 && state->account_entry)
				gtk_entry_set_text (GTK_ENTRY (state->account_entry), gtk_entry_get_text (GTK_ENTRY (state->nick_entry)));
			if (page_index == 3)
				servlist_onboarding_refresh_channel_suggestions (state);
			gtk_notebook_set_current_page (GTK_NOTEBOOK (state->notebook), page_index + 1);
			servlist_onboarding_update_buttons (state);
			continue;
		}
		if (response == GTK_RESPONSE_OK)
		{
			ServlistOnboardingChoice *choice = servlist_onboarding_selected_choice (state);
			ServlistOnboardingAccountMode mode = servlist_onboarding_account_mode (state);
			const char *nick = gtk_entry_get_text (GTK_ENTRY (state->nick_entry));
			const char *password = gtk_entry_get_text (GTK_ENTRY (state->password_entry));
			const char *email = gtk_entry_get_text (GTK_ENTRY (state->email_entry));
			gboolean remember = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (state->remember_password));
			int preferred_login;
			ServlistRegistrationGuide *guide = NULL;
			server *target_server;

			if (!choice || !servlist_onboarding_validate_page (state, 2) || !servlist_onboarding_validate_page (state, 3))
				continue;
			net = servlist_onboarding_ensure_network (choice);
			if (!net)
			{
				fe_message (_("ZoiteChat could not create a usable connection for the selected network. Try Advanced setup."), FE_MSG_ERROR);
				continue;
			}
			servlist_onboarding_set_identity (net, nick, gtk_entry_get_text (GTK_ENTRY (state->realname_entry)),
			                                  mode == ONBOARDING_ACCOUNT_EXISTING ? gtk_entry_get_text (GTK_ENTRY (state->account_entry)) : nick);
			servlist_onboarding_add_selected_channels (state, net);
			preferred_login = servlist_onboarding_preferred_login (choice, net);
			prefs.hex_gui_slist_select = g_slist_index (network_list, net);

			guide = g_new0 (ServlistRegistrationGuide, 1);
			guide->account_mode = mode;
			guide->network_name = g_strdup (choice->name);
			guide->nick = g_strdup (nick);
			guide->homepage = g_strdup (choice->homepage);
			guide->services_family = g_strdup (choice->services_family);
			guide->identify_method = g_strdup (choice->identify_method);
			guide->services_confidence = choice->services_confidence;
			guide->preferred_login = preferred_login;
			guide->remember_password = remember;
			guide->has_nickserv = choice->has_nickserv;
			guide->net = net;

			if (mode == ONBOARDING_ACCOUNT_EXISTING)
			{
				/* Connect with the supplied credential once, but do not persist it until
				 * the network has actually accepted the connection. */
				guide->password = g_strdup (password);
				net->logintype = preferred_login;
			}
			else if (mode == ONBOARDING_ACCOUNT_NEW)
			{
				gboolean common_recipe = servlist_onboarding_common_registration (choice);
				gboolean can_prepare = choice->registration_verified || common_recipe;
				guide->password = can_prepare ? g_strdup (password) : NULL;
				guide->email = can_prepare ? g_strdup (email) : NULL;
				guide->service = g_strdup (choice->registration_verified ? choice->registration_service :
				                           common_recipe ? "NickServ" : NULL);
				guide->command_template = g_strdup (choice->registration_verified ? choice->registration_template :
				                                    common_recipe ? "REGISTER {password} {email}" : NULL);
				guide->email_required = choice->registration_verified ? choice->registration_email_required : common_recipe;
				guide->verified_recipe = choice->registration_verified;
				guide->common_recipe = common_recipe;
				/* A brand-new account cannot authenticate before it exists. Keep the
				 * first connection unauthenticated; the guide switches the saved
				 * login method only after a registration request is sent. */
				net->logintype = LOGIN_PASS;
				servlist_onboarding_clear_password (net);
				net->flags &= ~FLAG_PROMPT_PASSWORD;
			}

			if (!is_session (sess))
				sess = current_sess;
			if (!sess)
				sess = new_ircwindow (NULL, NULL, SESS_SERVER, TRUE);
			target_server = sess->server;
			if (mode == ONBOARDING_ACCOUNT_EXISTING)
				servlist_onboarding_connect_password_once (sess, net, password);
			else
				servlist_connect (sess, net, TRUE);

			guide->serv = target_server;
			guide->deadline = g_get_monotonic_time () + (gint64) ONBOARDING_REGISTRATION_TIMEOUT_SECONDS * G_USEC_PER_SEC;
			fe_timeout_add (500, servlist_registration_wait_cb, guide);
			gtk_entry_set_text (GTK_ENTRY (state->password_entry), "");
			gtk_entry_set_text (GTK_ENTRY (state->password_confirm_entry), "");
			gtk_entry_set_text (GTK_ENTRY (state->email_entry), "");
			gtk_widget_destroy (state->dialog);
			servlist_onboarding_close_state (state);
			return TRUE;
		}
	}
}

void
fe_serverlist_open (session *sess)
{
	if (serverlist_win)
	{
		gtk_window_present (GTK_WINDOW (serverlist_win));
		return;
	}
	if (!prefs.hex_gui_onboarding_disable &&
	    (servlist_needs_onboarding () || prefs.hex_gui_onboarding_pending))
	{
		if (servlist_needs_onboarding () && !prefs.hex_gui_onboarding_pending)
		{
			/* Persist a small pending marker immediately. The normal shutdown path
			 * can create servlist.conf even after a cancelled or failed first
			 * connection; without this marker a newcomer would lose the assistant
			 * on the next launch. */
			prefs.hex_gui_onboarding_pending = TRUE;
			if (!save_config ())
				fe_message (_("Could not save the first-run assistant state."), FE_MSG_WARN);
		}
		if (servlist_open_onboarding (sess))
			return;
	}
	servlist_open_networks_window (sess);
}
