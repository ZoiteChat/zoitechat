/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/outbound.h"
#include "qt-bridge.h"
#include <string.h>
/* UI actions are commands regardless of the user's input prefix. */
void qt_core_submit_command(void *ptr, const char *line)
{
    session *sess = ptr;
    if (!sess || !is_session(sess) || !line || !*line || strpbrk(line, "\r\n")) return;
    if (*line == '/') ++line;
    if (!*line) return;
    char *command = g_strdup(line);
    qt_core_activate(sess);
    handle_command(sess, command, TRUE);
    g_free(command);
}
