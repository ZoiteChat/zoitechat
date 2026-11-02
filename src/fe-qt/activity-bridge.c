/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "qt-bridge.h"
static int attached(session *sess) { return qt_fe_session_attached(sess); }
void *qt_core_next_activity(void)
{
    /* Retain the engine's five priority classes and most-recent-first order. */
    return lastact_getfirst(attached);
}
