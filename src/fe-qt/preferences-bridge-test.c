/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "preferences-bridge.h"
#include <stdio.h>
#include <string.h>
struct zoitechatprefs prefs;
GSList *sess_list;
static int saved, logging, timers, save_ok = 1;
void zoitechat_reinit_timers(void) { ++timers; }
int save_config(void) { ++saved; return save_ok; }
void log_open_or_close(session *sess) { (void)sess; ++logging; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static struct QtPreferenceChange change(const char *key, const char *value) {
    struct QtPreferenceChange result = {0}; result.index = -1;
    for (int i = 0; i < qt_core_preference_count(); ++i) {
        struct QtPreference p; qt_core_preference_read(i, &p);
        if (!strcmp(key, p.key)) { result.index = i; strcpy(result.expected, p.value); strcpy(result.value, value); break; }
    }
    return result;
}
int main(void) {
    char error[512]; session context; sess_list = g_slist_append(NULL, &context);
    CHECK(qt_core_preference_count() >= 50);
    struct QtPreferenceChange edits[2] = {change("irc_quit_reason", "See you later"), change("irc_logging", "1")};
    CHECK(qt_core_preferences_apply(edits, 2, error, sizeof(error)));
    CHECK(!strcmp(prefs.hex_irc_quit_reason, "See you later") && prefs.hex_irc_logging && saved == 1 && logging == 1);
    CHECK(!qt_core_preferences_apply(edits, 2, error, sizeof(error)) && saved == 1);
    edits[0] = change("irc_part_reason", "good"); edits[1] = change("net_proxy_port", "65536");
    CHECK(!qt_core_preferences_apply(edits, 2, error, sizeof(error)) && !*prefs.hex_irc_part_reason);
    edits[1] = change("net_proxy_port", "1junk"); CHECK(!qt_core_preferences_apply(edits, 2, error, sizeof(error)));
    edits[1] = change("irc_part_reason", "good"); CHECK(!qt_core_preferences_apply(edits, 2, error, sizeof(error)));
    edits[1] = change("irc_quit_reason", "bad\nsetting = value"); CHECK(!qt_core_preferences_apply(edits, 2, error, sizeof(error)));
    edits[1] = change("irc_quit_reason", "\xff"); CHECK(!qt_core_preferences_apply(edits, 2, error, sizeof(error)));
    edits[0] = change("dcc_port_first", "5000"); edits[1] = change("dcc_port_last", "4000");
    CHECK(!qt_core_preferences_apply(edits, 2, error, sizeof(error)) && prefs.hex_dcc_port_first == 0);
    edits[1] = change("dcc_port_last", "5010"); CHECK(qt_core_preferences_apply(edits, 2, error, sizeof(error)));
    edits[0] = change("text_stripcolor_topic", "1");
    CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_text_stripcolor_topic && !prefs.hex_text_stripcolor_msg);
    edits[0] = change("irc_reconnect_rejoin", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_irc_reconnect_rejoin);
    edits[0] = change("irc_reconnect_rejoin", "0"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !prefs.hex_irc_reconnect_rejoin);
    save_ok = 0; edits[0] = change("irc_reconnect_rejoin", "1");
    CHECK(!qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !prefs.hex_irc_reconnect_rejoin);
    save_ok = 1;
    edits[0] = change("completion_auto", "1"); edits[1] = change("input_perc_color", "1");
    CHECK(qt_core_preferences_apply(edits, 2, error, sizeof(error)) && prefs.hex_completion_auto && prefs.hex_input_perc_color);
    edits[0] = change("input_perc_ascii", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_input_perc_ascii);
    save_ok = 0; edits[0] = change("completion_auto", "0"); edits[1] = change("input_perc_color", "0");
    CHECK(!qt_core_preferences_apply(edits, 2, error, sizeof(error)) && prefs.hex_completion_auto && prefs.hex_input_perc_color);
    save_ok = 1;
    edits[0] = change("completion_suffix", ","); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !strcmp(prefs.hex_completion_suffix, ","));
    edits[0] = change("completion_suffix", "é"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !strcmp(prefs.hex_completion_suffix, "é"));
    edits[0] = change("completion_suffix", "ab"); CHECK(!qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !strcmp(prefs.hex_completion_suffix, "é"));
    edits[0] = change("completion_suffix", "€"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !strcmp(prefs.hex_completion_suffix, "€"));
    edits[0] = change("completion_suffix", "😀"); CHECK(!qt_core_preferences_apply(edits, 1, error, sizeof(error)));
    edits[0] = change("completion_suffix", ""); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !*prefs.hex_completion_suffix);
    edits[0] = change("input_command_char", "!"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !strcmp(prefs.hex_input_command_char, "!"));
    edits[0] = change("input_command_char", "!!"); CHECK(!qt_core_preferences_apply(edits, 1, error, sizeof(error)));
    edits[0] = change("input_command_char", ""); CHECK(!qt_core_preferences_apply(edits, 1, error, sizeof(error)));
    edits[0] = change("gui_hide_menu", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_hide_menu);
    edits[0] = change("gui_topicbar", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_topicbar);
    edits[0] = change("gui_ulist_hide", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_ulist_hide);
    edits[0] = change("gui_ulist_buttons", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_ulist_buttons);
    edits[0] = change("gui_mode_buttons", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_mode_buttons);
    edits[0] = change("gui_usermenu", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_usermenu);
    edits[0] = change("gui_input_attr", "1"); CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_input_attr);
    edits[0] = change("gui_input_style", "1"); edits[1] = change("gui_ulist_style", "1");
    CHECK(qt_core_preferences_apply(edits, 2, error, sizeof(error)) && prefs.hex_gui_input_style && prefs.hex_gui_ulist_style);
    edits[0] = change("gui_input_nick", "1");
    CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_input_nick);
    prefs.hex_gui_ulist_pos = 3; prefs.hex_gui_tab_pos = 1;
    edits[0] = change("gui_tab_layout", "2"); edits[1] = change("gui_tab_pos", "5");
    CHECK(qt_core_preferences_apply(edits, 2, error, sizeof(error)) && prefs.hex_gui_tab_pos == 1);
    edits[0] = change("gui_tab_pos", "3");
    CHECK(qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_tab_pos == 4);
    save_ok = 0; edits[0] = change("gui_ulist_pos", "4");
    CHECK(!qt_core_preferences_apply(edits, 1, error, sizeof(error)) && prefs.hex_gui_tab_pos == 4 && prefs.hex_gui_ulist_pos == 3);
    save_ok = 1;
    edits[0] = change("gui_lagometer", "3"); edits[1] = change("gui_throttlemeter", "3");
    CHECK(qt_core_preferences_apply(edits, 2, error, sizeof(error)) && timers == 1 && prefs.hex_gui_lagometer == 3 && prefs.hex_gui_throttlemeter == 3);
    save_ok = 0; edits[0] = change("gui_lagometer", "0");
    CHECK(!qt_core_preferences_apply(edits, 1, error, sizeof(error)) && timers == 1 && prefs.hex_gui_lagometer == 3);
    edits[0] = change("irc_part_reason", "unsaved");
    CHECK(!qt_core_preferences_apply(edits, 1, error, sizeof(error)) && !*prefs.hex_irc_part_reason);
    CHECK(!qt_core_preferences_apply(NULL, -1, error, sizeof(error)));
    g_slist_free(sess_list); return 0;
}
