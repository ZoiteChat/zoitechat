/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_FRIENDS_BRIDGE_H
#define ZOITECHAT_QT_FRIENDS_BRIDGE_H
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*QtFriendVisitor)(const char *name, const char *networks, const char *network,
                                void *server, int online, time_t seen, time_t last_on, time_t last_off, void *data);
void qt_core_friends_foreach(QtFriendVisitor visitor, void *data);
int qt_core_friend_set(const char *old_name, const char *name, const char *networks);
int qt_core_friend_remove(const char *name);
int qt_core_friend_query(const char *name, void *server, const char *network);
void qt_core_new_tab(void *session, int server_tab);
void qt_fe_friends_changed(void);
void qt_fe_friend_ask(const char *name, const char *networks);
#ifdef __cplusplus
}
#endif
#endif
