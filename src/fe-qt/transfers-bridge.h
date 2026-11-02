/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_TRANSFERS_BRIDGE_H
#define ZOITECHAT_QT_TRANSFERS_BRIDGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct QtTransfer { uint64_t id; int type, state; const char *nick, *file, *destination; uint64_t size, position, resumable; int64_t speed; int resume_pending; };
void qt_core_transfers_read(void (*visitor)(const struct QtTransfer *, void *), void *data);
uint64_t qt_core_transfer_track(void *dcc, int removed);
// 0 accept, 1 resume, 2 abort, 3 remove completed, 4 decline queued offer, 5 open an active direct-chat conversation.
int qt_core_transfer_action(uint64_t id, int action, const char *destination, char *error, int capacity);
int qt_core_transfer_send(void *session, const char *nick, const char *file, int maxcps, int passive, char *error, int capacity);
int qt_core_direct_chat_start(void *session, const char *nick, int passive, char *error, int capacity);
void qt_fe_transfers_changed(void);
int qt_fe_direct_chats_open(int passive);
void qt_fe_transfer_offer(uint64_t id, const char *message);
int qt_fe_transfers_open(int passive);
void qt_fe_transfer_send_request(void *session, const char *nick, int maxcps, int passive);
#ifdef __cplusplus
}
#endif
#endif
