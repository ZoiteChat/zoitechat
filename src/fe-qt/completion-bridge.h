/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_COMPLETION_BRIDGE_H
#define ZOITECHAT_QT_COMPLETION_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
void qt_core_completion_read(void *session, int kind, /* 0 channels, 1 commands, 2 ordered nicks */ void (*visit)(const char *, void *), void *userdata);
const char *qt_core_command_prefix(void);
int qt_core_completion_compare(void *session, const char *left, const char *right);
#ifdef __cplusplus
}
#endif
#endif
