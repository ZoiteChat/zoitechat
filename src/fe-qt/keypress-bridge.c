/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/plugin.h"
#include "qt-bridge.h"
int qt_core_keypress(void *ptr, unsigned state, unsigned keyval, unsigned unicode)
{
    session *sess = ptr;
    if (!sess || !is_session(sess)) return 0;
    return plugin_emit_keypress(sess, state, keyval, unicode);
}
