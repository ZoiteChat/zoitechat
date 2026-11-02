/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "alerts-bridge.h"
#include <stdio.h>
#include <string.h>
struct zoitechatprefs prefs;
static session context;
static server connection;
int is_session(session *sess) { return sess == &context; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void) {
    struct QtAlertPolicy p; context.server = &connection;
    context.alert_balloon = context.alert_tray = context.alert_taskbar = SET_DEFAULT;
    prefs.hex_input_balloon_hilight = 1; prefs.hex_input_tray_priv = 1; prefs.hex_input_flash_chans = 1;
    CHECK(!qt_core_alert_policy(NULL, 1, 0, &p));
    CHECK(qt_core_alert_policy(&context, 1, 0, &p) && p.notify && !p.tray && !p.flash);
    CHECK(qt_core_alert_policy(&context, 0, 1, &p) && !p.notify && p.tray && !p.flash);
    CHECK(qt_core_alert_policy(&context, 0, 0, &p) && !p.notify && !p.tray && p.flash);
    context.alert_balloon = SET_OFF; context.alert_tray = SET_ON;
    CHECK(qt_core_alert_policy(&context, 1, 0, &p) && !p.notify && p.tray);
    context.alert_balloon = SET_ON; CHECK(qt_core_alert_policy(&context, 0, 0, &p) && p.notify && p.force_notify);
    prefs.hex_gui_focus_omitalerts = 1; prefs.hex_gui_tray_quiet = 1;
    CHECK(qt_core_alert_policy(&context, 0, 0, &p) && p.omit_focused && p.quiet);
    connection.is_away = 1; prefs.hex_away_omit_alerts = 1;
    CHECK(qt_core_alert_policy(&context, 1, 1, &p) && !p.notify && !p.tray && !p.flash && !p.force_notify);
    return 0;
}
