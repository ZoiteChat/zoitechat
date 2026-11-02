/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "alerts-bridge.h"
#include <string.h>
static int enabled(unsigned int global, guint8 local) { return local == SET_DEFAULT ? !!global : local == SET_ON; }
int qt_core_alert_policy(void *ptr, int mention, int private_message, struct QtAlertPolicy *result)
{
    session *sess = ptr;
    if (!result || !sess || !is_session(sess) || !sess->server) return 0;
    memset(result, 0, sizeof(*result));
    result->omit_focused = prefs.hex_gui_focus_omitalerts; result->quiet = prefs.hex_gui_tray_quiet;
    if (prefs.hex_away_omit_alerts && sess->server->is_away) return 1;
    unsigned int notify = mention ? prefs.hex_input_balloon_hilight : private_message ? prefs.hex_input_balloon_priv : prefs.hex_input_balloon_chans;
    unsigned int tray = mention ? prefs.hex_input_tray_hilight : private_message ? prefs.hex_input_tray_priv : prefs.hex_input_tray_chans;
    unsigned int flash = mention ? prefs.hex_input_flash_hilight : private_message ? prefs.hex_input_flash_priv : prefs.hex_input_flash_chans;
    result->notify = enabled(notify, sess->alert_balloon); result->tray = enabled(tray, sess->alert_tray); result->flash = enabled(flash, sess->alert_taskbar);
    result->force_notify = sess->alert_balloon == SET_ON; return 1;
}
