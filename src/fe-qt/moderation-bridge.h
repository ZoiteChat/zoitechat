/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_MODERATION_BRIDGE_H
#define ZOITECHAT_QT_MODERATION_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char *channel;
    const char *modes;
    const char *supported;
    const char *simple;
    int lists; /* b=1, e=2, only if advertised as list modes */
    int limit;
    int key_set;
} QtChannelState;
int qt_core_channel_state(void *session, QtChannelState *state);
int qt_core_channel_refresh(void *session, const char *channel);
int qt_core_channel_mode(void *session, const char *channel, char mode, int enabled, const char *value);
int qt_core_ban_request(void *session, const char *channel, char mode);
void qt_fe_moderation_closed(void *session);
int qt_fe_ban_row(void *session, const char *mask, const char *who, const char *when, int code);
int qt_fe_ban_end(void *session, int code);
#ifdef __cplusplus
}
#endif
#endif
