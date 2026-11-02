/*
 * ZoiteChat: provisional Qt 6 frontend implementation.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The C engine continues to own sessions, sockets, and plugin contexts.
 * Functionality not yet in this first milestone is deliberately marked.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/zoitechat.h"
#include <glib-object.h>
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/outbound.h"
#include "../common/userlist.h"
#include "../common/text.h"
#include "../common/util.h"
#include "../common/fe.h"
#include "qt-bridge.h"
#include "tools-bridge.h"
#include "moderation-bridge.h"
#include "friends-bridge.h"
#include "customization-bridge.h"
#include "textevents-bridge.h"
#include "addons-bridge.h"
#include "transfers-bridge.h"
#include "../common/dcc.h"

static int qt_argc;
static char **qt_argv;
static char *option_cfgdir;
static gint show_version;
static gint show_cfgdir;
static gint startup_minimize;

static GOptionEntry qt_options[] = {
#ifdef __linux__
  { "run-as-root", 0, 0, G_OPTION_ARG_NONE, &arg_run_as_root, "Allow ZoiteChat to run as root", NULL },
#endif
  { "minimize", 0, 0, G_OPTION_ARG_INT, &startup_minimize, "Begin minimized: 0 normal, 1 iconified, 2 tray", "LEVEL" },
  { "no-auto", 'a', 0, G_OPTION_ARG_NONE, &arg_dont_autoconnect, "Don't auto connect", NULL },
  { "cfgdir", 'd', 0, G_OPTION_ARG_STRING, &option_cfgdir, "Use configuration directory", "PATH" },
  { "no-plugins", 'n', 0, G_OPTION_ARG_NONE, &arg_skip_plugins, "Don't autoload plugins", NULL },
  { "configdir", 'u', 0, G_OPTION_ARG_NONE, &show_cfgdir, "Display configuration directory", NULL },
  { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, "Display version", NULL },
  { "url", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_url, "IRC URL", "URL" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &arg_urls, "IRC URLs", "URL" },
  { NULL }
};

int
fe_args(int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context = g_option_context_new(NULL);
  g_option_context_add_main_entries(context, qt_options, GETTEXT_PACKAGE);
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_printerr("zoitechat-qt: %s\n", error->message);
    g_clear_error(&error);
    g_option_context_free(context);
    return 1;
  }
  g_option_context_free(context);
  if (startup_minimize < 0 || startup_minimize > 2) {
    g_printerr("zoitechat-qt: --minimize must be 0, 1, or 2\n");
    return 1;
  }
  if (show_version) {
    g_print("%s %s (Qt 6)\n", PACKAGE_NAME, PACKAGE_VERSION);
    return 0;
  }
  if (option_cfgdir) {
    g_free(xdir);
    xdir = g_strdup(option_cfgdir);
    g_strchomp(xdir);
    g_free(option_cfgdir);
    option_cfgdir = NULL;
  }
  if (show_cfgdir) {
    g_print("%s\n", get_xdir());
    return 0;
  }
  qt_argc = argc;
  qt_argv = argv;
  return -1;
}

void fe_init(void)
{
  qt_fe_initialize(&qt_argc, qt_argv);
}
void fe_main(void) { qt_fe_startup_state(startup_minimize); (void)qt_fe_run(); }
void fe_exit(void) { qt_fe_quit(); }
void fe_cleanup(void) { }

void fe_new_window(session *sess, int focus)
{
  if (!sess->server->front_session)
    sess->server->front_session = sess;
  if (!sess->server->server_session)
    sess->server->server_session = sess;
  /* Qt activates the actual selected conversation, respecting the native focus policy. */
  qt_fe_new_session(sess, sess->channel[0] ? sess->channel : sess->type == SESS_CHANNEL ? "(not joined)" : "Server", focus);
}
void fe_new_server(server *serv) { (void)serv; }
void fe_close_window(session *sess) { session_free(sess); }
void fe_session_callback(session *sess)
{
  /* A result tab may outlive its source. Do not let a reused session address
   * turn a repeat search into a search of an unrelated conversation. */
  for (GSList *entry = sess_list; entry; entry = entry->next) {
    session *other = entry->data;
    if (other->lastlog_sess == sess) other->lastlog_sess = NULL;
  }
  qt_fe_remove_session(sess);
}
void fe_server_callback(server *serv) { qt_fe_tools_server_closed(serv); }

void fe_print_text(session *sess, char *text, time_t stamp, gboolean no_activity)
{
  qt_fe_print(sess, text, stamp);
  if (!no_activity && (sess->tab_state & (TAB_STATE_NEW_MSG | TAB_STATE_NEW_HILIGHT)) &&
      (sess->tab_state & (TAB_STATE_NEW_MSG | TAB_STATE_NEW_HILIGHT)) !=
      (sess->last_tab_state & (TAB_STATE_NEW_MSG | TAB_STATE_NEW_HILIGHT)))
    qt_fe_message_alert(sess, text, !!(sess->tab_state & TAB_STATE_NEW_HILIGHT), sess->type == SESS_DIALOG);
  if (!no_activity)
    fe_set_tab_color(sess, sess == current_tab ? FE_COLOR_NONE :
                          (sess->tab_state & TAB_STATE_NEW_HILIGHT) ? FE_COLOR_NEW_HILIGHT :
                          (sess->tab_state & TAB_STATE_NEW_MSG) ? FE_COLOR_NEW_MSG : FE_COLOR_NEW_DATA);
}
void fe_text_clear(session *sess, int lines) { qt_fe_clear_lines(sess, lines); }
void fe_clear_channel(session *sess) { qt_fe_moderation_closed(sess); qt_fe_topic(sess, ""); }
void fe_set_topic(session *sess, char *topic, char *stripped_topic)
{
  qt_fe_topic(sess, topic ? topic : stripped_topic);
}
void fe_set_channel(session *sess)
{
  const char *name = sess->channel[0] ? sess->channel :
                     sess->type == SESS_CHANNEL ? sess->waitchannel : "Server";
  if (sess->type == SESS_CHANNEL && !sess->channel[0]) {
    char *label = g_strdup_printf("(%s)", *name ? name : "not joined");
    qt_fe_rename_session(sess, label);
    g_free(label);
  } else {
    qt_fe_rename_session(sess, name);
  }
}
void fe_set_title(session *sess) { fe_set_channel(sess); }
void fe_set_tab_color(session *sess, tabcolor color)
{
  int level = color & ~FE_COLOR_ALLFLAGS;
  if (sess == current_tab) level = FE_COLOR_NONE;
  if (level == FE_COLOR_NONE) sess->tab_state = TAB_STATE_NONE;
  else if (level == FE_COLOR_NEW_HILIGHT) sess->tab_state = TAB_STATE_NEW_HILIGHT;
  else if (level == FE_COLOR_NEW_MSG &&
           (!(color & FE_COLOR_FLAG_NOOVERRIDE) || !(sess->tab_state & TAB_STATE_NEW_HILIGHT)))
    sess->tab_state = TAB_STATE_NEW_MSG;
  else if (level == FE_COLOR_NEW_DATA &&
           (!(color & FE_COLOR_FLAG_NOOVERRIDE) || !(sess->tab_state & (TAB_STATE_NEW_MSG | TAB_STATE_NEW_HILIGHT))))
    sess->tab_state = TAB_STATE_NEW_DATA;
  lastact_update(sess);
  sess->last_tab_state = sess->tab_state;
  qt_fe_activity(sess, sess->tab_state == TAB_STATE_NEW_HILIGHT ? FE_COLOR_NEW_HILIGHT :
                       sess->tab_state == TAB_STATE_NEW_MSG ? FE_COLOR_NEW_MSG :
                       sess->tab_state == TAB_STATE_NEW_DATA ? FE_COLOR_NEW_DATA : FE_COLOR_NONE);
}
void fe_flash_window(session *sess) { qt_fe_attention(sess); }
void fe_message(char *text, int flags)
{
  if (qt_core_text_event_capture_message(text)) return;
  qt_fe_message(text, (flags & (FE_MSG_WARN | FE_MSG_ERROR)) != 0);
}
void fe_beep(session *sess) { (void)sess; qt_fe_beep(); }

static void qt_user_details(session *sess, struct User *user)
{
  int rank;
  for (rank = 0; rank < USERACCESS_SIZE; ++rank) if (user->access & (1U << rank)) break;
  qt_fe_user_details(sess, user->nick, user->hostname, user->realname, user->account,
                     user->servername, user->away, user->me, rank);
}

void fe_userlist_insert(session *sess, struct User *user, gboolean select)
{
  if (user) { qt_user_details(sess, user); qt_fe_user_add(sess, user->nick, user->prefix, select || user->selected); }
}
int fe_userlist_remove(session *sess, struct User *user)
{
  if (!user) return 0;
  user->selected = qt_fe_user_selected(sess, user->nick);
  qt_fe_user_remove(sess, user->nick);
  return user->selected;
}
void fe_userlist_rehash(session *sess, struct User *user)
{
  if (user) { qt_user_details(sess, user); qt_fe_user_add(sess, user->nick, user->prefix, FALSE); }
}
void fe_userlist_update(session *sess, struct User *user) { fe_userlist_rehash(sess, user); }
void fe_userlist_clear(session *sess) { qt_fe_users_clear(sess); }
void fe_userlist_numbers(session *sess) { (void)sess; }
void fe_userlist_set_selected(session *sess)
{
  GSList *users = userlist_flat_list(sess), *entry;
  for (entry = users; entry; entry = entry->next) {
    struct User *user = entry->data;
    user->selected = qt_fe_user_selected(sess, user->nick);
  }
  g_slist_free(users);
}
void fe_uselect(session *sess, char *word[], int clear, int scroll_to)
{
  int i;
  if (clear) qt_fe_user_select(sess, NULL, TRUE, FALSE);
  for (i = 0; word[i] && *word[i]; ++i) {
    /* Let the IRC engine resolve the server's nickname case mapping. */
    struct User *user = userlist_find(sess, word[i]);
    if (user) qt_fe_user_select(sess, user->nick, FALSE, scroll_to);
  }
}

void fe_ctrl_gui(session *sess, fe_gui_action action, int arg)
{
  if (action == FE_GUI_COLOR) fe_set_tab_color(sess, arg);
  else if (action == FE_GUI_FLASH) fe_flash_window(sess);
  else qt_fe_gui_control(sess, action, arg);
}
int fe_gui_info(session *sess, int type)
{ return type == 0 ? qt_fe_session_window_status(sess) : -1; }
void *fe_gui_info_ptr(session *sess, int type)
{ (void)sess; (void)type; return NULL; } /* no GTK widget is exposed */

static char *input_cache;
char *fe_get_inputbox_contents(session *sess)
{
  g_free(input_cache);
  input_cache = g_strdup(qt_fe_get_input(sess));
  return input_cache;
}
void fe_set_inputbox_contents(session *sess, char *text) { qt_fe_set_input(sess, text); }
int fe_get_inputbox_cursor(session *sess)
{ return qt_fe_get_cursor(sess); }
void fe_set_inputbox_cursor(session *sess, int delta, int pos)
{ qt_fe_set_cursor(sess, delta, pos); }
void fe_open_url(const char *url) { qt_fe_open_url(url); }

int fe_timeout_add(int interval, void *callback, void *userdata)
{ return (int)g_timeout_add(interval, (GSourceFunc)callback, userdata); }
int fe_timeout_add_seconds(int interval, void *callback, void *userdata)
{ return (int)g_timeout_add_seconds(interval, (GSourceFunc)callback, userdata); }
void fe_timeout_remove(int tag) { if (tag > 0) g_source_remove((guint)tag); }
void fe_idle_add(void *func, void *data) { g_idle_add((GSourceFunc)func, data); }
int fe_input_add(int socket, int flags, void *func, void *data)
{
  GIOChannel *channel;
  GIOCondition conditions = 0;
  guint id;
#ifdef G_OS_WIN32
  channel = (flags & FIA_FD) ?
    g_io_channel_win32_new_fd(socket) : g_io_channel_win32_new_socket(socket);
#else
  channel = g_io_channel_unix_new(socket);
#endif
  if (flags & FIA_READ) conditions |= G_IO_IN | G_IO_HUP | G_IO_ERR;
  if (flags & FIA_WRITE) conditions |= G_IO_OUT | G_IO_ERR;
  if (flags & FIA_EX) conditions |= G_IO_PRI;
  id = g_io_add_watch(channel, conditions, (GIOFunc)func, data);
  g_io_channel_unref(channel);
  return (int)id;
}
void fe_input_remove(int tag) { if (tag > 0) g_source_remove((guint)tag); }

/* Commands and events from Qt go straight through ZoiteChat's existing parser. */
void qt_core_activate(void *ptr)
{
  session *sess = ptr;
  if (!sess || !is_session(sess)) return;
  current_tab = current_sess = sess;
  sess->tab_state = TAB_STATE_NONE;
  sess->last_tab_state = TAB_STATE_NONE;
  lastact_update(sess);
  if (sess->server) sess->server->front_session = sess;
}
void qt_core_submit(void *ptr, const char *line)
{
  session *sess = ptr;
  char *mutable_line;
  if (!sess || !is_session(sess) || !line || !*line) return;
  qt_core_activate(sess);
  mutable_line = g_strdup(line);
  handle_multiline(sess, mutable_line, TRUE, FALSE);
  g_free(mutable_line);
}
void qt_core_quit(void) { if (!zoitechat_is_quitting) zoitechat_exit(); }
void qt_core_submit_literal(void *ptr, const char *line)
{
  session *sess = ptr;
  char *text;
  if (!sess || !is_session(sess) || !line || !*line) return;
  qt_core_activate(sess); text = g_strdup(line);
  handle_multiline(sess, text, TRUE, TRUE);
  g_free(text);
}

const char *qt_core_config_dir(void) { return get_xdir(); }
int qt_core_nick_color(const char *nick) { return nick && *nick ? text_color_of((char *)nick) : -1; }

int qt_core_session_info(void *ptr, QtSessionInfo *info)
{
  session *sess = ptr;
  server *serv;
  if (!sess || !info || !is_session(sess)) return FALSE;
  serv = sess->server;
  info->channel = sess->type == SESS_CHANNEL && sess->channel[0];
  info->channel_session = sess->type == SESS_CHANNEL;
  info->server = sess->type == SESS_SERVER;
  info->connected = serv->connected;
  info->connecting = serv->connecting;
  info->away = serv->is_away;
  info->nick = serv->nick;
  info->nick_prefixes = serv->nick_prefixes;
  info->host = serv->servername[0] ? serv->servername : serv->hostname;
  info->query = sess->type == SESS_DIALOG;
  info->reconnect_wait = serv->recondelay_tag != 0;
  info->registering = serv->connected && !serv->end_of_motd;
  info->lag_pending = serv->lag_sent != 0;
  unsigned long pending = info->lag_pending ? make_ping_time() - serv->lag_sent : 0;
  info->lag_ms = info->lag_pending ? (int)MIN(pending, 30000UL) : MAX(serv->lag, 0);
  info->queued_bytes = MAX(serv->sendq_len, 0);
  return TRUE;
}

void qt_core_set_topic(void *ptr, const char *topic)
{
  session *sess = ptr;
  if (!sess || !is_session(sess) || sess->type != SESS_CHANNEL ||
      !sess->server->connected || !sess->channel[0] || !topic ||
      strchr(topic, '\r') || strchr(topic, '\n')) return;
  /* Empty text clears a topic; an empty string otherwise requests the topic. */
  sess->server->p_topic(sess->server, sess->channel, *topic ? (char *)topic : NULL);
}

/* These callbacks are intentionally incomplete in milestone one.
 * The Qt UI is a working chat/command prototype, NOT GTK feature parity. */
void fe_add_rawlog(server *serv, char *text, int len, int outbound)
{ qt_fe_rawlog(serv, text, len, outbound); }
void fe_update_mode_buttons(session *sess, char mode, char sign)
{ (void)sess; (void)mode; (void)sign; qt_fe_connection_changed(); }
void fe_update_channel_key(session *sess) { (void)sess; qt_fe_connection_changed(); }
void fe_update_channel_limit(session *sess) { (void)sess; qt_fe_connection_changed(); }
int fe_is_chanwindow(server *serv) { return qt_fe_channel_browser_open(serv); }
void fe_add_chan_list(server *serv, char *chan, char *users, char *topic)
{ qt_fe_channel_row(serv, chan, users, topic); }
void fe_chan_list_end(server *serv) { qt_fe_channel_end(serv); }
gboolean fe_add_ban_list(session *sess, char *mask, char *who, char *when, int rplcode)
{ return qt_fe_ban_row(sess, mask, who, when, rplcode); }
gboolean fe_ban_list_end(session *sess, int code)
{ return qt_fe_ban_end(sess, code); }
void fe_notify_update(char *name) { (void)name; qt_fe_friends_changed(); }
void fe_notify_ask(char *name, char *networks) { qt_fe_friend_ask(name, networks); }
void fe_progressbar_start(session *sess) { (void)sess; qt_fe_connection_changed(); }
void fe_progressbar_end(server *serv) { (void)serv; qt_fe_connection_changed(); }
void fe_dcc_add(struct DCC *dcc) { qt_core_transfer_track(dcc, FALSE); qt_fe_transfers_changed(); }
void fe_dcc_update(struct DCC *dcc) { qt_core_transfer_track(dcc, FALSE); qt_fe_transfers_changed(); }
void fe_dcc_remove(struct DCC *dcc) { qt_core_transfer_track(dcc, TRUE); qt_fe_transfers_changed(); }
int fe_dcc_open_recv_win(int passive) { return qt_fe_transfers_open(passive); }
int fe_dcc_open_send_win(int passive) { return qt_fe_transfers_open(passive); }
int fe_dcc_open_chat_win(int passive) { return qt_fe_direct_chats_open(passive); }
void fe_url_add(const char *url) { qt_fe_url_capture(url); }
void fe_pluginlist_update(void) { qt_fe_addons_changed(); }
void fe_buttons_update(session *sess) { (void)sess; qt_fe_customization_changed(QT_CONFIG_BUTTONS); }
void fe_dlgbuttons_update(session *sess) { (void)sess; qt_fe_customization_changed(QT_CONFIG_DIALOG_BUTTONS); }
void fe_dcc_send_filereq(session *sess, char *nick, int maxcps, int passive)
{ qt_fe_transfer_send_request(sess, nick, maxcps, passive); }
void fe_set_nonchannel(session *sess, int state) { (void)sess; (void)state; }
void fe_set_nick(server *serv, char *nick) { (void)serv; (void)nick; qt_fe_connection_changed(); }
void fe_ignore_update(int level) { if (level != 2) qt_fe_ignore_changed(); }
void fe_lastlog(session *sess, session *lastlog, char *search, gtk_xtext_search_flags flags)
{ qt_fe_lastlog(sess, lastlog, search, flags); }
void fe_set_lag(server *serv, long lag) { (void)serv; (void)lag; qt_fe_connection_changed(); }
void fe_set_throttle(server *serv) { (void)serv; qt_fe_connection_changed(); }
void fe_set_away(server *serv) { (void)serv; qt_fe_connection_changed(); }
void fe_serverlist_open(session *sess) { (void)sess; qt_fe_network_manager(); }
void fe_get_bool(char *title, char *prompt, void *callback, void *userdata)
{ qt_fe_prompt_bool(title, prompt, (void (*)(int, void *))callback, userdata); }
void fe_get_str(char *prompt, char *def, void *callback, void *userdata)
{ qt_fe_prompt_string(prompt, def, (void (*)(int, char *, void *))callback, userdata); }
void fe_get_int(char *prompt, int def, void *callback, void *userdata)
{ qt_fe_prompt_integer(prompt, def, (void (*)(int, int, void *))callback, userdata); }
void fe_get_file(const char *title, char *initial,
    void (*callback)(void *, char *), void *userdata, int flags)
{
  qt_fe_prompt_file(title, initial, flags, callback, userdata);
}
void fe_confirm(const char *message, void (*yesproc)(void *),
                void (*noproc)(void *), void *userdata)
{
  if (is_dcc(userdata)) {
    qt_fe_transfer_offer(qt_core_transfer_track(userdata, FALSE), message);
    return;
  }
  if (qt_fe_confirm(message)) { if (yesproc) yesproc(userdata); }
  else if (noproc) noproc(userdata);
}
void fe_menu_del(menu_entry *entry) { qt_core_menu_removed(entry); qt_fe_menus_changed(); }
char *fe_menu_add(menu_entry *entry) { char *plain = qt_core_menu_added(entry); qt_fe_menus_changed(); return plain; }
void fe_menu_update(menu_entry *entry) { (void)entry; qt_fe_menus_changed(); }
void fe_server_event(server *serv, int type, int arg)
{ (void)serv; (void)type; (void)arg; qt_fe_connection_changed(); }
void fe_tray_set_flash(const char *a, const char *b, int timeout)
{ qt_fe_tray_flash(a, b, timeout); }
void fe_tray_set_file(const char *file) { qt_fe_tray_file(file); }
void fe_tray_set_icon(feicon icon) { qt_fe_tray_state(icon); }
void fe_tray_set_tooltip(const char *text) { qt_fe_tray_tooltip(text); }
void fe_open_chan_list(server *serv, char *filter, int refresh)
{ qt_fe_channel_browser(serv, filter, refresh); }
const char *fe_get_default_font(void) { return "Monospace 10"; }

/* ZoiteChat core extensions beyond the original HexChat frontend API. */
fe_preferences_save_result fe_preferences_persistence_save_all(void)
{
  fe_preferences_save_result result = {0};
  result.success = save_config() != 0;
  result.config_failed = !result.success;
  return result;
}
void fe_print_text_with_message(session *sess, char *text, time_t stamp,
                                gboolean no_activity, const struct reply_item *message)
{
  (void)message;
  fe_print_text(sess, text, stamp, no_activity);
}
void fe_set_typing(session *sess, const char *nick, const char *state)
{
  (void)sess; (void)nick; (void)state;
}
void fe_get_password(char *prompt, void *callback, void *userdata)
{
  qt_fe_prompt_password(prompt, (void (*)(int, char *, void *))callback, userdata);
}
#ifdef __linux__
void fe_root_warning(void)
{
  fputs("ZoiteChat refuses to run as root on Linux.\n"
        "Run it as a normal user, or pass --run-as-root to override this protection.\n", stderr);
}
#endif
