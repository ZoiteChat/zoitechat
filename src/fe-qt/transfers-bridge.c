/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/dcc.h"
#include "../common/outbound.h"
#include "transfers-bridge.h"
#include <string.h>
static GHashTable *identities;
static uint64_t sequence;
uint64_t qt_core_transfer_track(void *ptr, int removed)
{
    if (!identities) identities = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    if (removed) { g_hash_table_remove(identities, ptr); return 0; }
    if (!ptr || !is_dcc(ptr)) return 0;
    uint64_t *id = g_hash_table_lookup(identities, ptr);
    if (!id) { id = g_new(uint64_t, 1); *id = ++sequence; g_hash_table_insert(identities, ptr, id); }
    return *id;
}
void qt_core_transfers_read(void (*visitor)(const struct QtTransfer *, void *), void *data)
{
    if (!visitor) return;
    for (GSList *link = dcc_list; link; link = link->next) {
        struct DCC *dcc = link->data;
        struct QtTransfer row = {qt_core_transfer_track(dcc, 0), dcc->type, dcc->dccstat, dcc->nick, dcc->file, dcc->destfile,
            dcc->size, dcc->type == TYPE_SEND ? dcc->ack : dcc->pos, dcc->resumable, dcc->cps, dcc->resume_sent};
        visitor(&row, data);
    }
}
static int fail(char *error, int capacity, const char *message)
{ if (error && capacity > 0) g_strlcpy(error, message, capacity); return 0; }
int qt_core_transfer_action(uint64_t id, int action, const char *destination, char *error, int capacity)
{
    struct DCC *dcc = NULL;
    for (GSList *link = dcc_list; link; link = link->next) if (qt_core_transfer_track(link->data, 0) == id) { dcc = link->data; break; }
    if (!dcc) return fail(error, capacity, "This transfer no longer exists.");
    session *context = NULL;
    for (GSList *link = sess_list; link; link = link->next) if (((session *)link->data)->server == dcc->serv) { context = link->data; break; }
    if (!context) return fail(error, capacity, "The transfer's server context has closed.");
    if (action == 5) {
        if ((dcc->type != TYPE_CHATRECV && dcc->type != TYPE_CHATSEND) || dcc->dccstat != STAT_ACTIVE) return fail(error, capacity, "Only an active direct chat can open a conversation.");
        open_query(dcc->serv, dcc->nick, TRUE);
    } else if (action == 2 || action == 3 || action == 4) {
        const int terminal = dcc->dccstat == STAT_DONE || dcc->dccstat == STAT_ABORTED || dcc->dccstat == STAT_FAILED;
        if (action == 3 && !terminal) return fail(error, capacity, "Only completed, aborted or failed entries can be removed.");
        if (action == 2 && terminal) return fail(error, capacity, "This transfer has already ended.");
        if (action == 4 && (dcc->dccstat != STAT_QUEUED || dcc->resume_sent)) return fail(error, capacity, "This offer is no longer queued.");
        dcc_abort(context, dcc);
    } else {
        if (dcc->dccstat != STAT_QUEUED || dcc->resume_sent) return fail(error, capacity, "This offer is no longer ready to accept.");
        if (dcc->type != TYPE_RECV && dcc->type != TYPE_CHATRECV) return fail(error, capacity, "Only incoming offers can be accepted.");
        if (action == 1) {
            if (dcc->type != TYPE_RECV || !dcc_resume(dcc)) return fail(error, capacity, "The destination file cannot be resumed.");
        } else if (action == 0) {
            if (destination && *destination) {
                if (dcc->type != TYPE_RECV || strlen(destination) > 4096 || strpbrk(destination, "\r\n")) return fail(error, capacity, "Invalid destination path.");
                dcc_get_with_destfile(dcc, (char *)destination);
            } else dcc_get(dcc);
        } else return fail(error, capacity, "Invalid transfer action.");
    }
    qt_fe_transfers_changed(); return 1;
}
int qt_core_transfer_send(void *ptr, const char *nick, const char *file, int maxcps, int passive, char *error, int capacity)
{
    session *context = ptr;
    if (!context || !is_session(context) || !context->server->connected) return fail(error, capacity, "The original server is no longer connected.");
    if (!nick || !*nick || strlen(nick) >= NICKLEN || strpbrk(nick, " \t\r\n,") || !file || !*file || strlen(file) > 4096 || strpbrk(file, "\r\n")) return fail(error, capacity, "Invalid nickname or file path.");
    if (!g_file_test(file, G_FILE_TEST_IS_REGULAR)) return fail(error, capacity, "Choose a regular file to send.");
    dcc_send(context, (char *)nick, (char *)file, maxcps, !!passive); return 1;
}

int qt_core_direct_chat_start(void *ptr, const char *nick, int passive, char *error, int capacity)
{
    session *context = ptr;
    if (!context || !is_session(context) || !context->server->connected) return fail(error, capacity, "The original server is no longer connected.");
    if (!nick || !*nick || strlen(nick) >= NICKLEN || strpbrk(nick, " \t\r\n,")) return fail(error, capacity, "Invalid nickname.");
    dcc_chat(context, (char *)nick, !!passive); return 1;
}
