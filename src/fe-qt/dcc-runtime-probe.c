/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Loaded only by dcc-runtime-test.py in an isolated configuration. */
#include "zoitechat.h"
#include "dcc.h"
#include "zoitechat-plugin.h"
#include "qt-bridge.h"
#include "transfers-bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static zoitechat_plugin *plugin;
static void *context;
static unsigned completed;
static int started, connected, chat_sent[3], failed;
static uint64_t remove_ids[3];
static char failure[1024];
static void trace(const char *text) { FILE *out = fopen(getenv("QT_DCC_TRACE"), "a"); if (out) { fprintf(out, "%s\n", text); fclose(out); } }
static const char *base(const char *path) { const char *slash = path ? strrchr(path, '/') : NULL; return slash ? slash + 1 : path ? path : ""; }
static void report(const char *status)
{
    char temporary[4096]; const char *path = getenv("QT_DCC_REPORT");
    snprintf(temporary, sizeof(temporary), "%s.new", path);
    FILE *out = fopen(temporary, "w");
    if (out) { int written = fprintf(out, "%s mask=%u %s\n", status, completed, failure); int closed = fclose(out); if (written > 0 && closed == 0) rename(temporary, path); }
}
static void row(const struct QtTransfer *transfer, void *unused)
{
    (void)unused; const char *name = base(transfer->file); char error[1024] = {0};
    if (transfer->state == STAT_FAILED) { failed = 1; snprintf(failure, sizeof(failure), "Transfer failed: %s", name); return; }
    if (transfer->type == TYPE_RECV && !strcmp(name, "decline.bin")) {
        if (transfer->state == STAT_QUEUED && !qt_core_transfer_action(transfer->id, 4, "", error, sizeof(error))) { failed = 1; snprintf(failure, sizeof(failure), "Decline: %s", error); }
        if (transfer->state == STAT_ABORTED) remove_ids[0] = transfer->id;
        return;
    }
    if (transfer->type == TYPE_SEND && !strcmp(name, "outgoing-abort.bin")) {
        if (transfer->state == STAT_QUEUED && !qt_core_transfer_action(transfer->id, 2, "", error, sizeof(error))) { failed = 1; snprintf(failure, sizeof(failure), "Abort: %s", error); }
        if (transfer->state == STAT_ABORTED) remove_ids[1] = transfer->id;
        return;
    }
    if (transfer->type == TYPE_RECV && !strcmp(name, "active-abort.bin")) {
        if (transfer->state == STAT_QUEUED && !qt_core_transfer_action(transfer->id, 0, "", error, sizeof(error))) failed = 1;
        if (transfer->state == STAT_ACTIVE && transfer->position >= 1024 && !qt_core_transfer_action(transfer->id, 2, "", error, sizeof(error))) failed = 1;
        if (transfer->state == STAT_ABORTED) remove_ids[2] = transfer->id;
        if (failed) snprintf(failure, sizeof(failure), "Active abort: %s", error);
        return;
    }
    if (transfer->type == TYPE_RECV && transfer->state == STAT_QUEUED && !transfer->resume_pending) {
        int resume = !strcmp(name, "resume.bin");
        const char *destination = !strcmp(name, "receive file.bin") ? getenv("QT_DCC_RECEIVE_DEST") : "";
        if ((resume && !transfer->resumable) || !qt_core_transfer_action(transfer->id, resume ? 1 : 0, destination, error, sizeof(error))) {
            failed = 1; snprintf(failure, sizeof(failure), "Accept/resume %s: %s", name, error);
        }
    }
    if (transfer->state == STAT_DONE && transfer->position == transfer->size) {
        if (transfer->type == TYPE_SEND && !strcmp(name, "outgoing.bin")) completed |= 1;
        if (transfer->type == TYPE_SEND && !strcmp(name, "outgoing-passive.bin")) completed |= 2;
        if (transfer->type == TYPE_SEND && !strcmp(name, "outgoing-resume.bin")) completed |= 64;
        if (transfer->type == TYPE_RECV && !strcmp(name, "receive file.bin")) completed |= 4;
        if (transfer->type == TYPE_RECV && !strcmp(name, "resume.bin")) completed |= 8;
        if (transfer->type == TYPE_RECV && !strcmp(name, "passive.bin")) completed |= 16;
    }
    if ((transfer->type == TYPE_CHATSEND || transfer->type == TYPE_CHATRECV) && transfer->state == STAT_ACTIVE) {
        const int kind = !strcmp(transfer->nick, "peer-passive") ? 1 : !strcmp(transfer->nick, "peer-incoming") ? 2 : 0;
        const char *message = kind == 1 ? "from-client-passive" : kind == 2 ? "from-client-incoming" : "from-client";
        const char *reply = kind == 1 ? "from-peer-passive" : kind == 2 ? "from-peer-incoming" : "from-peer";
        if (!chat_sent[kind]) {
            if (!qt_core_transfer_action(transfer->id, 5, "", error, sizeof(error))) { failed = 1; snprintf(failure, sizeof(failure), "Open chat: %s", error); return; }
            void *query = plugin->zoitechat_find_context(plugin, NULL, transfer->nick);
            if (!query) { failed = 1; snprintf(failure, sizeof(failure), "No DCC query context"); return; }
            chat_sent[kind] = 1; qt_core_submit_literal(query, message);
        }
        if (transfer->position >= strlen(reply) && transfer->size >= strlen(message)) completed |= kind == 1 ? 128 : kind == 2 ? 256 : 32;
    }
}
static int tick(void *unused)
{
    (void)unused;
    if (!started) {
        context = plugin->zoitechat_get_context(plugin);
        if (!context) return 1;
        char command[128]; snprintf(command, sizeof(command), "server -insecure 127.0.0.1 %s", getenv("QT_DCC_PORT"));
        trace(command); qt_core_submit_command(context, command); started = 1;
    }
    if (!connected) {
        QtSessionInfo info;
        if (!qt_core_session_info(context, &info) || !info.connected || info.registering) return 1;
        trace("registered"); connected = 1; char error[1024] = {0};
        if (!qt_core_transfer_send(context, "peer", getenv("QT_DCC_SEND"), 0, 0, error, sizeof(error)) ||
            !qt_core_transfer_send(context, "peer", getenv("QT_DCC_PASSIVE_SEND"), 0, 1, error, sizeof(error)) ||
            !qt_core_transfer_send(context, "peer", getenv("QT_DCC_RESUME_SEND"), 0, 0, error, sizeof(error)) ||
            !qt_core_transfer_send(context, "peer", getenv("QT_DCC_ABORT_SEND"), 0, 0, error, sizeof(error)) ||
            !qt_core_direct_chat_start(context, "peer", 0, error, sizeof(error)) ||
            !qt_core_direct_chat_start(context, "peer-passive", 1, error, sizeof(error))) {
            snprintf(failure, sizeof(failure), "Start: %s", error); report("FAIL"); return 0;
        }
    }
    qt_core_transfers_read(row, NULL);
    for (int i = 0; i < 3; ++i) if (remove_ids[i]) {
        char error[1024] = {0}; uint64_t id = remove_ids[i]; remove_ids[i] = 0;
        if (!qt_core_transfer_action(id, 3, "", error, sizeof(error)) || qt_core_transfer_action(id, 0, "", error, sizeof(error))) { failed = 1; snprintf(failure, sizeof(failure), "Remove/stale ID: %s", error); }
        else completed |= i == 0 ? 512 : i == 1 ? 1024 : 2048;
    }
    if (failed) { report("FAIL"); return 0; }
    if (completed == 4095) { report("PASS"); return 0; }
    return 1;
}
int zoitechat_plugin_init(zoitechat_plugin *handle, char **name, char **description, char **version, char *argument)
{
    (void)argument; plugin = handle; *name = "Qt DCC runtime probe"; *description = "Isolated loopback verification"; *version = "1";
    trace("probe initialized"); handle->zoitechat_hook_timer(handle, 25, tick, NULL); return 1;
}
