/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/dcc.h"
#include "transfers-bridge.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
GSList *dcc_list, *sess_list;
static session context;
static int accepted, resumed, aborted, sent, chats, queries;
session *open_query(server *serv, char *nick, gboolean focus) { (void)serv; (void)nick; (void)focus; ++queries; return &context; }
void dcc_chat(session *sess, char *nick, int passive) { (void)sess; (void)nick; (void)passive; ++chats; }
static char destination[1024];
gboolean is_dcc(struct DCC *dcc) { return g_slist_find(dcc_list, dcc) != NULL; }
int is_session(session *sess) { return sess == &context; }
void qt_fe_transfers_changed(void) {}
void dcc_get(struct DCC *dcc) { ++accepted; dcc->dccstat = STAT_CONNECTING; }
void dcc_get_with_destfile(struct DCC *dcc, char *file) { g_strlcpy(destination, file, sizeof(destination)); dcc_get(dcc); }
int dcc_resume(struct DCC *dcc) { if (!dcc->resumable) return 0; ++resumed; dcc->resume_sent = 1; return 1; }
void dcc_abort(session *sess, struct DCC *dcc) { (void)sess; ++aborted; dcc->dccstat = STAT_ABORTED; }
void dcc_send(session *sess, char *nick, char *file, gint64 maxcps, int passive) { (void)sess; (void)nick; (void)file; (void)maxcps; (void)passive; ++sent; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    server serv = {0}; context.server = &serv; serv.connected = 1; sess_list = g_slist_append(NULL, &context);
    struct DCC dcc = {0}; dcc.serv = &serv; dcc.type = TYPE_RECV; dcc.dccstat = STAT_QUEUED;
    dcc_list = g_slist_append(NULL, &dcc); char error[1024];
    uint64_t first = qt_core_transfer_track(&dcc, 0); CHECK(first && first == qt_core_transfer_track(&dcc, 0));
    CHECK(!qt_core_transfer_action(first, 1, "", error, sizeof(error)) && !resumed);
    dcc.resumable = 20; CHECK(qt_core_transfer_action(first, 1, "", error, sizeof(error)) && resumed == 1);
    CHECK(!qt_core_transfer_action(first, 0, "", error, sizeof(error)) && !accepted);
    CHECK(!qt_core_transfer_action(first, 4, "", error, sizeof(error)) && !aborted);
    dcc.resume_sent = 0;
    CHECK(qt_core_transfer_action(first, 0, "/tmp/chosen path", error, sizeof(error)) && accepted == 1 && !strcmp(destination, "/tmp/chosen path"));
    CHECK(!qt_core_transfer_action(first, 0, "", error, sizeof(error)));
    CHECK(qt_core_transfer_action(first, 2, "", error, sizeof(error)) && aborted == 1);
    CHECK(!qt_core_transfer_action(first, 2, "", error, sizeof(error)));
    CHECK(qt_core_transfer_action(first, 3, "", error, sizeof(error)) && aborted == 2);
    qt_core_transfer_track(&dcc, 1); dcc.dccstat = STAT_QUEUED;
    uint64_t second = qt_core_transfer_track(&dcc, 0); CHECK(second != first);
    CHECK(!qt_core_transfer_action(first, 0, "", error, sizeof(error)));
    dcc.type = TYPE_SEND; CHECK(!qt_core_transfer_action(second, 0, "", error, sizeof(error)));
    dcc.type = TYPE_RECV; CHECK(!qt_core_transfer_action(second, 0, "bad\npath", error, sizeof(error)));
    dcc.type = TYPE_CHATRECV; dcc.dccstat = STAT_QUEUED;
    CHECK(!qt_core_transfer_action(second, 5, "", error, sizeof(error)) && !queries);
    CHECK(qt_core_transfer_action(second, 0, "", error, sizeof(error)) && accepted == 2);
    dcc.dccstat = STAT_ACTIVE;
    CHECK(qt_core_transfer_action(second, 5, "", error, sizeof(error)) && queries == 1);
    CHECK(!qt_core_transfer_action(second, 1, "", error, sizeof(error)));
    CHECK(qt_core_direct_chat_start(&context, "Alice", 1, error, sizeof(error)) && chats == 1);
    CHECK(!qt_core_direct_chat_start(&context, "Alice Bob", 0, error, sizeof(error)) && chats == 1);
    char *path; int fd = g_file_open_tmp("qt-transfer-XXXXXX", &path, NULL); CHECK(fd >= 0); close(fd);
    CHECK(qt_core_transfer_send(&context, "Alice", path, 0, 0, error, sizeof(error)) && sent == 1);
    CHECK(!qt_core_transfer_send(&context, "Alice Bob", path, 0, 0, error, sizeof(error)) && sent == 1);
    serv.connected = 0; CHECK(!qt_core_transfer_send(&context, "Alice", path, 0, 0, error, sizeof(error)));
    g_remove(path); g_free(path); g_slist_free(dcc_list); dcc_list = NULL; qt_core_transfer_track(&dcc, 1);
    CHECK(!qt_core_transfer_action(second, 0, "", error, sizeof(error))); g_slist_free(sess_list); return 0;
}
