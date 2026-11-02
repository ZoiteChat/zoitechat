/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_TOOLS_BRIDGE_H
#define ZOITECHAT_QT_TOOLS_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
void *qt_core_session_server(void *session);
int qt_core_server_info(void *server, const char **name, int *connected);
int qt_core_request_channels(void *server, const char *filter);
int qt_core_join_channel(void *server, const char *channel);
void qt_core_ignore_foreach(void (*visitor)(const char *, int, void *), void *data);
int qt_core_ignore_set(const char *old_mask, const char *mask, int flags);
int qt_core_ignore_remove(const char *mask);
void qt_core_urls_clear(void);
void qt_fe_url_capture(const char *url);
void qt_fe_channel_browser(void *server, const char *filter, int refresh);
int qt_fe_channel_browser_open(void *server);
void qt_fe_channel_row(void *server, const char *channel, const char *users, const char *topic);
void qt_fe_channel_end(void *server);
void qt_fe_rawlog(void *server, const char *text, int length, int outbound);
void qt_fe_tools_server_closed(void *server);
void qt_fe_ignore_changed(void);
void qt_fe_prompt_bool(const char *title, const char *prompt, void (*callback)(int, void *), void *data);
void qt_fe_prompt_password(const char *prompt, void (*callback)(int, char *, void *), void *data);
void qt_fe_prompt_string(const char *prompt, const char *initial,
                         void (*callback)(int, char *, void *), void *data);
void qt_fe_prompt_integer(const char *prompt, int initial,
                          void (*callback)(int, int, void *), void *data);
void qt_fe_prompt_file(const char *title, const char *initial, int flags,
                       void (*callback)(void *, char *), void *data);
#ifdef __cplusplus
}
#endif
#endif
