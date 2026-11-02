/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "moderation-bridge.h"
#include <stdio.h>
#include <string.h>
static session channel;
static int live = 1, sent;
static char last[512], target[512];
int is_session(session *sess) { return live && sess == &channel; }
static void send_mode(server *serv, char *destination, char *mode)
{
    (void)serv; ++sent; g_strlcpy(last, mode, sizeof(last)); g_strlcpy(target, destination, sizeof(target));
}
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #test); return 1; } } while (0)
int main(void)
{
    server serv = {0};
    QtChannelState state;
    serv.connected = TRUE; serv.p_mode = send_mode; serv.p_cmp = g_ascii_strcasecmp;
    serv.chanmodes = "beIq,k,l,imnpst";
    channel.server = &serv; channel.type = SESS_CHANNEL;
    g_strlcpy(channel.channel, "#test", sizeof(channel.channel));
    channel.current_modes = "+nt";
    CHECK(qt_core_channel_state(&channel, &state));
    CHECK(!strcmp(state.channel, "#test") && state.lists == 3 && !strcmp(state.simple, "imnpst"));
    CHECK(qt_core_channel_refresh(&channel, "#TEST") && !strcmp(last, "") && !strcmp(target, "#test"));
    CHECK(qt_core_channel_mode(&channel, "#test", 'm', 1, "") && !strcmp(last, "+m"));
    CHECK(qt_core_channel_mode(&channel, "#test", 't', 0, "") && !strcmp(last, "-t"));
    CHECK(qt_core_channel_mode(&channel, "#test", 'k', 1, "secret") && !strcmp(last, "+k secret"));
    g_strlcpy(channel.channelkey, "oldsecret", sizeof(channel.channelkey));
    CHECK(qt_core_channel_mode(&channel, "#test", 'k', 0, "") && !strcmp(last, "-k oldsecret"));
    CHECK(qt_core_channel_mode(&channel, "#test", 'l', 1, "42") && !strcmp(last, "+l 42"));
    CHECK(qt_core_channel_mode(&channel, "#test", 'l', 0, "") && !strcmp(last, "-l"));
    CHECK(qt_core_ban_request(&channel, "#test", 'b') && !strcmp(last, "+b"));
    CHECK(qt_core_channel_mode(&channel, "#test", 'e', 1, "nick!*@*") && !strcmp(last, "+e nick!*@*"));
    CHECK(qt_core_channel_mode(&channel, "#test", 'b', 0, "nick!*@*") && !strcmp(last, "-b nick!*@*"));
    int before = sent;
    CHECK(!qt_core_channel_mode(&channel, "#other", 'm', 1, ""));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'z', 1, ""));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'k', 1, ""));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'k', 1, "bad key"));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'l', 1, "0"));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'l', 1, "42x"));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'l', 1, "999999999999999999999999"));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'b', 1, "mask\r\nOPER secret"));
    CHECK(!qt_core_ban_request(&channel, "#test", 'q'));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'b', 1, ""));
    serv.chanmodes = "b,k,l,nt";
    CHECK(!qt_core_channel_mode(&channel, "#test", 'e', 1, "nick!*@*"));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'm', 1, ""));
    serv.connected = FALSE;
    CHECK(!qt_core_channel_state(&channel, &state));
    CHECK(!qt_core_channel_mode(&channel, "#test", 'n', 1, ""));
    serv.connected = TRUE; channel.channel[0] = 0;
    CHECK(!qt_core_channel_refresh(&channel, "#test"));
    live = 0; CHECK(!qt_core_channel_state(&channel, &state));
    CHECK(sent == before);
    return 0;
}
