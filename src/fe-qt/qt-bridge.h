/*
 * ZoiteChat Qt frontend - C/Qt bridge.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * No Qt types escape this interface: the existing IRC engine remains C.
 */
#ifndef ZOITECHAT_QT_BRIDGE_H
#define ZOITECHAT_QT_BRIDGE_H
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
void qt_fe_initialize(int *argc, char **argv);
int qt_fe_run(void);
void qt_fe_quit(void);
void qt_fe_connection_changed(void);
void qt_fe_new_session(void *session, const char *name, int focus);
void qt_fe_remove_session(void *session);
void qt_fe_rename_session(void *session, const char *name);
void qt_fe_select_session(void *session);
void qt_fe_print(void *session, const char *text, time_t stamp);
void qt_fe_clear(void *session);
void qt_fe_clear_lines(void *session, int lines);
void qt_fe_gui_control(void *session, int action, int argument);
int qt_fe_window_status(void);
int qt_fe_session_window_status(void *session);
void qt_fe_lastlog(void *source, void *destination, const char *search, int flags);
void qt_fe_topic(void *session, const char *topic);
void qt_fe_user_add(void *session, const char *nick, const char *prefix, int select);
void qt_fe_user_details(void *session, const char *nick, const char *host, const char *real_name,
                        const char *account, const char *server, int away, int me, int rank);
void qt_fe_message_alert(void *session, const char *text, int mention, int private_message);
void qt_fe_attention(void *session);
void qt_fe_beep(void);
void qt_fe_tray_state(int state);
void qt_fe_tray_file(const char *file);
void qt_fe_tray_flash(const char *first, const char *second, int interval);
void qt_fe_tray_tooltip(const char *text);
int qt_fe_user_selected(void *session, const char *nick);
void qt_fe_user_select(void *session, const char *nick, int clear, int scroll);
void qt_fe_user_remove(void *session, const char *nick);
void qt_fe_users_clear(void *session);
void qt_fe_activity(void *session, int level);
void qt_fe_message(const char *text, int warning);
void qt_fe_set_input(void *session, const char *text);
const char *qt_fe_get_input(void *session);
int qt_fe_get_cursor(void *session);
void qt_fe_set_cursor(void *session, int relative, int position);
void qt_fe_open_url(const char *url);
int qt_fe_confirm(const char *message);
const char *qt_fe_pick_file(const char *title, const char *initial, int flags);
int qt_fe_is_active(void);
void qt_fe_network_manager(void);
void qt_fe_startup_state(int level);

/* Implemented on the C side; called by Qt controls. */
void qt_core_activate(void *session);
void *qt_core_next_activity(void);
int qt_core_keypress(void *session, unsigned state, unsigned keyval, unsigned unicode);
int qt_fe_session_attached(void *session);
void qt_core_submit(void *session, const char *line);
void qt_core_submit_command(void *session, const char *line);
void qt_core_submit_literal(void *session, const char *line);
void qt_core_quit(void);
/* Snapshot strings are borrowed; consume them before returning to the event loop. */
typedef struct {
    int channel;
    int server;
    int connected;
    int connecting;
    int away;
    const char *nick;
    const char *host;
    int query;
    int reconnect_wait;
    int registering;
    int lag_ms;
    int lag_pending;
    int queued_bytes;
    const char *nick_prefixes;
    int channel_session; /* Includes a channel window that has not joined yet. */
} QtSessionInfo;
int qt_core_session_info(void *session, QtSessionInfo *info);
void qt_core_set_topic(void *session, const char *topic);
const char *qt_core_config_dir(void);
int qt_core_nick_color(const char *nick);
#ifdef __cplusplus
}
#endif
#endif
