/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Loaded only by irc-runtime-test.py, using an isolated configuration. */
#include "zoitechat.h"
#include "zoitechat-plugin.h"
#include "qt-bridge.h"
#include "qt-network.h"
#include "moderation-bridge.h"
#include "userlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static zoitechat_plugin *plugin;
static void *root, *channel;
static int stage, registrations, denied, mode_retried, users_done, connection_failed, saw_reconnect_wait;
static const char topic[] = "  \00304red topic <b>literal</b>  ";
static void report(const char *status)
{
    const char *path = getenv("QT_IRC_REPORT");
    char temporary[4096];
    snprintf(temporary, sizeof(temporary), "%s.new", path);
    FILE *out = fopen(temporary, "w");
    if (out) {
        int written = fprintf(out, "%s stage=%d registrations=%d connection_failed=%d\n", status, stage, registrations, connection_failed);
        int closed = fclose(out);
        if (written > 0 && !closed) rename(temporary, path);
    }
}
static int registered(char *word[], char *word_eol[], void *unused)
{
    (void)word; (void)word_eol; (void)unused;
    ++registrations;
    return ZOITECHAT_EAT_NONE;
}
static int denied_mode(char *word[], char *word_eol[], void *unused)
{
    (void)word; (void)word_eol; (void)unused;
    denied = 1;
    return ZOITECHAT_EAT_NONE;
}
static int notice(char *word[], char *word_eol[], void *unused)
{
    (void)word; (void)unused;
    if (word_eol[4] && !strcmp(word_eol[4], ":users-done")) users_done = 1;
    return ZOITECHAT_EAT_NONE;
}
static int failed_connection(char *word[], void *unused)
{
    (void)word; (void)unused;
    connection_failed = 1;
    report("WAIT connection failure observed");
    return ZOITECHAT_EAT_NONE;
}
static int tick(void *unused)
{
    QtSessionInfo info;
    QtChannelState modes;
    (void)unused;
    const char *recovery = getenv("QT_IRC_RECOVERY");
    if (root && qt_core_session_info(root, &info) && info.reconnect_wait) saw_reconnect_wait = 1;
    if (!stage) {
        root = plugin->zoitechat_get_context(plugin);
        if (!root) return 1;
        const char *tls = getenv("QT_IRC_TLS");
        if (tls && *tls) {
            char servers[128], error[512]; void *identity = NULL;
            snprintf(servers, sizeof(servers), "127.0.0.1/%s", getenv("QT_IRC_PORT"));
            QtNetwork net = {0};
            net.name = "Qt runtime TLS"; net.servers = servers;
            net.nick = "runtime-nick"; net.user = "runtime-user"; net.real = "Runtime Test";
            net.password = "runtime-password";
            net.login_type = !strcmp(tls, "external") ? QT_NET_LOGIN_SASL_EXTERNAL : QT_NET_LOGIN_SASL;
            if (!strcmp(tls, "scram-1")) net.login_type = QT_NET_LOGIN_SCRAM_SHA1;
            else if (!strcmp(tls, "scram-512")) net.login_type = QT_NET_LOGIN_SCRAM_SHA512;
            else if (!strncmp(tls, "scram-", 6)) net.login_type = QT_NET_LOGIN_SCRAM_SHA256;
            net.flags = QT_NET_TLS | (!strcmp(tls, "allow") ? QT_NET_ALLOW_INVALID_CERT : 0);
            if (!qt_net_store(&net, &identity, error, sizeof(error)) || !qt_net_connect(identity)) { report("FAIL saved TLS network"); return 0; }
            root = plugin->zoitechat_find_context(plugin, "Qt runtime TLS", NULL);
            if (!root) { report("FAIL saved TLS context"); return 0; }
            stage = (!strcmp(tls, "reject") || !strcmp(tls, "mismatch")) ? 10 : 1;
        } else {
            char command[128];
            snprintf(command, sizeof(command), "server -insecure 127.0.0.1 %s", getenv("QT_IRC_PORT"));
            qt_core_submit_command(root, command); stage = 1;
        }
    } else if (stage == 1 || stage == 7) {
        if (!qt_core_session_info(root, &info) || !info.connected || info.registering || registrations < (stage == 1 ? 1 : 2)) return 1;
        qt_core_submit_command(root, "join #port");
        ++stage;
    } else if (stage == 2 || stage == 8) {
        channel = plugin->zoitechat_find_context(plugin, NULL, "#port");
        if (!channel || !qt_core_session_info(channel, &info) || !info.channel || !qt_core_channel_state(channel, &modes)) return 1;
        struct User *peer = userlist_find(channel, "peer");
        if (!peer || !peer->op || !strchr(modes.modes, 'n') || !strchr(modes.modes, 't')) return 1;
        if (stage == 8 || (recovery && !strcmp(recovery, "refused"))) {
            if (stage != 8 && (!connection_failed || !saw_reconnect_wait)) { report("FAIL initial failure recovery not observed"); return 0; }
            qt_core_submit_command(root, "discon"); stage = 9;
        } else if (recovery && *recovery) {
            qt_fe_set_input(channel, "draft survives reconnect");
            qt_core_submit_literal(channel, "drop connection now"); stage = 11;
        } else {
            if (!qt_core_channel_mode(channel, "#port", 'm', 1, "")) { report("FAIL mode request"); return 0; }
            stage = 3;
        }
    } else if (stage == 3) {
        if (!qt_core_channel_state(channel, &modes)) return 1;
        if (!mode_retried) {
            if (!denied) return 1;
            if (strchr(modes.modes, 'm')) { report("FAIL rejected mode applied optimistically"); return 0; }
            if (!qt_core_channel_mode(channel, "#port", 'm', 1, "")) { report("FAIL mode retry"); return 0; }
            mode_retried = 1;
            return 1;
        }
        if (!strchr(modes.modes, 'm')) return 1;
        qt_core_set_topic(channel, topic); stage = 4;
    } else if (stage == 4) {
        if (!qt_core_session_info(channel, &info) || !info.channel || !((session *)channel)->topic || strcmp(((session *)channel)->topic, "  red topic <b>literal</b>  ")) return 1;
        qt_core_submit_literal(channel, "message through Qt adapter");
        stage = 5;
    } else if (stage == 5) {
        if (!users_done) return 1;
        struct User *peer = userlist_find(channel, "peer-renamed");
        if (!peer || !peer->op || userlist_find(channel, "peer") || userlist_find(channel, "temporary") || userlist_find(channel, "parted")) {
            report("FAIL JOIN/NICK/PART/QUIT membership"); return 0;
        }
        qt_core_submit_command(channel, "part #port runtime-part"); stage = 6;
    } else if (stage == 6) {
        if (!qt_core_session_info(channel, &info) || info.channel) return 1;
        if (qt_core_channel_mode(channel, "#port", 'm', 1, "")) { report("FAIL parted channel accepted a mode request"); return 0; }
        qt_core_submit_command(root, "reconnect"); stage = 7;
    } else if (stage == 11) {
        if (!saw_reconnect_wait) return 1;
        if (!qt_core_session_info(channel, &info)) { report("FAIL lost channel during reconnect"); return 0; }
        if (registrations == 1) {
            if (info.channel || userlist_find(channel, "peer") || qt_core_channel_mode(channel, "#port", 'm', 1, "")) { report("FAIL stale membership during reconnect"); return 0; }
            if (recovery && !strcmp(recovery, "cancel")) {
                qt_core_submit_command(root, "discon"); stage = 9;
            }
            return 1;
        }
        if (!qt_core_session_info(root, &info) || !info.connected || info.registering) return 1;
        stage = 12;
    } else if (stage == 12) {
        void *rejoined = plugin->zoitechat_find_context(plugin, NULL, "#port");
        if (!rejoined || !qt_core_session_info(rejoined, &info) || !info.channel) return 1;
        if (rejoined != channel || strcmp(qt_fe_get_input(channel), "draft survives reconnect") || !userlist_find(channel, "peer")) { report("FAIL rejoin did not preserve conversation/draft"); return 0; }
        qt_core_submit_command(root, "discon"); stage = 9;
    } else if (stage == 10) {
        if (registrations) { report("FAIL invalid TLS registered"); return 0; }
        if (!connection_failed || !qt_core_session_info(root, &info) || info.connected || info.connecting) return 1;
        report("PASS invalid TLS rejected"); return 0;
    } else if (stage == 9) {
        if (!qt_core_session_info(root, &info) || info.connected || info.connecting || info.reconnect_wait) return 1;
        if (!qt_core_session_info(channel, &info) || info.channel) { report("FAIL disconnected channel still joined"); return 0; }
        report("PASS"); return 0;
    }
    report("WAIT");
    return 1;
}
int zoitechat_plugin_init(zoitechat_plugin *handle, char **name, char **description, char **version, char *argument)
{
    (void)argument;
    plugin = handle; *name = "Qt IRC runtime probe"; *description = "Isolated protocol verification"; *version = "1";
    handle->zoitechat_hook_server(handle, "001", ZOITECHAT_PRI_NORM, registered, NULL);
    handle->zoitechat_hook_server(handle, "482", ZOITECHAT_PRI_NORM, denied_mode, NULL);
    handle->zoitechat_hook_server(handle, "NOTICE", ZOITECHAT_PRI_NORM, notice, NULL);
    handle->zoitechat_hook_print(handle, "Connection Failed", ZOITECHAT_PRI_NORM, failed_connection, NULL);
    handle->zoitechat_hook_timer(handle, 25, tick, NULL);
    return 1;
}
